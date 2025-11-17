#include "ui_i18n.h"

#include "esp_log.h"
#include "nvs_flash.h"
#include "nvs.h"
#include <string.h>

#define UI_I18N_NAMESPACE "ui_prefs"
#define UI_I18N_KEY       "lang"
#define UI_I18N_MAX_LISTENERS 16

static const char *TAG = "UI_I18N";

typedef struct {
    const char *key;
    const char *fr;
    const char *en;
} ui_translation_t;

static const ui_translation_t s_translations[] = {
    // Tabs
    {"tab.dashboard", "Dashboard", "Dashboard"},
    {"tab.home", "Accueil", "Home"},
    {"tab.pack", "Pack", "Pack"},
    {"tab.cells", "Cellules", "Cells"},
    {"tab.power", "Puissance", "Power"},
    {"tab.alerts", "Alertes", "Alerts"},
    {"tab.config", "Config", "Config"},
    {"tab.tbms_status", "Statut TBMS", "TBMS Status"},
    {"tab.tbms_config", "Config TBMS", "TBMS Config"},
    {"tab.can_status", "Statut CAN", "CAN Status"},
    {"tab.can_config", "Config CAN", "CAN Config"},
    {"tab.bms_control", "Contr\xC3\xB4le BMS", "BMS Control"},
    {"tab.history", "Historique", "History"},

    // Home
    {"home.soc", "SOC", "SOC"},
    {"home.voltage", "Tension", "Voltage"},
    {"home.current", "Courant", "Current"},
    {"home.power", "Puissance", "Power"},
    {"home.temperature", "Temp", "Temp"},
    {"home.status.bms", "BMS", "BMS"},
    {"home.status.can", "CAN", "CAN"},
    {"home.status.mqtt", "MQTT", "MQTT"},
    {"home.status.wifi", "WiFi", "WiFi"},
    {"home.status.bal", "BAL", "BAL"},
    {"home.status.alm", "ALM", "ALM"},

    // Dashboard
    {"dashboard.card.soc", "SOC / SOH", "SOC / SOH"},
    {"dashboard.card.temp", "Temp\xC3\xA9rature", "Temperature"},
    {"dashboard.card.power", "Puissance & Courant", "Power & Current"},
    {"dashboard.card.status", "Statuts syst\xC3\xA8me", "System status"},
    {"dashboard.status.wifi", "WiFi", "WiFi"},
    {"dashboard.status.storage", "Stockage", "Storage"},
    {"dashboard.status.errors", "Erreurs", "Errors"},

    // Power screen
    {"power.pv", "PV : N/A", "PV: N/A"},
    {"power.battery", "[Batterie]", "[Battery]"},
    {"power.flow.default", "\xE2\x86\x92", "\xE2\x86\x92"},
    {"power.flow.charge", "\xE2\x86\x90", "\xE2\x86\x90"},
    {"power.flow.dir_discharge", "vers CHARGE", "to LOAD"},
    {"power.flow.dir_charge", "depuis CHARGE/GRID", "from LOAD/GRID"},
    {"power.load", "[Charge/Grille]", "[Load/Grid]"},
    {"power.status.ok", "Statut : OK", "Status: OK"},
    {"power.status.check", "Statut : V\xC3\xA9rifier syst\xC3\xA8me", "Status: CHECK SYSTEM"},

    // Config screen
    {"config.title", "Configuration HMI / BMS", "HMI / BMS configuration"},
    {"config.section.wifi", "WiFi STA", "WiFi STA"},
    {"config.section.mqtt", "MQTT", "MQTT"},
    {"config.section.bus", "Bus CAN & UART", "CAN & UART bus"},
    {"config.label.ssid", "SSID", "SSID"},
    {"config.label.password", "Mot de passe", "Password"},
    {"config.label.static_ip", "IP statique (optionnel)", "Static IP (optional)"},
    {"config.placeholder.static_ip", "192.168.1.50", "192.168.1.50"},
    {"config.label.broker", "Broker (host:port)", "Broker (host:port)"},
    {"config.label.pub", "Topic publication", "Publish topic"},
    {"config.label.sub", "Topic souscription", "Subscribe topic"},
    {"config.label.can", "CAN bitrate (ex: 500000)", "CAN bitrate (e.g. 500000)"},
    {"config.label.uart_baud", "UART baudrate", "UART baudrate"},
    {"config.label.uart_parity", "UART parit\xC3\xA9 (N/E/O)", "UART parity (N/E/O)"},
    {"config.label.language", "Langue", "Language"},
    {"config.btn.reload", "Recharger", "Reload"},
    {"config.btn.reconnect", "Rebasculer en connect\xC3\xA9", "Switch back to connected"},
    {"config.btn.save", "Sauvegarder", "Save"},
    {"config.status.ready", "Pr\xC3\xAAt", "Ready"},
    {"config.status.loading", "Chargement configuration...", "Loading configuration..."},
    {"config.status.saving", "Enregistrement...", "Saving..."},
    {"config.status.updated", "Configuration mise \xC3\xA0 jour", "Configuration updated"},
    {"config.status.reconnect", "Mode connect\xC3\xA9 demand\xC3\xA9", "Connected mode requested"},
    {"config.error.ssid", "SSID requis", "SSID required"},
    {"config.error.broker", "Broker MQTT requis", "MQTT broker required"},
    {"config.error.ip", "IP statique invalide (xxx.xxx.xxx.xxx)", "Invalid static IP (xxx.xxx.xxx.xxx)"},
    {"config.error.can", "Bitrate CAN invalide", "Invalid CAN bitrate"},
    {"config.error.baud", "Baudrate UART invalide", "Invalid UART baudrate"},
};

