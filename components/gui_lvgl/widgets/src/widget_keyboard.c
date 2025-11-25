#include "widget_keyboard.h"
#include <stdlib.h>
#include <string.h>

// Callback interne pour gérer les événements du clavier
static void keyboard_event_cb(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t *kb = lv_event_get_target(e);
    widget_keyboard_t *keyboard = (widget_keyboard_t *)lv_event_get_user_data(e);

    if (code == LV_EVENT_READY || code == LV_EVENT_CANCEL) {
        // Masquer le clavier si auto_hide est activé
        if (keyboard && keyboard->is_visible) {
            widget_keyboard_hide(keyboard);
        }
    }
}

// Callback pour gérer le focus des textarea
static void textarea_focus_cb(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t *ta = lv_event_get_target(e);
    widget_keyboard_t *keyboard = (widget_keyboard_t *)lv_event_get_user_data(e);

    if (code == LV_EVENT_FOCUSED && keyboard) {
        // Attacher et afficher le clavier
        lv_keyboard_set_textarea(keyboard->keyboard, ta);
        widget_keyboard_show(keyboard);
    }
    else if (code == LV_EVENT_DEFOCUSED && keyboard) {
        // Optionnel: masquer le clavier quand le textarea perd le focus
        // Commenté car peut être gênant si l'utilisateur clique ailleurs
        // widget_keyboard_hide(keyboard);
    }
}

