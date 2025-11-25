/**
 * @file keyboard_example.c
 * @brief Exemples d'utilisation du widget clavier virtuel
 *
 * Ce fichier montre différentes façons d'utiliser le widget clavier
 * dans votre application.
 */

#include "widget_keyboard.h"
#include "lvgl.h"

/**
 * Exemple 1: Clavier simple avec textarea
 *
 * Utilisation basique: créer un textarea et un clavier,
 * le clavier s'affiche automatiquement quand le textarea reçoit le focus.
 */
void keyboard_example_basic(lv_obj_t *parent) {
    // Créer un label
    lv_obj_t *label = lv_label_create(parent);
    lv_label_set_text(label, "Entrez votre nom:");
    lv_obj_align(label, LV_ALIGN_TOP_LEFT, 10, 10);

    // Créer un textarea
    lv_obj_t *textarea = lv_textarea_create(parent);
    lv_obj_set_size(textarea, 300, 40);
    lv_obj_align(textarea, LV_ALIGN_TOP_LEFT, 10, 40);
    lv_textarea_set_one_line(textarea, true);
    lv_textarea_set_placeholder_text(textarea, "Votre nom...");

    // Créer le clavier avec configuration par défaut
    widget_keyboard_config_t config = WIDGET_KEYBOARD_DEFAULT_CONFIG;
    widget_keyboard_t *keyboard = widget_keyboard_create(parent, &config);

    // Attacher le clavier au textarea
    // Le clavier s'affichera automatiquement quand le textarea reçoit le focus
    widget_keyboard_set_textarea(keyboard, textarea);
}

/**
 * Exemple 2: Clavier numérique pour saisie de nombres
 *
 * Pour entrer des valeurs numériques (température, tension, etc.)
 */
void keyboard_example_numeric(lv_obj_t *parent) {
    // Label
    lv_obj_t *label = lv_label_create(parent);
    lv_label_set_text(label, "Entrez la tension (V):");
    lv_obj_align(label, LV_ALIGN_TOP_LEFT, 10, 100);

    // Textarea
    lv_obj_t *textarea = lv_textarea_create(parent);
    lv_obj_set_size(textarea, 150, 40);
    lv_obj_align(textarea, LV_ALIGN_TOP_LEFT, 10, 130);
    lv_textarea_set_one_line(textarea, true);
    lv_textarea_set_placeholder_text(textarea, "0.00");
    lv_textarea_set_accepted_chars(textarea, "0123456789."); // Accepter seulement chiffres et point

    // Clavier numérique
    widget_keyboard_config_t config = WIDGET_KEYBOARD_DEFAULT_CONFIG;
    config.mode = KEYBOARD_MODE_NUMBER;
    widget_keyboard_t *keyboard = widget_keyboard_create(parent, &config);
    widget_keyboard_set_textarea(keyboard, textarea);
}

/**
 * Exemple 3: Clavier avec callback personnalisé
 *
 * Traiter la saisie quand l'utilisateur appuie sur OK
 */
static void on_keyboard_ok(lv_event_t *e) {
    lv_obj_t *textarea = lv_event_get_user_data(e);
    const char *text = lv_textarea_get_text(textarea);

    // Traiter la saisie
    printf("Texte saisi: %s\n", text);

    // Optionnel: effacer le textarea
    lv_textarea_set_text(textarea, "");
}

void keyboard_example_with_callback(lv_obj_t *parent) {
    // Label
    lv_obj_t *label = lv_label_create(parent);
    lv_label_set_text(label, "Entrez un message:");
    lv_obj_align(label, LV_ALIGN_TOP_LEFT, 10, 180);

    // Textarea
    lv_obj_t *textarea = lv_textarea_create(parent);
    lv_obj_set_size(textarea, 300, 80);
    lv_obj_align(textarea, LV_ALIGN_TOP_LEFT, 10, 210);
    lv_textarea_set_placeholder_text(textarea, "Votre message...");

    // Clavier avec callback
    widget_keyboard_config_t config = WIDGET_KEYBOARD_DEFAULT_CONFIG;
    widget_keyboard_t *keyboard = widget_keyboard_create(parent, &config);
    widget_keyboard_set_textarea(keyboard, textarea);

    // Définir un callback appelé quand OK est pressé
    widget_keyboard_set_ok_callback(keyboard, on_keyboard_ok, textarea);
}

