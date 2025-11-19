#pragma once

#include <algorithm>
#include <array>
#include <cctype>
#include <cstddef>
#include <cstdio>
#include <string>
#include <string_view>
#include <utility>

#include "lvgl.h"

// Detect whether the real {fmt} library is available.
#if defined(__has_include)
#  if __has_include(<fmt/format.h>)
#    include <fmt/format.h>
#    define GUI_FORMAT_HAS_FMT 1
#  endif
#endif

#ifndef GUI_FORMAT_HAS_FMT
#  define GUI_FORMAT_HAS_FMT 0
namespace fmt {
template <typename... Args>
using format_string = const char *;
}  // namespace fmt
#endif

namespace gui {

template <std::size_t N>
using StaticBuffer = std::array<char, N>;

namespace detail {
#if GUI_FORMAT_HAS_FMT
template <typename... Args>
std::string_view to_string_view(fmt::format_string<Args...> fmt_str)
{
    const fmt::string_view sv(fmt_str);
    return std::string_view(sv.data(), sv.size());
}
#else
template <typename... Args>
std::string_view to_string_view(fmt::format_string<Args...> fmt_str)
{
    return std::string_view(fmt_str);
}
#endif

constexpr std::size_t kMaxPatternSize = 128;

template <std::size_t N>
bool append_char(StaticBuffer<N> &buffer, std::size_t &pos, char ch)
{
    if (pos + 1 >= buffer.size()) {
        return false;
    }
    buffer[pos++] = ch;
    return true;
}

template <std::size_t N>
bool append_literal(StaticBuffer<N> &buffer, std::size_t &pos, std::string_view literal)
{
    for (char ch : literal) {
        if (!append_char(buffer, pos, ch)) {
            return false;
        }
    }
    return true;
}

template <std::size_t N>
bool convert_to_printf_pattern(StaticBuffer<N> &buffer, std::size_t &length, std::string_view fmt)
{
    length = 0;
    for (std::size_t i = 0; i < fmt.size();) {
        const char ch = fmt[i];
        if (ch == '{') {
            if (i + 1 < fmt.size() && fmt[i + 1] == '{') {
                if (!append_char(buffer, length, '{')) {
                    return false;
                }
                i += 2;
                continue;
            }

            ++i;
            while (i < fmt.size() && std::isdigit(static_cast<unsigned char>(fmt[i])) && fmt[i] != '}' && fmt[i] != ':') {
                ++i;
            }

            std::string spec;
            if (i < fmt.size() && fmt[i] == ':') {
                ++i;
                while (i < fmt.size() && fmt[i] != '}') {
                    spec.push_back(fmt[i++]);
                }
            }

            if (i >= fmt.size() || fmt[i] != '}') {
                return false;
            }
            ++i;

            if (!append_char(buffer, length, '%')) {
                return false;
            }

            if (spec.empty()) {
                if (!append_char(buffer, length, 's')) {
                    return false;
                }
            } else {
                for (char sc : spec) {
                    if (sc == '%') {
                        if (!append_literal(buffer, length, "%%")) {
                            return false;
                        }
                    } else if (!append_char(buffer, length, sc)) {
                        return false;
                    }
                }
            }
        } else if (ch == '}') {
            if (i + 1 < fmt.size() && fmt[i + 1] == '}') {
                if (!append_char(buffer, length, '}')) {
                    return false;
                }
                i += 2;
            } else {
                if (!append_char(buffer, length, '}')) {
                    return false;
                }
                ++i;
            }
        } else if (ch == '%') {
            if (!append_literal(buffer, length, "%%")) {
                return false;
            }
            ++i;
        } else {
            if (!append_char(buffer, length, ch)) {
                return false;
            }
            ++i;
        }
    }

    if (length >= buffer.size()) {
        return false;
    }
    buffer[length] = '\0';
    return true;
}

template <std::size_t N, typename... Args>
std::size_t format_to_buffer(StaticBuffer<N> &buffer, fmt::format_string<Args...> fmt_str, Args &&...args)
{
    if (buffer.empty()) {
        return 0;
    }
#if GUI_FORMAT_HAS_FMT
    auto result = fmt::format_to_n(buffer.begin(), buffer.size() - 1, fmt_str, std::forward<Args>(args)...);
    const std::size_t count = std::min<std::size_t>(result.size, buffer.size() - 1);
    buffer[count]         = '\0';
    return count;
#else
    StaticBuffer<kMaxPatternSize> pattern{};
    std::size_t                  length = 0;
    if (!convert_to_printf_pattern(pattern, length, to_string_view(fmt_str))) {
        buffer[0] = '\0';
        return 0;
    }

    int written = std::snprintf(buffer.data(), buffer.size(), pattern.data(), std::forward<Args>(args)...);
    if (written < 0) {
        buffer[0] = '\0';
        return 0;
    }

    std::size_t count = static_cast<std::size_t>(written);
    if (count >= buffer.size()) {
        count = buffer.size() - 1;
    }
    buffer[count] = '\0';
    return count;
#endif
}

}  // namespace detail

constexpr std::size_t kDefaultLabelBufferSize = 64;

template <typename... Args>
void set_label_textf(lv_obj_t *label, fmt::format_string<Args...> fmt_str, Args &&...args)
{
    if (!label) {
        return;
    }

    StaticBuffer<kDefaultLabelBufferSize> buffer{};
    detail::format_to_buffer(buffer, fmt_str, std::forward<Args>(args)...);
    lv_label_set_text(label, buffer.data());
}

enum class StatusState { Neutral, Ok, Warn, Error };

struct StatusPalette {
    lv_palette_t neutral{LV_PALETTE_GREY};
    lv_palette_t ok{LV_PALETTE_GREEN};
    lv_palette_t warn{LV_PALETTE_YELLOW};
    lv_palette_t error{LV_PALETTE_RED};
};

class StatusLabel {
public:
    StatusLabel() = default;
    StatusLabel(lv_obj_t *label, StatusPalette palette = {}) { reset(label, palette); }

    void reset(lv_obj_t *label, StatusPalette palette = {})
    {
        label_   = label;
        palette_ = palette;
    }

    lv_obj_t *get() const { return label_; }

    void set(const char *text, StatusState state) const
    {
        switch (state) {
        case StatusState::Ok:
            set_with_palette(text, palette_.ok);
            break;
        case StatusState::Warn:
            set_with_palette(text, palette_.warn);
            break;
        case StatusState::Error:
            set_with_palette(text, palette_.error);
            break;
        case StatusState::Neutral:
        default:
            set_with_palette(text, palette_.neutral);
            break;
        }
    }

    void set_with_palette(const char *text, lv_palette_t palette) const
    {
        if (!label_) {
            return;
        }
        lv_label_set_text(label_, text ? text : "");
        lv_obj_set_style_text_color(label_, lv_palette_main(palette), 0);
    }

private:
    lv_obj_t    *label_{nullptr};
    StatusPalette palette_{};
};

}  // namespace gui
