# Widgets de Jauges pour Dashboard BMS

Ce document décrit les widgets de jauges circulaires et semi-circulaires créés pour afficher les données du BMS de manière visuelle et professionnelle.

## Vue d'ensemble

Deux nouveaux widgets ont été créés pour reproduire l'interface dashboard montrée dans l'image de référence :

1. **widget_gauge_circular** - Jauge circulaire complète (360°) avec aiguille
2. **widget_gauge_semicircular** - Jauge semi-circulaire (180°) avec multi-aiguilles

## 1. Widget Gauge Circulaire

### Description

Widget de jauge circulaire (360°) avec aiguille animée, idéal pour afficher des valeurs comme voltage, courant, pression, etc.

### Caractéristiques

✅ **Échelle circulaire complète** (0-360°)
✅ **Aiguille animée** avec rotation fluide
✅ **Support de valeurs négatives** (-5000 à +5000 par exemple)
✅ **Label central** avec valeur et unité
✅ **Personnalisation complète** (couleurs, tailles, format)
✅ **Animations** avec easing
✅ **Point central décoratif**

### Fichiers

```
components/gui_lvgl/widgets/
├── include/
│   └── widget_gauge_circular.h
└── src/
    └── widget_gauge_circular.c
```

### Utilisation de base

```c
#include "widget_gauge_circular.h"

// Configuration pour une jauge de voltage
widget_gauge_circular_config_t config = WIDGET_GAUGE_CIRCULAR_DEFAULT_CONFIG;
config.size = 200;                    // Diamètre 200px
config.min_value = -120;              // -120V
config.max_value = 120;               // +120V
config.unit = "V";                    // Unité
config.format = "%.1f";               // Format d'affichage
config.needle_color = lv_color_hex(0x4299E1);  // Bleu
config.needle_length = 70;            // 70% du rayon

// Créer la jauge
widget_gauge_circular_t *gauge = widget_gauge_circular_create(parent, &config);

// Mettre à jour la valeur
widget_gauge_circular_set_value(gauge, 50.5);  // 50.5V

// Détruire quand terminé
widget_gauge_circular_destroy(gauge);
```

### Configuration détaillée

```c
typedef struct {
    lv_coord_t size;           // Diamètre du cadran (défaut: 200)
    float min_value;           // Valeur min (ex: -5000)
    float max_value;           // Valeur max (ex: 5000)
    const char *title;         // Titre (NULL = pas de titre)
    const char *unit;          // Unité (ex: "V", "A")
    const char *format;        // Format d'affichage (ex: "%.1f")
    lv_color_t needle_color;   // Couleur de l'aiguille
    lv_color_t scale_color;    // Couleur de l'échelle
    uint16_t needle_width;     // Largeur de l'aiguille (défaut: 3)
    uint16_t needle_length;    // Longueur de l'aiguille en % (défaut: 70)
    bool animate;              // Activer les animations
    uint16_t anim_duration;    // Durée animation ms (défaut: 500)
} widget_gauge_circular_config_t;
```

### API Complète

#### `widget_gauge_circular_create()`
```c
widget_gauge_circular_t* widget_gauge_circular_create(
    lv_obj_t *parent,
    const widget_gauge_circular_config_t *config);
```
Crée une nouvelle jauge circulaire.

#### `widget_gauge_circular_set_value()`
```c
void widget_gauge_circular_set_value(
    widget_gauge_circular_t *gauge,
    float value);
```
Met à jour la valeur de la jauge avec animation.

#### `widget_gauge_circular_set_title()`
```c
void widget_gauge_circular_set_title(
    widget_gauge_circular_t *gauge,
    const char *title);
```
Change le titre de la jauge.

#### `widget_gauge_circular_destroy()`
```c
void widget_gauge_circular_destroy(widget_gauge_circular_t *gauge);
```
Détruit la jauge et libère la mémoire.

### Exemples d'utilisation

#### Jauge de voltage

```c
widget_gauge_circular_config_t voltage_cfg = WIDGET_GAUGE_CIRCULAR_DEFAULT_CONFIG;
voltage_cfg.size = 180;
voltage_cfg.min_value = 0;
voltage_cfg.max_value = 60;
voltage_cfg.unit = "V";
voltage_cfg.format = "%.2f";
voltage_cfg.needle_color = lv_color_hex(0x4299E1);

widget_gauge_circular_t *voltage_gauge =
    widget_gauge_circular_create(parent, &voltage_cfg);

widget_gauge_circular_set_value(voltage_gauge, 48.5);
```

#### Jauge de courant (avec valeurs négatives)

