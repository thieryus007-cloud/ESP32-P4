#include <cassert>
#include <string_view>

#include "gui_format.hpp"

int main()
{
    gui::StaticBuffer<8> truncated{};
    auto len = gui::detail::format_to_buffer(truncated, "{:s}", "123456789");
    assert(len == 7);
    assert(std::string_view(truncated.data()) == "1234567");

    gui::StaticBuffer<16> with_percent{};
    gui::detail::format_to_buffer(with_percent, "{:.1f} %", 12.34f);
    assert(std::string_view(with_percent.data()) == "12.3 %");

    gui::StaticBuffer<16> escaped{};
    gui::detail::format_to_buffer(escaped, "{{val}}: {:02d}", 7);
    assert(std::string_view(escaped.data()) == "{val}: 07");

    return 0;
}
