# Widget Clavier Virtuel - Guide Complet

## Vue d'ensemble

Le widget clavier virtuel (`widget_keyboard`) fournit un clavier LVGL entièrement fonctionnel pour la saisie de texte sur écran tactile. Il s'intègre automatiquement avec les `lv_textarea` et supporte plusieurs modes de saisie.

## Caractéristiques

✅ **Modes multiples** : Texte, Numérique, Spéciaux, Hexadécimal
✅ **Auto-attachement** : Se connecte automatiquement au textarea en focus
✅ **Animations** : Slide-up/down fluides
✅ **Callbacks** : Événements personnalisables
✅ **Multi-textarea** : Un clavier pour plusieurs champs
✅ **Contrôle manuel** : API pour afficher/masquer programmatiquement

## Architecture

### Fichiers

```
components/gui_lvgl/widgets/
├── include/
│   └── widget_keyboard.h        # API publique
├── src/
│   └── widget_keyboard.c        # Implémentation
└── examples/
    └── keyboard_example.c       # 6 exemples d'utilisation
```

### Structure

```c
typedef struct {
    lv_obj_t *keyboard;      // Objet clavier LVGL
    lv_obj_t *textarea;      // Textarea associé
    lv_obj_t *container;     // Container pour positionnement
    keyboard_mode_t mode;    // Mode actuel
    bool is_visible;         // État de visibilité
} widget_keyboard_t;
```

## Modes Disponibles