```c
widget_gauge_circular_config_t current_cfg = WIDGET_GAUGE_CIRCULAR_DEFAULT_CONFIG;
current_cfg.size = 180;
current_cfg.min_value = -100;  // Décharge
current_cfg.max_value = 100;   // Charge
current_cfg.unit = "A";
current_cfg.format = "%.1f";
current_cfg.needle_color = lv_color_hex(0x38A169);

widget_gauge_circular_t *current_gauge =
    widget_gauge_circular_create(parent, &current_cfg);

widget_gauge_circular_set_value(current_gauge, -25.3);  // Décharge de 25.3A
```

## 2. Widget Gauge Semi-Circulaire

### Description

Widget de jauge semi-circulaire (180°) avec support de plusieurs aiguilles, idéal pour afficher des températures multiples, SOC/SOH, ou toute donnée comparative.

### Caractéristiques

✅ **Arc semi-circulaire** (180°)
✅ **Jusqu'à 4 aiguilles** simultanées
✅ **Labels individuels** pour chaque aiguille avec nom et valeur
✅ **Dégradé de couleur** sur l'arc
✅ **Animations indépendantes** pour chaque aiguille
✅ **Échelle graduée** personnalisable

### Fichiers

```
components/gui_lvgl/widgets/
├── include/
│   └── widget_gauge_semicircular.h
└── src/
    └── widget_gauge_semicircular.c
```

### Utilisation de base

```c
#include "widget_gauge_semicircular.h"

// Configuration pour températures
widget_gauge_semicircular_config_t config = WIDGET_GAUGE_SEMICIRCULAR_DEFAULT_CONFIG;
config.width = 280;
config.height = 160;
config.min_value = 0;
config.max_value = 70;  // 0-70°C
config.unit = "°C";
config.arc_color_start = lv_color_hex(0x4299E1);  // Bleu (froid)
config.arc_color_end = lv_color_hex(0xED8936);    // Orange (chaud)

// Créer la jauge
widget_gauge_semicircular_t *temps =
    widget_gauge_semicircular_create(parent, &config);

// Ajouter des aiguilles pour chaque capteur
int s1 = widget_gauge_semicircular_add_needle(temps, "S1",
                                              lv_color_hex(0xFF1493), 40.0);
int s2 = widget_gauge_semicircular_add_needle(temps, "S2",
                                              lv_color_hex(0x00D9FF), 25.0);
int internal = widget_gauge_semicircular_add_needle(temps, "Int",
                                                    lv_color_hex(0xFFA500), 55.0);

// Mettre à jour les valeurs
widget_gauge_semicircular_set_needle_value(temps, s1, 42.5);
widget_gauge_semicircular_set_needle_value(temps, s2, 28.0);
widget_gauge_semicircular_set_needle_value(temps, internal, 58.3);

// Détruire quand terminé
widget_gauge_semicircular_destroy(temps);
```

### Configuration détaillée

```c
typedef struct {
    lv_coord_t width;         // Largeur (défaut: 280)
    lv_coord_t height;        // Hauteur (défaut: 180)
    float min_value;          // Valeur min (ex: 0)
    float max_value;          // Valeur max (ex: 100)
    const char *title;        // Titre (NULL = pas de titre)
    const char *unit;         // Unité (ex: "°C", "%")
    uint16_t arc_width;       // Largeur de l'arc (défaut: 12)
    lv_color_t arc_color_start; // Couleur début du dégradé
    lv_color_t arc_color_end;   // Couleur fin du dégradé
    bool show_gradient;       // Afficher le dégradé
    bool animate;             // Activer les animations
    uint16_t anim_duration;   // Durée animation ms (défaut: 500)
} widget_gauge_semicircular_config_t;
```

### API Complète

#### `widget_gauge_semicircular_create()`
```c
widget_gauge_semicircular_t* widget_gauge_semicircular_create(
    lv_obj_t *parent,
    const widget_gauge_semicircular_config_t *config);
```
Crée une nouvelle jauge semi-circulaire.

#### `widget_gauge_semicircular_add_needle()`
```c
int widget_gauge_semicircular_add_needle(
    widget_gauge_semicircular_t *gauge,
    const char *name,
    lv_color_t color,
    float initial_value);
```
Ajoute une aiguille. Retourne l'index de l'aiguille (0-3) ou -1 si erreur.

#### `widget_gauge_semicircular_set_needle_value()`
```c
void widget_gauge_semicircular_set_needle_value(
    widget_gauge_semicircular_t *gauge,
    uint8_t needle_index,
    float value);
```
Met à jour la valeur d'une aiguille spécifique.