/**
 * Exemple 4: Plusieurs textareas avec un seul clavier
 *
 * Réutiliser le même clavier pour plusieurs champs de saisie
 */
void keyboard_example_multiple_textareas(lv_obj_t *parent) {
    // Créer plusieurs textareas
    lv_obj_t *ta1 = lv_textarea_create(parent);
    lv_obj_set_size(ta1, 200, 40);
    lv_obj_align(ta1, LV_ALIGN_TOP_LEFT, 10, 300);
    lv_textarea_set_one_line(ta1, true);
    lv_textarea_set_placeholder_text(ta1, "SSID WiFi...");

    lv_obj_t *ta2 = lv_textarea_create(parent);
    lv_obj_set_size(ta2, 200, 40);
    lv_obj_align(ta2, LV_ALIGN_TOP_LEFT, 10, 350);
    lv_textarea_set_one_line(ta2, true);
    lv_textarea_set_password_mode(ta2, true);
    lv_textarea_set_placeholder_text(ta2, "Mot de passe...");

    lv_obj_t *ta3 = lv_textarea_create(parent);
    lv_obj_set_size(ta3, 200, 40);
    lv_obj_align(ta3, LV_ALIGN_TOP_LEFT, 10, 400);
    lv_textarea_set_one_line(ta3, true);
    lv_textarea_set_placeholder_text(ta3, "Adresse IP...");

    // Créer un seul clavier
    widget_keyboard_config_t config = WIDGET_KEYBOARD_DEFAULT_CONFIG;
    widget_keyboard_t *keyboard = widget_keyboard_create(parent, &config);

    // Le clavier se connectera automatiquement au textarea qui reçoit le focus
    widget_keyboard_set_textarea(keyboard, ta1);
    widget_keyboard_set_textarea(keyboard, ta2);
    widget_keyboard_set_textarea(keyboard, ta3);
}

/**
 * Exemple 5: Contrôle manuel du clavier
 *
 * Afficher/masquer le clavier manuellement avec des boutons
 */
static void show_keyboard_cb(lv_event_t *e) {
    widget_keyboard_t *keyboard = lv_event_get_user_data(e);
    widget_keyboard_show(keyboard);
}

static void hide_keyboard_cb(lv_event_t *e) {
    widget_keyboard_t *keyboard = lv_event_get_user_data(e);
    widget_keyboard_hide(keyboard);
}

static void toggle_keyboard_cb(lv_event_t *e) {
    widget_keyboard_t *keyboard = lv_event_get_user_data(e);
    widget_keyboard_toggle(keyboard);
}

void keyboard_example_manual_control(lv_obj_t *parent) {
    // Textarea
    lv_obj_t *textarea = lv_textarea_create(parent);
    lv_obj_set_size(textarea, 300, 40);
    lv_obj_align(textarea, LV_ALIGN_TOP_MID, 0, 450);

    // Clavier (masqué par défaut)
    widget_keyboard_config_t config = WIDGET_KEYBOARD_DEFAULT_CONFIG;
    config.auto_hide = false; // Ne pas masquer automatiquement
    widget_keyboard_t *keyboard = widget_keyboard_create(parent, &config);
    widget_keyboard_set_textarea(keyboard, textarea);

    // Boutons de contrôle
    lv_obj_t *btn_show = lv_btn_create(parent);
    lv_obj_align(btn_show, LV_ALIGN_TOP_LEFT, 10, 500);
    lv_obj_t *label1 = lv_label_create(btn_show);
    lv_label_set_text(label1, "Afficher");
    lv_obj_add_event_cb(btn_show, show_keyboard_cb, LV_EVENT_CLICKED, keyboard);

    lv_obj_t *btn_hide = lv_btn_create(parent);
    lv_obj_align(btn_hide, LV_ALIGN_TOP_MID, 0, 500);
    lv_obj_t *label2 = lv_label_create(btn_hide);
    lv_label_set_text(label2, "Masquer");
    lv_obj_add_event_cb(btn_hide, hide_keyboard_cb, LV_EVENT_CLICKED, keyboard);

    lv_obj_t *btn_toggle = lv_btn_create(parent);
    lv_obj_align(btn_toggle, LV_ALIGN_TOP_RIGHT, -10, 500);
    lv_obj_t *label3 = lv_label_create(btn_toggle);
    lv_label_set_text(label3, "Basculer");
    lv_obj_add_event_cb(btn_toggle, toggle_keyboard_cb, LV_EVENT_CLICKED, keyboard);
}