| Mode | Description | Usage typique |
|------|-------------|---------------|
| `KEYBOARD_MODE_TEXT` | Clavier texte complet AZERTY/QWERTY | Noms, adresses, messages |
| `KEYBOARD_MODE_NUMBER` | Clavier numérique avec décimales | Tensions, températures, quantités |
| `KEYBOARD_MODE_SPECIAL` | Caractères spéciaux (@, #, $, etc.) | Emails, mots de passe |
| `KEYBOARD_MODE_HEX` | Hexadécimal (0-9, A-F) | Adresses MAC, configuration BMS |

## Installation

### 1. Inclure le header

```c
#include "widget_keyboard.h"
```

### 2. CMakeLists.txt

Le widget est déjà inclus dans `components/gui_lvgl/CMakeLists.txt` :

```cmake
SRCS "widgets/src/widget_keyboard.c"
```

## Guide d'utilisation

### Exemple 1 : Utilisation de base

Le cas le plus simple : un textarea avec un clavier qui apparaît au focus.

```c
void create_input_form(lv_obj_t *parent) {
    // Créer un label
    lv_obj_t *label = lv_label_create(parent);
    lv_label_set_text(label, "Nom d'utilisateur:");

    // Créer un textarea
    lv_obj_t *textarea = lv_textarea_create(parent);
    lv_obj_set_size(textarea, 300, 40);
    lv_textarea_set_one_line(textarea, true);
    lv_textarea_set_placeholder_text(textarea, "Entrez votre nom...");

    // Créer le clavier avec configuration par défaut
    widget_keyboard_config_t config = WIDGET_KEYBOARD_DEFAULT_CONFIG;
    widget_keyboard_t *keyboard = widget_keyboard_create(parent, &config);

    // Attacher le textarea (le clavier s'affichera automatiquement au focus)
    widget_keyboard_set_textarea(keyboard, textarea);
}
```

**Comportement** :
- L'utilisateur touche le textarea → Le clavier apparaît (slide-up)
- L'utilisateur appuie sur OK → Le clavier se masque (slide-down)
- L'utilisateur appuie sur Fermer → Le clavier se masque

### Exemple 2 : Clavier numérique

Pour la saisie de valeurs numériques (tension, courant, température).

```c
void create_voltage_input(lv_obj_t *parent) {
    lv_obj_t *label = lv_label_create(parent);
    lv_label_set_text(label, "Tension (V):");

    lv_obj_t *textarea = lv_textarea_create(parent);
    lv_obj_set_size(textarea, 150, 40);
    lv_textarea_set_one_line(textarea, true);
    lv_textarea_set_placeholder_text(textarea, "0.00");

    // Limiter aux chiffres et point décimal
    lv_textarea_set_accepted_chars(textarea, "0123456789.");

    // Clavier numérique
    widget_keyboard_config_t config = WIDGET_KEYBOARD_DEFAULT_CONFIG;
    config.mode = KEYBOARD_MODE_NUMBER;

    widget_keyboard_t *keyboard = widget_keyboard_create(parent, &config);
    widget_keyboard_set_textarea(keyboard, textarea);
}
```

### Exemple 3 : Traitement de la saisie avec callback

Exécuter du code quand l'utilisateur valide sa saisie.

```c
// Callback appelé quand OK est pressé
static void on_password_entered(lv_event_t *e) {
    lv_obj_t *textarea = lv_event_get_user_data(e);
    const char *password = lv_textarea_get_text(textarea);

    // Vérifier le mot de passe
    if (validate_password(password)) {
        printf("Mot de passe correct!\n");
        // Continuer...
    } else {
        printf("Mot de passe incorrect!\n");
        lv_textarea_set_text(textarea, ""); // Effacer
    }
}

void create_password_input(lv_obj_t *parent) {
    lv_obj_t *textarea = lv_textarea_create(parent);
    lv_textarea_set_password_mode(textarea, true); // Masquer le texte
    lv_textarea_set_placeholder_text(textarea, "Mot de passe...");

    widget_keyboard_config_t config = WIDGET_KEYBOARD_DEFAULT_CONFIG;
    widget_keyboard_t *keyboard = widget_keyboard_create(parent, &config);
    widget_keyboard_set_textarea(keyboard, textarea);

    // Définir le callback
    widget_keyboard_set_ok_callback(keyboard, on_password_entered, textarea);
}
```

### Exemple 4 : Plusieurs textareas avec un clavier

Réutiliser le même clavier pour plusieurs champs (formulaire).

```c
void create_wifi_config_form(lv_obj_t *parent) {
    // SSID
    lv_obj_t *ta_ssid = lv_textarea_create(parent);
    lv_textarea_set_placeholder_text(ta_ssid, "SSID WiFi...");

    // Mot de passe
    lv_obj_t *ta_password = lv_textarea_create(parent);
    lv_textarea_set_password_mode(ta_password, true);
    lv_textarea_set_placeholder_text(ta_password, "Mot de passe...");

    // IP statique
    lv_obj_t *ta_ip = lv_textarea_create(parent);
    lv_textarea_set_placeholder_text(ta_ip, "192.168.1.100");

    // Un seul clavier pour tous
    widget_keyboard_config_t config = WIDGET_KEYBOARD_DEFAULT_CONFIG;
    widget_keyboard_t *keyboard = widget_keyboard_create(parent, &config);

    // Attacher tous les textareas
    // Le clavier se connectera automatiquement à celui qui a le focus
    widget_keyboard_set_textarea(keyboard, ta_ssid);
    widget_keyboard_set_textarea(keyboard, ta_password);
    widget_keyboard_set_textarea(keyboard, ta_ip);
}
```

### Exemple 5 : Contrôle manuel

Afficher/masquer le clavier programmatiquement avec des boutons.

```c
static void show_kb_cb(lv_event_t *e) {
    widget_keyboard_t *kb = lv_event_get_user_data(e);
    widget_keyboard_show(kb);
}

static void hide_kb_cb(lv_event_t *e) {
    widget_keyboard_t *kb = lv_event_get_user_data(e);
    widget_keyboard_hide(kb);
}

void create_manual_keyboard(lv_obj_t *parent) {
    lv_obj_t *textarea = lv_textarea_create(parent);

    // Clavier sans auto-masquage
    widget_keyboard_config_t config = WIDGET_KEYBOARD_DEFAULT_CONFIG;
    config.auto_hide = false;
    widget_keyboard_t *keyboard = widget_keyboard_create(parent, &config);
    widget_keyboard_set_textarea(keyboard, textarea);

    // Bouton "Afficher"
    lv_obj_t *btn_show = lv_btn_create(parent);
    lv_obj_t *lbl1 = lv_label_create(btn_show);
    lv_label_set_text(lbl1, "Afficher Clavier");
    lv_obj_add_event_cb(btn_show, show_kb_cb, LV_EVENT_CLICKED, keyboard);

    // Bouton "Masquer"
    lv_obj_t *btn_hide = lv_btn_create(parent);
    lv_obj_t *lbl2 = lv_label_create(btn_hide);
    lv_label_set_text(lbl2, "Masquer Clavier");
    lv_obj_add_event_cb(btn_hide, hide_kb_cb, LV_EVENT_CLICKED, keyboard);
}
```

### Exemple 6 : Changer le mode dynamiquement

Basculer entre différents types de clavier selon le contexte.

```c
static void set_text_mode_cb(lv_event_t *e) {
    widget_keyboard_t *kb = lv_event_get_user_data(e);
    widget_keyboard_set_mode(kb, KEYBOARD_MODE_TEXT);
}

static void set_number_mode_cb(lv_event_t *e) {
    widget_keyboard_t *kb = lv_event_get_user_data(e);
    widget_keyboard_set_mode(kb, KEYBOARD_MODE_NUMBER);
}

void create_multi_mode_keyboard(lv_obj_t *parent) {
    lv_obj_t *textarea = lv_textarea_create(parent);

    widget_keyboard_config_t config = WIDGET_KEYBOARD_DEFAULT_CONFIG;
    widget_keyboard_t *keyboard = widget_keyboard_create(parent, &config);
    widget_keyboard_set_textarea(keyboard, textarea);

    // Boutons pour changer de mode
    lv_obj_t *btn_text = lv_btn_create(parent);
    lv_label_set_text(lv_label_create(btn_text), "ABC");
    lv_obj_add_event_cb(btn_text, set_text_mode_cb, LV_EVENT_CLICKED, keyboard);

    lv_obj_t *btn_num = lv_btn_create(parent);
    lv_label_set_text(lv_label_create(btn_num), "123");
    lv_obj_add_event_cb(btn_num, set_number_mode_cb, LV_EVENT_CLICKED, keyboard);
}
```

## API Complète

### Fonctions de création/destruction

#### `widget_keyboard_create()`
```c
widget_keyboard_t* widget_keyboard_create(
    lv_obj_t *parent,
    const widget_keyboard_config_t *config);
```
Crée un nouveau widget clavier.

**Paramètres** :
- `parent` : Objet parent LVGL
- `config` : Configuration (NULL pour défaut)

**Retour** : Pointeur vers le widget créé

#### `widget_keyboard_destroy()`
```c
void widget_keyboard_destroy(widget_keyboard_t *keyboard);
```
Détruit le widget et libère la mémoire.

### Fonctions de configuration

#### `widget_keyboard_set_textarea()`
```c
void widget_keyboard_set_textarea(
    widget_keyboard_t *keyboard,
    lv_obj_t *textarea);
```
Attache le clavier à un textarea. Le clavier s'affichera automatiquement quand le textarea reçoit le focus.

#### `widget_keyboard_set_mode()`
```c
void widget_keyboard_set_mode(
    widget_keyboard_t *keyboard,
    keyboard_mode_t mode);
```
Change le mode du clavier (TEXT, NUMBER, SPECIAL, HEX).

#### `widget_keyboard_set_ok_callback()`
```c
void widget_keyboard_set_ok_callback(
    widget_keyboard_t *keyboard,
    lv_event_cb_t callback,
    void *user_data);
```
Définit un callback appelé quand l'utilisateur appuie sur OK.

### Fonctions de contrôle

#### `widget_keyboard_show()`
```c
void widget_keyboard_show(widget_keyboard_t *keyboard);
```
Affiche le clavier avec une animation slide-up.

#### `widget_keyboard_hide()`
```c
void widget_keyboard_hide(widget_keyboard_t *keyboard);
```
Masque le clavier avec une animation slide-down.

#### `widget_keyboard_toggle()`
```c
void widget_keyboard_toggle(widget_keyboard_t *keyboard);
```
Bascule entre visible et masqué.

## Configuration

### Structure de configuration

```c
typedef struct {
    keyboard_mode_t mode;    // Mode initial (défaut: TEXT)
    bool auto_hide;          // Masquer après validation (défaut: true)
    lv_coord_t height;       // Hauteur (0 = auto)
    const char *ok_text;     // Texte bouton OK (NULL = "OK")
    const char *close_text;  // Texte bouton fermer (NULL = "Fermer")
} widget_keyboard_config_t;
```

### Configuration par défaut

```c
widget_keyboard_config_t config = WIDGET_KEYBOARD_DEFAULT_CONFIG;
// Équivalent à:
// {
//     .mode = KEYBOARD_MODE_TEXT,
//     .auto_hide = true,
//     .height = 0,
//     .ok_text = NULL,
//     .close_text = NULL
// }
```

### Personnalisation

```c
widget_keyboard_config_t config = WIDGET_KEYBOARD_DEFAULT_CONFIG;
config.mode = KEYBOARD_MODE_NUMBER;  // Clavier numérique
config.auto_hide = false;            // Ne pas masquer automatiquement
config.height = 300;                 // Hauteur fixe de 300px
```

## Intégration dans les écrans existants

### Exemple : Écran de configuration WiFi

```c
// Dans screen_config.cpp

static widget_keyboard_t *s_keyboard = NULL;

void screen_config_create(lv_obj_t *parent) {
    // ... créer les textareas existants ...

    // Créer le clavier une seule fois
    widget_keyboard_config_t config = WIDGET_KEYBOARD_DEFAULT_CONFIG;
    s_keyboard = widget_keyboard_create(parent, &config);

    // Attacher aux textareas
    widget_keyboard_set_textarea(s_keyboard, fields_.ssid);
    widget_keyboard_set_textarea(s_keyboard, fields_.password);
    widget_keyboard_set_textarea(s_keyboard, fields_.static_ip);
    widget_keyboard_set_textarea(s_keyboard, fields_.mqtt_broker);
    // ... etc.
}

void screen_config_destroy(void) {
    if (s_keyboard) {
        widget_keyboard_destroy(s_keyboard);
        s_keyboard = NULL;
    }
}
```

## Bonnes pratiques

### ✅ À faire

1. **Un clavier par écran** : Créer un seul clavier et le réutiliser pour tous les textareas
2. **Libérer la mémoire** : Toujours appeler `widget_keyboard_destroy()` à la fin
3. **Mode adapté** : Utiliser le mode approprié (numérique pour les nombres, etc.)
4. **Validation** : Utiliser `lv_textarea_set_accepted_chars()` pour limiter les caractères
5. **Callbacks** : Traiter la saisie dans les callbacks OK

### ❌ À éviter

1. **Plusieurs claviers** : Ne pas créer un clavier par textarea (gaspillage mémoire)
2. **Fuites mémoire** : Ne pas oublier de détruire le clavier
3. **Conflits** : Ne pas attacher plusieurs claviers au même textarea
4. **Hauteur fixe** : Laisser `height = 0` sauf besoin spécifique

## Styles et Thèmes

Le clavier hérite automatiquement du thème LVGL actuel (Dark/Light).

### Personnalisation avancée

```c
// Accéder à l'objet LVGL sous-jacent
lv_obj_t *kb_obj = keyboard->keyboard;

// Appliquer des styles personnalisés
lv_obj_set_style_bg_color(kb_obj, lv_color_hex(0x1A202C), 0);
lv_obj_set_style_text_color(kb_obj, lv_color_white(), LV_PART_ITEMS);
```

## Performance

### Empreinte mémoire

- Widget structure : ~32 bytes
- Objet LVGL clavier : ~500-1000 bytes (selon LVGL)
- Animation : ~100 bytes temporaires

**Total** : ~1-2 KB par clavier

### Optimisation

- Réutiliser le même clavier pour plusieurs textareas
- Masquer le clavier quand non utilisé (`auto_hide = true`)
- Libérer le clavier sur les écrans peu utilisés

## Dépannage

### Le clavier ne s'affiche pas

**Causes possibles** :
1. Le textarea n'est pas attaché : Vérifier `widget_keyboard_set_textarea()`
2. Le clavier est masqué : Appeler `widget_keyboard_show()`
3. Problème de z-index : Le clavier est derrière d'autres objets

**Solution** :
```c
// Forcer l'affichage au premier plan
lv_obj_move_foreground(keyboard->container);
```

### Le clavier ne se masque pas après OK

**Cause** : `auto_hide = false` dans la config

**Solution** :
```c
widget_keyboard_config_t config = WIDGET_KEYBOARD_DEFAULT_CONFIG;
config.auto_hide = true; // Activer le masquage automatique
```

### Animation saccadée

**Cause** : Charge processeur élevée

**Solution** :
- Réduire la complexité des autres écrans
- Optimiser la fréquence de rafraîchissement LVGL
- Désactiver les animations :
```c
// Dans widget_keyboard_show/hide, commenter les animations
// lv_anim_start(&anim);
```

## Exemples complets

Consultez le fichier `components/gui_lvgl/widgets/examples/keyboard_example.c` pour 6 exemples complets et fonctionnels.

## Références

- **Documentation LVGL Keyboard** : https://docs.lvgl.io/8.3/widgets/keyboard.html
- **Code source** : `components/gui_lvgl/widgets/src/widget_keyboard.c`
- **Header** : `components/gui_lvgl/widgets/include/widget_keyboard.h`

---

**Dernière mise à jour** : Novembre 2025
**Version** : 1.0
**Auteur** : Développement ESP32-P4 HMI
