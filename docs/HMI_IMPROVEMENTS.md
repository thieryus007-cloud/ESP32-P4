# Améliorations de l'Interface HMI ESP32-P4

Ce document décrit les améliorations apportées à l'interface HMI du projet ESP32-P4 conformément aux spécifications du document `ESP32-P4_HMI_Specifications.docx`.

## Vue d'ensemble

Les améliorations incluent :

1. **Widgets Réutilisables** - Composants UI modulaires et cohérents
2. **Système d'Historique Amélioré** - Stockage et visualisation des données historiques
3. **Système de Thèmes Avancé** - Support complet Dark/Light avec persistance NVS

## 1. Widgets Réutilisables

### Architecture

Les widgets sont situés dans `components/gui_lvgl/widgets/` :

```
widgets/
├── include/
│   ├── widget_soc_gauge.h
│   ├── widget_status_indicator.h
│   ├── widget_cell_bar.h
│   └── widget_value_display.h
└── src/
    ├── widget_soc_gauge.c
    ├── widget_status_indicator.c
    ├── widget_cell_bar.c
    └── widget_value_display.c
```

### Widget SOC Gauge

Affiche l'état de charge (SOC) sous forme d'arc circulaire avec animation.

**Fonctionnalités :**
- Arc de progression animé (0-100%)
- Couleurs dynamiques selon les seuils (rouge < 20%, orange 20-80%, vert > 80%)
- Indicateur de tendance (charge/décharge/stable)
- Configuration complète (taille, épaisseur, couleurs)

**Exemple d'utilisation :**

```c
#include "widget_soc_gauge.h"

// Configuration personnalisée (optionnel)
widget_soc_gauge_config_t config = WIDGET_SOC_GAUGE_DEFAULT_CONFIG;
config.width = 250;
config.height = 250;
config.show_trend = true;

// Créer le widget
widget_soc_gauge_t *gauge = widget_soc_gauge_create(parent, &config);

// Mettre à jour la valeur
widget_soc_gauge_set_value(gauge, 85, 1); // 85%, en charge

// Détruire le widget
widget_soc_gauge_destroy(gauge);
```

### Widget Status Indicator

Indicateur d'état avec LED colorée et label.

**États disponibles :**
- `STATUS_INACTIVE` - Gris (inactif)
- `STATUS_ERROR` - Rouge (erreur)
- `STATUS_WARNING` - Orange (avertissement)
- `STATUS_OK` - Vert (fonctionnel)

**Exemple d'utilisation :**

```c
#include "widget_status_indicator.h"

widget_status_config_t config = WIDGET_STATUS_DEFAULT_CONFIG;
config.label_text = "BMS";
config.horizontal = true;

widget_status_indicator_t *status = widget_status_create(parent, &config);
widget_status_set_state(status, STATUS_OK);
```

### Widget Cell Bar

Barre de visualisation pour une cellule de batterie avec tension et équilibrage.

**Fonctionnalités :**
- Barre de progression colorée selon les seuils
- Affichage de la tension en volts
- Icône d'équilibrage animée
- Seuils configurables (min, max, low, high)

**Exemple d'utilisation :**

```c
#include "widget_cell_bar.h"

widget_cell_bar_config_t config = WIDGET_CELL_BAR_DEFAULT_CONFIG;
config.min_voltage = 2800;  // 2.8V
config.max_voltage = 4200;  // 4.2V
config.low_threshold = 3000;
config.high_threshold = 4100;

widget_cell_bar_t *bar = widget_cell_bar_create(parent, 1, &config); // Cellule 1
widget_cell_bar_set_voltage(bar, 3700, false); // 3.7V, pas d'équilibrage
```

### Widget Value Display

Affichage générique de valeur avec titre, valeur et unité.

**Exemple d'utilisation :**

```c
#include "widget_value_display.h"

widget_value_config_t config = WIDGET_VALUE_DEFAULT_CONFIG;
config.title = "Tension";
config.unit = "V";
config.format = "%.2f";

widget_value_display_t *display = widget_value_create(parent, &config);
widget_value_set_float(display, 51.23); // Affiche "51.23 V"
```

## 2. Système d'Historique Amélioré

### Architecture

Le gestionnaire d'historique est implémenté dans `components/history_manager/` :

```
history_manager/
├── include/
│   ├── history_data.h      // Structures de données
│   └── history_manager.h   // API publique
└── history_manager.c       // Implémentation
```

### Fonctionnalités

- **Ring buffers multi-périodes** : 1min, 1h, 24h, 7 jours
- **Stockage en PSRAM** : Utilisation optimale de la mémoire
- **Persistance SPIFFS** : Sauvegarde/chargement automatique
- **Export CSV** : Génération de fichiers CSV pour analyse

### Structure des données

```c
typedef struct {
    uint32_t timestamp;     // Unix timestamp
    int16_t voltage_cv;     // Tension en centivolts
    int16_t current_ca;     // Courant en centi-ampères
    uint8_t soc;            // SOC 0-100
    int8_t temperature;     // Température -128 à +127°C
    uint16_t cell_min_mv;   // Tension cell min
    uint16_t cell_max_mv;   // Tension cell max
} history_point_t;
```

### Utilisation