#### `widget_gauge_semicircular_set_title()`
```c
void widget_gauge_semicircular_set_title(
    widget_gauge_semicircular_t *gauge,
    const char *title);
```
Change le titre de la jauge.

#### `widget_gauge_semicircular_destroy()`
```c
void widget_gauge_semicircular_destroy(widget_gauge_semicircular_t *gauge);
```
Détruit la jauge et libère la mémoire.

### Exemples d'utilisation

#### Jauge SOC et SOH

```c
widget_gauge_semicircular_config_t config = WIDGET_GAUGE_SEMICIRCULAR_DEFAULT_CONFIG;
config.width = 260;
config.height = 150;
config.min_value = 20;  // Commence à 20%
config.max_value = 100;
config.unit = "%";
config.arc_color_start = lv_color_hex(0x4299E1);
config.arc_color_end = lv_color_hex(0x38A169);

widget_gauge_semicircular_t *battery =
    widget_gauge_semicircular_create(parent, &config);

// SOC (State of Charge) - Vert
int soc_needle = widget_gauge_semicircular_add_needle(battery, "SOC",
                                                      lv_color_hex(0x38A169), 80);

// SOH (State of Health) - Bleu
int soh_needle = widget_gauge_semicircular_add_needle(battery, "SOH",
                                                      lv_color_hex(0x4299E1), 95);
```

#### Jauge multi-températures

```c
widget_gauge_semicircular_config_t config = WIDGET_GAUGE_SEMICIRCULAR_DEFAULT_CONFIG;
config.width = 280;
config.height = 160;
config.min_value = -20;  // -20°C à 80°C
config.max_value = 80;
config.unit = "°C";
config.title = "TEMPERATURES";

widget_gauge_semicircular_t *temps =
    widget_gauge_semicircular_create(parent, &config);

// Capteur 1 - Cyan
int t1 = widget_gauge_semicircular_add_needle(temps, "T1",
                                              lv_color_hex(0x00D9FF), 25);

// Capteur 2 - Magenta
int t2 = widget_gauge_semicircular_add_needle(temps, "T2",
                                              lv_color_hex(0xFF1493), 40);

// Capteur 3 - Orange
int t3 = widget_gauge_semicircular_add_needle(temps, "T3",
                                              lv_color_hex(0xFFA500), 55);

// Capteur 4 - Vert
int t4 = widget_gauge_semicircular_add_needle(temps, "T4",
                                              lv_color_hex(0x38A169), 30);
```

## 3. Exemple Complet : Dashboard BMS

Un exemple complet reproduisant l'interface de l'image est disponible dans :
`components/gui_lvgl/widgets/examples/dashboard_gauges_example.c`

### Architecture du dashboard

```
┌─────────────────────────────────────────────────────────────────────┐
│                         DASHBOARD BMS                               │
├──────────────────┬──────────────────────────┬────────────────────────┤
│ BATTERY STATUS   │   BATTERY MONITOR        │   TEMPERATURES         │
│                  │                          │                        │
│  ┌───────────┐  │  ┌─────┐     ┌─────┐    │      ┌───────────┐    │
│  │  SOC/SOH  │  │  │ VOL │ PWR │ CUR │    │      │ S2/S1/Int │    │
│  │   Gauge   │  │  │ AGE │     │RENT │    │      │   Gauge   │    │
│  │  (semi)   │  │  │ (○) │     │ (○) │    │      │   (semi)  │    │
│  └───────────┘  │  └─────┘     └─────┘    │      └───────────┘    │
│  SOC 80%        │    50V    0W    0A       │  S2:25° S1:40° Int:55° │
│  SOH 95%        │                          │                        │
└──────────────────┴──────────────────────────┴────────────────────────┘
```

### Code d'intégration

```c
#include "dashboard_gauges_example.h"

void screen_dashboard_create(lv_obj_t *parent) {
    // Créer le dashboard
    dashboard_gauges_create(parent);
}

void screen_dashboard_update(battery_status_t *status) {
    // Mettre à jour avec les vraies données
    dashboard_gauges_update(
        status->soc,           // State of Charge
        status->soh,           // State of Health
        status->voltage,       // Voltage
        status->current,       // Current
        status->temp_sensor2,  // Temperature S2
        status->temp_sensor1,  // Temperature S1
        status->temp_internal  // Temperature interne
    );
}
```

## Intégration dans le projet

### 1. CMakeLists.txt

Les widgets sont déjà ajoutés dans :
- `components/gui_lvgl/widgets/CMakeLists.txt`
- `components/gui_lvgl/CMakeLists.txt`

### 2. Includes