/**
 * Exemple 6: Changer le mode du clavier dynamiquement
 *
 * Basculer entre différents types de claviers
 */
static void change_to_text_cb(lv_event_t *e) {
    widget_keyboard_t *keyboard = lv_event_get_user_data(e);
    widget_keyboard_set_mode(keyboard, KEYBOARD_MODE_TEXT);
}

static void change_to_number_cb(lv_event_t *e) {
    widget_keyboard_t *keyboard = lv_event_get_user_data(e);
    widget_keyboard_set_mode(keyboard, KEYBOARD_MODE_NUMBER);
}

static void change_to_special_cb(lv_event_t *e) {
    widget_keyboard_t *keyboard = lv_event_get_user_data(e);
    widget_keyboard_set_mode(keyboard, KEYBOARD_MODE_SPECIAL);
}

void keyboard_example_change_mode(lv_obj_t *parent) {
    // Textarea
    lv_obj_t *textarea = lv_textarea_create(parent);
    lv_obj_set_size(textarea, 300, 60);
    lv_obj_align(textarea, LV_ALIGN_CENTER, 0, -100);

    // Clavier
    widget_keyboard_config_t config = WIDGET_KEYBOARD_DEFAULT_CONFIG;
    widget_keyboard_t *keyboard = widget_keyboard_create(parent, &config);
    widget_keyboard_set_textarea(keyboard, textarea);

    // Boutons pour changer de mode
    lv_obj_t *btn_text = lv_btn_create(parent);
    lv_obj_align(btn_text, LV_ALIGN_CENTER, -100, -30);
    lv_obj_t *lbl1 = lv_label_create(btn_text);
    lv_label_set_text(lbl1, "Texte");
    lv_obj_add_event_cb(btn_text, change_to_text_cb, LV_EVENT_CLICKED, keyboard);

    lv_obj_t *btn_num = lv_btn_create(parent);
    lv_obj_align(btn_num, LV_ALIGN_CENTER, 0, -30);
    lv_obj_t *lbl2 = lv_label_create(btn_num);
    lv_label_set_text(lbl2, "123");
    lv_obj_add_event_cb(btn_num, change_to_number_cb, LV_EVENT_CLICKED, keyboard);

    lv_obj_t *btn_spec = lv_btn_create(parent);
    lv_obj_align(btn_spec, LV_ALIGN_CENTER, 100, -30);
    lv_obj_t *lbl3 = lv_label_create(btn_spec);
    lv_label_set_text(lbl3, "#@!");
    lv_obj_add_event_cb(btn_spec, change_to_special_cb, LV_EVENT_CLICKED, keyboard);
}

/**
 * Fonction principale pour tester tous les exemples
 */
void keyboard_examples_all(lv_obj_t *parent) {
    // Note: Dans une vraie application, vous n'utiliseriez qu'un seul exemple
    // à la fois. Ici, ils sont tous listés pour démonstration.

    // Décommenter celui que vous voulez tester:

    // keyboard_example_basic(parent);
    // keyboard_example_numeric(parent);
    // keyboard_example_with_callback(parent);
    // keyboard_example_multiple_textareas(parent);
    // keyboard_example_manual_control(parent);
    // keyboard_example_change_mode(parent);
}