```c
#include "history_manager.h"

// Initialisation
esp_err_t ret = history_manager_init();

// Ajouter un point de données
history_point_t point = {
    .timestamp = (uint32_t)time(NULL),
    .voltage_cv = 5123,  // 51.23V
    .current_ca = 1050,  // 10.50A
    .soc = 85,
    .temperature = 25,
    .cell_min_mv = 3650,
    .cell_max_mv = 3720
};
history_add_point(&point);

// Récupérer les données
history_point_t points[360];
uint32_t count = history_get_points(HISTORY_1H, points, 360);

// Exporter en CSV
history_export_csv(HISTORY_24H, "/spiffs/history.csv");

// Sauvegarder/charger
history_save_to_flash();
history_load_from_flash();
```

### Capacités mémoire

| Buffer   | Points | Intervalle | Mémoire  |
|----------|--------|------------|----------|
| 1 minute | 60     | 1s         | 960 B    |
| 1 heure  | 360    | 10s        | 5.6 KB   |
| 24 heures| 1440   | 1min       | 22.5 KB  |
| 7 jours  | 2016   | 5min       | 31.5 KB  |
| **TOTAL**| 3876   | -          | **~61 KB** |

## 3. Système de Thèmes Avancé

### Fonctionnalités

- **Trois modes** : Auto, Clair, Sombre
- **Palette complète** : 16 couleurs définies par thème
- **Persistance NVS** : Sauvegarde automatique du choix
- **Basculement horaire** : Mode automatique selon l'heure
- **API complète** : Accès à toutes les couleurs de la palette

### Palettes de couleurs

#### Thème Sombre
```c
bg_primary:     #1A202C  // Fond principal
bg_secondary:   #2D3748  // Cartes
text_primary:   #F7FAFC  // Texte principal
accent_primary: #4299E1  // Bleu
accent_success: #38A169  // Vert
accent_warning: #ED8936  // Orange
accent_error:   #E53E3E  // Rouge
```

#### Thème Clair
```c
bg_primary:     #F7FAFC  // Fond principal
bg_secondary:   #FFFFFF  // Cartes
text_primary:   #1A202C  // Texte principal
accent_primary: #3070B3  // Bleu
accent_success: #2F855A  // Vert
accent_warning: #C05F21  // Orange
accent_error:   #C52A2A  // Rouge
```

### Utilisation

```c
#include "ui_theme.h"

// Initialisation (charge depuis NVS)
ui_theme_init(disp);

// Changer de mode (sauvegarde en NVS)
ui_theme_set_mode(UI_THEME_MODE_DARK);

// Mode automatique horaire
ui_theme_set_auto(true, 19, 7); // Sombre à 19h, Clair à 7h

// Basculer entre les modes
ui_theme_toggle();

// Récupérer la palette actuelle
const theme_palette_t *palette = ui_theme_get_palette();
lv_obj_set_style_bg_color(obj, palette->bg_secondary, 0);
```

### Menu rapide de thème

Un widget de menu déroulant est disponible pour permettre à l'utilisateur de changer le thème :

```c
lv_obj_t *menu = ui_theme_create_quick_menu(parent);
lv_obj_align(menu, LV_ALIGN_TOP_RIGHT, -8, 8);
```

## Intégration dans le projet

### Modifications apportées

1. **CMakeLists.txt** mis à jour pour inclure :
   - Les fichiers widgets
   - La dépendance `nvs_flash`
   - Les chemins d'inclusion des widgets

2. **Composants ajoutés** :
   - `history_manager/` - Gestionnaire d'historique
   - `gui_lvgl/widgets/` - Widgets réutilisables

3. **Fichiers modifiés** :
   - `ui_theme.h` - API étendue
   - `ui_theme.cpp` - Implémentation complète avec NVS

### Compilation

Le projet se compile normalement avec ESP-IDF :

```bash
idf.py build
```

### Tests recommandés

1. **Widgets** :
   - Créer un écran de test affichant tous les widgets
   - Vérifier les animations et les mises à jour
   - Tester la destruction sans fuites mémoire

2. **Historique** :
   - Ajouter des points de données en continu
   - Vérifier la persistance après redémarrage
   - Tester l'export CSV

3. **Thèmes** :
   - Basculer entre les modes
   - Vérifier la persistance après redémarrage
   - Tester le mode automatique horaire

## Notes techniques

### Gestion de la mémoire

- Les widgets utilisent `lv_malloc`/`lv_free` de LVGL
- Le history_manager utilise PSRAM en priorité avec fallback sur SRAM
- Tous les widgets ont une fonction `destroy()` pour libérer la mémoire

### Thread-safety

- Le history_manager utilise un mutex FreeRTOS
- Les widgets LVGL doivent être manipulés dans le thread LVGL

### Performance

- Les animations des widgets sont optimisées (path ease-out)
- Le ring buffer évite les allocations dynamiques
- Les couleurs sont pré-calculées dans les palettes

## Futures améliorations possibles

1. **Widgets** :
   - Widget de graphique en temps réel
   - Widget de liste scrollable optimisée
   - Widget de clavier virtuel

2. **Historique** :
   - Compression des données anciennes
   - Synchronisation cloud
   - Alertes basées sur l'historique

3. **Thèmes** :
   - Thèmes personnalisables par l'utilisateur
   - Thèmes saisonniers
   - Import/export de thèmes

## Références

- Document de spécifications : `ESP32-P4_HMI_Specifications.docx`
- Documentation LVGL : https://docs.lvgl.io/
- ESP-IDF : https://docs.espressif.com/projects/esp-idf/

---

**Date de mise en œuvre** : Novembre 2025
**Version** : 1.0
**Auteur** : Développement ESP32-P4 HMI