```c
#include "widget_gauge_circular.h"
#include "widget_gauge_semicircular.h"
```

### 3. Utilisation dans un écran

```c
// Dans screen_dashboard.cpp

static widget_gauge_circular_t *voltage_gauge = NULL;
static widget_gauge_semicircular_t *temp_gauge = NULL;

void screen_dashboard_create(lv_obj_t *parent) {
    // Créer les jauges
    widget_gauge_circular_config_t cfg = WIDGET_GAUGE_CIRCULAR_DEFAULT_CONFIG;
    cfg.min_value = 0;
    cfg.max_value = 60;
    cfg.unit = "V";
    voltage_gauge = widget_gauge_circular_create(parent, &cfg);

    // ... créer les autres jauges
}

void screen_dashboard_update(float voltage, float temp) {
    widget_gauge_circular_set_value(voltage_gauge, voltage);
    widget_gauge_semicircular_set_needle_value(temp_gauge, 0, temp);
}

void screen_dashboard_destroy(void) {
    if (voltage_gauge) {
        widget_gauge_circular_destroy(voltage_gauge);
        voltage_gauge = NULL;
    }
    if (temp_gauge) {
        widget_gauge_semicircular_destroy(temp_gauge);
        temp_gauge = NULL;
    }
}
```

## Performance

### Empreinte mémoire

| Widget | Structure | Objets LVGL | Total estimé |
|--------|-----------|-------------|--------------|
| Gauge circulaire | ~80 bytes | ~800 bytes | ~1 KB |
| Gauge semi-circulaire | ~120 bytes | ~1.2 KB | ~1.5 KB |
| **Dashboard complet** | | | **~8 KB** |

### Optimisations

- **Animations** : Désactiver si CPU chargé (`animate = false`)
- **Fréquence de mise à jour** : Limiter à 1-2 Hz pour les valeurs lentes
- **Destruction** : Toujours appeler `destroy()` pour libérer la mémoire

## Personnalisation avancée

### Couleurs des aiguilles

```c
// Couleurs recommandées pour BMS
#define COLOR_VOLTAGE    lv_color_hex(0x4299E1)  // Bleu
#define COLOR_CURRENT    lv_color_hex(0x38A169)  // Vert
#define COLOR_TEMP_COLD  lv_color_hex(0x00D9FF)  // Cyan
#define COLOR_TEMP_WARM  lv_color_hex(0xFF1493)  // Magenta
#define COLOR_TEMP_HOT   lv_color_hex(0xFFA500)  // Orange
#define COLOR_SOC        lv_color_hex(0x38A169)  // Vert
#define COLOR_SOH        lv_color_hex(0x4299E1)  // Bleu
```

### Thèmes Dark/Light

Les widgets s'adaptent automatiquement au thème LVGL actuel. Vous pouvez personnaliser :

```c
// Pour thème sombre
config.needle_color = lv_color_hex(0x4299E1);
config.scale_color = lv_color_hex(0x4A5568);

// Pour thème clair
config.needle_color = lv_color_hex(0x3070B3);
config.scale_color = lv_color_hex(0xE2E8F0);
```

## Dépannage

### L'aiguille ne bouge pas

**Cause** : Animations désactivées ou valeur hors limites

**Solution** :
```c
config.animate = true;
// Vérifier que la valeur est entre min et max
if (value < config.min_value) value = config.min_value;
if (value > config.max_value) value = config.max_value;
```

### Les aiguilles se superposent

**Cause** : Valeurs proches sur la jauge semi-circulaire

**Solution** : Utiliser des couleurs contrastées et espacer visuellement
```c
// Aiguille 1 - épaisse
lv_obj_set_style_line_width(gauge->needles[0], 4, 0);
// Aiguille 2 - fine
lv_obj_set_style_line_width(gauge->needles[1], 2, 0);
```

### Labels mal positionnés

**Cause** : Taille du container inadaptée

**Solution** : Ajuster les tailles selon votre layout
```c
config.width = 300;   // Augmenter si nécessaire
config.height = 180;  // Ajuster selon l'espace disponible
```

## Références

- **Code source** : `components/gui_lvgl/widgets/src/widget_gauge_*.c`
- **Headers** : `components/gui_lvgl/widgets/include/widget_gauge_*.h`
- **Exemple complet** : `components/gui_lvgl/widgets/examples/dashboard_gauges_example.c`
- **Documentation LVGL** : https://docs.lvgl.io/8.3/widgets/arc.html

---

**Dernière mise à jour** : Novembre 2025
**Version** : 1.0
**Auteur** : Développement ESP32-P4 HMI