widget_keyboard_t* widget_keyboard_create(
    lv_obj_t *parent,
    const widget_keyboard_config_t *config) {

    // Configuration par défaut si NULL
    widget_keyboard_config_t cfg = WIDGET_KEYBOARD_DEFAULT_CONFIG;
    if (config != NULL) {
        cfg = *config;
    }

    // Allocation mémoire
    widget_keyboard_t *keyboard = lv_malloc(sizeof(widget_keyboard_t));
    if (keyboard == NULL) return NULL;

    // Container principal (pour gérer le positionnement)
    keyboard->container = lv_obj_create(parent);
    lv_obj_set_size(keyboard->container, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(keyboard->container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(keyboard->container, 0, 0);
    lv_obj_set_style_pad_all(keyboard->container, 0, 0);
    lv_obj_clear_flag(keyboard->container, LV_OBJ_FLAG_SCROLLABLE);

    // Positionner en bas de l'écran
    lv_obj_align(keyboard->container, LV_ALIGN_BOTTOM_MID, 0, 0);

    // Créer le clavier LVGL
    keyboard->keyboard = lv_keyboard_create(keyboard->container);
    lv_obj_set_width(keyboard->keyboard, LV_PCT(100));

    // Définir la hauteur si spécifiée
    if (cfg.height > 0) {
        lv_obj_set_height(keyboard->keyboard, cfg.height);
    }

    // Appliquer le mode initial
    keyboard->mode = cfg.mode;
    switch (cfg.mode) {
        case KEYBOARD_MODE_TEXT:
            lv_keyboard_set_mode(keyboard->keyboard, LV_KEYBOARD_MODE_TEXT_LOWER);
            break;
        case KEYBOARD_MODE_NUMBER:
            lv_keyboard_set_mode(keyboard->keyboard, LV_KEYBOARD_MODE_NUMBER);
            break;
        case KEYBOARD_MODE_SPECIAL:
            lv_keyboard_set_mode(keyboard->keyboard, LV_KEYBOARD_MODE_SPECIAL);
            break;
        case KEYBOARD_MODE_HEX:
            lv_keyboard_set_mode(keyboard->keyboard, LV_KEYBOARD_MODE_HEX);
            break;
    }

    // Personnaliser les textes des boutons si fournis
    // Note: LVGL 8.x ne permet pas de changer facilement le texte des boutons
    // Cette fonctionnalité nécessiterait une map personnalisée

    // Ajouter un callback pour gérer les événements
    lv_obj_add_event_cb(keyboard->keyboard, keyboard_event_cb, LV_EVENT_ALL, keyboard);

    // Initialiser les autres champs
    keyboard->textarea = NULL;
    keyboard->is_visible = false;

    // Masquer initialement le clavier
    lv_obj_add_flag(keyboard->container, LV_OBJ_FLAG_HIDDEN);

    return keyboard;
}

void widget_keyboard_set_textarea(
    widget_keyboard_t *keyboard,
    lv_obj_t *textarea) {

    if (keyboard == NULL) return;

    keyboard->textarea = textarea;

    if (textarea != NULL) {
        // Attacher le textarea au clavier
        lv_keyboard_set_textarea(keyboard->keyboard, textarea);

        // Ajouter un callback au textarea pour afficher le clavier au focus
        lv_obj_add_event_cb(textarea, textarea_focus_cb, LV_EVENT_ALL, keyboard);
    }
}

void widget_keyboard_set_mode(
    widget_keyboard_t *keyboard,
    keyboard_mode_t mode) {

    if (keyboard == NULL) return;

    keyboard->mode = mode;

    switch (mode) {
        case KEYBOARD_MODE_TEXT:
            lv_keyboard_set_mode(keyboard->keyboard, LV_KEYBOARD_MODE_TEXT_LOWER);
            break;
        case KEYBOARD_MODE_NUMBER:
            lv_keyboard_set_mode(keyboard->keyboard, LV_KEYBOARD_MODE_NUMBER);
            break;
        case KEYBOARD_MODE_SPECIAL:
            lv_keyboard_set_mode(keyboard->keyboard, LV_KEYBOARD_MODE_SPECIAL);
            break;
        case KEYBOARD_MODE_HEX:
            lv_keyboard_set_mode(keyboard->keyboard, LV_KEYBOARD_MODE_HEX);
            break;
    }
}

void widget_keyboard_show(widget_keyboard_t *keyboard) {
    if (keyboard == NULL) return;

    lv_obj_clear_flag(keyboard->container, LV_OBJ_FLAG_HIDDEN);
    keyboard->is_visible = true;

    // Animation de slide-up (optionnel)
    lv_anim_t anim;
    lv_anim_init(&anim);
    lv_anim_set_var(&anim, keyboard->container);
    lv_anim_set_exec_cb(&anim, (lv_anim_exec_xcb_t)lv_obj_set_y);
    lv_anim_set_values(&anim, lv_obj_get_y(keyboard->container) + 50, lv_obj_get_y(keyboard->container));
    lv_anim_set_time(&anim, 200);
    lv_anim_set_path_cb(&anim, lv_anim_path_ease_out);
    lv_anim_start(&anim);
}

void widget_keyboard_hide(widget_keyboard_t *keyboard) {
    if (keyboard == NULL) return;

    // Animation de slide-down (optionnel)
    lv_anim_t anim;
    lv_anim_init(&anim);
    lv_anim_set_var(&anim, keyboard->container);
    lv_anim_set_exec_cb(&anim, (lv_anim_exec_xcb_t)lv_obj_set_y);
    lv_anim_set_values(&anim, lv_obj_get_y(keyboard->container), lv_obj_get_y(keyboard->container) + 50);
    lv_anim_set_time(&anim, 200);
    lv_anim_set_path_cb(&anim, lv_anim_path_ease_in);

    // Callback pour masquer après l'animation
    lv_anim_set_ready_cb(&anim, [](lv_anim_t *a) {
        lv_obj_t *obj = (lv_obj_t *)a->var;
        lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);
    });

    lv_anim_start(&anim);

    keyboard->is_visible = false;
}

void widget_keyboard_toggle(widget_keyboard_t *keyboard) {
    if (keyboard == NULL) return;

    if (keyboard->is_visible) {
        widget_keyboard_hide(keyboard);
    } else {
        widget_keyboard_show(keyboard);
    }
}

void widget_keyboard_set_ok_callback(
    widget_keyboard_t *keyboard,
    lv_event_cb_t callback,
    void *user_data) {

    if (keyboard == NULL) return;

    // Ajouter un callback personnalisé pour l'événement READY (OK)
    lv_obj_add_event_cb(keyboard->keyboard, callback, LV_EVENT_READY, user_data);
}

void widget_keyboard_destroy(widget_keyboard_t *keyboard) {
    if (keyboard == NULL) return;

    // Arrêter toutes les animations en cours
    lv_anim_del(keyboard->container, NULL);
    lv_anim_del(keyboard->keyboard, NULL);

    // Détacher le textarea si existant
    if (keyboard->textarea) {
        lv_keyboard_set_textarea(keyboard->keyboard, NULL);
    }

    // Supprimer le container (qui supprime aussi le clavier)
    lv_obj_del(keyboard->container);

    // Libérer la mémoire
    lv_free(keyboard);
}
