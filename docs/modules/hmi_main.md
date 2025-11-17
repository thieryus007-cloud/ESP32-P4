# hmi_main.c

## Rôle
Module d'orchestration central. Il instancie l'EventBus, charge le mode de fonctionnement (connecté S3 ou autonome TinyBMS), initialise les modules de communication, de modèles métiers, d'agrégation et la GUI. Il gère la bascule de mode et déclenche le démarrage différé des composants réseau.

## Entrées
- Mode de fonctionnement persistant chargé via `operation_mode_init()`.
- Événements utilisateur `EVENT_USER_INPUT_CHANGE_MODE` pour changer de mode.
- Événements `EVENT_NETWORK_FAILOVER_ACTIVATED` pour réagir aux bascules réseau.

## Sorties
- Publication des événements `EVENT_OPERATION_MODE_CHANGED` et `EVENT_SYSTEM_STATUS_UPDATED` lors du démarrage ou des changements de mode.
- Initialisation et démarrage conditionnel de `net_client` et `remote_event_adapter` lorsque la télémétrie réseau est attendue.
- Démarrage des modèles (`telemetry_model`, `system_events_model`, `config_model`, `history_model`, `stats_aggregator`, `network_publisher`, `status_endpoint`) et des modules TinyBMS (`tinybms_client`, `tinybms_model`).
- Démarrage de la GUI via `gui_init()`/`gui_start()`.

## Place dans le flux
`hmi_main` relie la configuration persistante, l'infrastructure EventBus et les modules fonctionnels. Il conditionne tout le reste du pipeline (réseau, UART TinyBMS, modèles, GUI) en fonction du mode courant et fournit les premiers événements de statut consommés par la suite de la chaîne.
