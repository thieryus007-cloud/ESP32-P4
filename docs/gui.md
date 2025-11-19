# Architecture GUI LVGL

La couche `components/gui_lvgl/` combine encore des fichiers C hérités et de
nouvelles briques C++. Pour stabiliser la transition, chaque `screen_*.h`
expose désormais :

1. Une API C historique protégée par des gardes `extern "C"` pour les
   composants (ou tests) qui continuent d'inclure les en-têtes depuis du code C.
2. Une classe C++ légère (`gui::ScreenBattery`, `gui::ScreenAlerts`, etc.) qui
   encapsule l'API C et fournit des méthodes orientées objet.

## Exigence C++17

Le composant est maintenant compilé avec `target_compile_features(... cxx_std_17)`
car les wrappers utilisent `std::unique_ptr`, `std::make_unique` et les lambdas
capturant `this`. L'ensemble de la GUI requiert donc un toolchain ESP-IDF
configuré avec le support C++17 (défaut sur ESP-IDF v5+).

## Exemple d'utilisation

```cpp
lv_obj_t *tab_power = lv_tabview_add_tab(tabview, "Power");
auto screen_power = std::make_unique<gui::ScreenPower>(tab_power);

battery_status_t status = {};
status.soc      = 82.5f;
status.voltage  = 52.1f;
status.current  = -4.7f;
status.power    = -245.0f;
status.temperature = 26.0f;

screen_power->update(status);
```

## Chemins C existants

Les fonctions C demeurent disponibles pendant la migration afin de limiter les
refactors massifs. Chaque classe se contente d'appeler la fonction C
correspondante ; il n'y a donc pas de double source de vérité. Une fois les
composants convertis en C++, les fonctions C pourront être supprimées et les
classes prendront directement en charge la logique LVGL.