static ui_language_t s_language = UI_LANG_FR;

static struct {
    ui_i18n_listener_t cb;
    void *ctx;
} s_listeners[UI_I18N_MAX_LISTENERS];
static size_t s_listener_count = 0;

static void load_language_from_nvs(void)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open(UI_I18N_NAMESPACE, NVS_READONLY, &handle);
    if (err != ESP_OK) {
        ESP_LOGI(TAG, "NVS open for language (read) failed: %s", esp_err_to_name(err));
        return;
    }

    uint8_t lang = 0;
    err = nvs_get_u8(handle, UI_I18N_KEY, &lang);
    if (err == ESP_OK && lang <= UI_LANG_EN) {
        s_language = (ui_language_t) lang;
        ESP_LOGI(TAG, "Loaded language from NVS: %d", lang);
    }

    nvs_close(handle);
}

static void save_language_to_nvs(ui_language_t lang)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open(UI_I18N_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Unable to open NVS for language save: %s", esp_err_to_name(err));
        return;
    }

    err = nvs_set_u8(handle, UI_I18N_KEY, (uint8_t) lang);
    if (err == ESP_OK) {
        nvs_commit(handle);
    } else {
        ESP_LOGW(TAG, "Failed to store language: %s", esp_err_to_name(err));
    }

    nvs_close(handle);
}

static void notify_listeners(void)
{
    for (size_t i = 0; i < s_listener_count; ++i) {
        if (s_listeners[i].cb) {
            s_listeners[i].cb(s_listeners[i].ctx);
        }
    }
}

void ui_i18n_init(void)
{
    load_language_from_nvs();
}

ui_language_t ui_i18n_get_language(void)
{
    return s_language;
}

void ui_i18n_set_language(ui_language_t lang)
{
    if (lang > UI_LANG_EN) {
        return;
    }
    if (s_language == lang) {
        return;
    }

    s_language = lang;
    save_language_to_nvs(lang);
    notify_listeners();
}

const char *ui_i18n(const char *key)
{
    if (!key) return "";
    for (size_t i = 0; i < sizeof(s_translations) / sizeof(s_translations[0]); ++i) {
        if (strcmp(s_translations[i].key, key) == 0) {
            return (s_language == UI_LANG_FR) ? s_translations[i].fr : s_translations[i].en;
        }
    }
    return key;
}

void ui_i18n_label_set_text(lv_obj_t *label, const char *key)
{
    if (!label) return;
    lv_label_set_text(label, ui_i18n(key));
}

void ui_i18n_register_listener(ui_i18n_listener_t cb, void *user_ctx)
{
    if (!cb || s_listener_count >= UI_I18N_MAX_LISTENERS) {
        return;
    }
    s_listeners[s_listener_count].cb = cb;
    s_listeners[s_listener_count].ctx = user_ctx;
    s_listener_count++;
}
