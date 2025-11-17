# tinybms_model

## Rôle
Modèle haut niveau des registres TinyBMS. Il maintient un cache cohérent, valide les écritures, orchestre les lectures complètes ou ciblées et publie les événements de mise à jour.

## Entrées
- EventBus fourni via `tinybms_model_init(event_bus_t *bus)`.
- Lectures/écritures déclenchées par la GUI ou des tâches internes (`tinybms_model_read_all`, `tinybms_model_read_register`, `tinybms_model_write_register`).

## Sorties
- Publication des événements `EVENT_TINYBMS_REGISTER_UPDATED`, `EVENT_TINYBMS_CONFIG_CHANGED` et alertes associées sur l'EventBus.
- Mise à disposition d'un snapshot de configuration via `tinybms_model_get_config()` et de métriques de cache (`tinybms_model_get_stats`).

## Place dans le flux
`tinybms_model` transforme les lectures brutes UART en données structurées prêtes à afficher ou exporter. Il joue le rôle de tampon cohérent entre le client UART et les consommateurs (GUI, `telemetry_model`, export MQTT/HTTP).
