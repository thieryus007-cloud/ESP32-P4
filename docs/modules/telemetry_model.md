# telemetry_model

## Rôle
Modèle local de télémétrie. Il s'abonne aux événements issus de TinyBMS ou du backend et calcule des structures dérivées (`battery_status_t`, `pack_stats_t`) pour la GUI et les exporteurs.

## Entrées
- EventBus fourni à l'initialisation via `telemetry_model_init(event_bus_t *bus)`.
- Événements de télémétrie (TinyBMS ou S3) publiés sur l'EventBus.

## Sorties
- Publication des mises à jour de statut batterie et pack (`EVENT_BATTERY_STATUS_UPDATED`, `EVENT_PACK_STATS_UPDATED`).

## Place dans le flux
`telemetry_model` consolide les mesures brutes (UART ou WebSocket) et expose des vues prêtes à afficher. Il sert de source unique pour la GUI et les modules d'export (MQTT/HTTP, CAN) afin d'assurer la cohérence des données.
