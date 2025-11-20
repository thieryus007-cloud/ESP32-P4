#include "TinyBMS.h"
#include "lvgl.h" // Votre librairie LVGL

TinyBMS bms;

// Fonction appelée par un Timer LVGL (ex: toutes les 500ms) pour rafraîchir l'UI
void update_ui_callback(lv_timer_t * timer) {
    // Récupération thread-safe des dernières données
    TinyBMSData data = bms.getData();

    // Mise à jour des widgets LVGL
    // Exemple : Label Voltage
    lv_label_set_text_fmt(ui_LabelPackVoltage, "%.2f V", data.packVoltage);
    
    // Exemple : Jauge SOC
    lv_bar_set_value(ui_BarSOC, (int32_t)data.soc, LV_ANIM_ON);

    // Exemple : Barres Cellules
    for(int i=0; i<16; i++) {
        // Supposons que vous ayez un tableau de barres ui_CellBars[]
        int val = (int)((data.cellVoltages[i] - 3.0f) * 100); // Simple mapping 3V-4V -> 0-100
        if(val < 0) val = 0;
        // lv_bar_set_value(ui_CellBars[i], val, LV_ANIM_OFF);
    }
}

// Fonction appelée quand on appuie sur un bouton "Sauvegarder Paramètre"
void on_save_settings_click(lv_event_t * e) {
    // Exemple : changer le cutoff charge à 4.1V
    // ID 300, Valeur 4.1, Scale 0.001 (car stocké en mV)
    bms.writeRegister(REG_FULLY_CHARGED_VOLTAGE, 4.1f, 0.001f);
}

extern "C" void app_main() {
    // 1. Init BMS Communication
    bms.begin();

    // 2. Init LVGL (votre code init standard)
    // ... lv_init(), lv_display_create()...

    // 3. Créer un timer LVGL pour le rafraichissement des données
    lv_timer_create(update_ui_callback, 500, NULL);

    // 4. Boucle principale (si nécessaire pour LVGL handle task)
    while (1) {
        lv_timer_handler();
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
