# hmi_main.c

## Rôle
Module d'orchestration central. Il instancie l'EventBus, charge le mode de fonctionnement (connecté S3 ou autonome TinyBMS), initialise les modèles métiers, la pile réseau, l'UART TinyBMS et la GUI. Il gère la bascule de mode et déclenche le démarrage différé des composants réseau.

## Entrées
- Mode de fonctionnement persistant chargé via `operation_mode_init()` / `operation_mode_get()`.
- Événements utilisateur `EVENT_USER_INPUT_CHANGE_MODE` pour changer de mode (par ex. via la GUI ou une entrée physique).
Module d'orchestration central. Il instancie l'EventBus, charge le mode de fonctionnement (connecté S3 ou autonome TinyBMS), initialise les modules de communication, de modèles métiers, d'agrégation et la GUI. Il gère la bascule de mode et déclenche le démarrage différé des composants réseau.

## Entrées
- Mode de fonctionnement persistant chargé via `operation_mode_init()`.
- Événements utilisateur `EVENT_USER_INPUT_CHANGE_MODE` pour changer de mode.
- Événements `EVENT_NETWORK_FAILOVER_ACTIVATED` pour réagir aux bascules réseau.

## Sorties
- Publication des événements `EVENT_OPERATION_MODE_CHANGED` et `EVENT_SYSTEM_STATUS_UPDATED` lors du démarrage ou des changements de mode.
- Initialisation/démarrage conditionnel de `net_client` et `remote_event_adapter` lorsque la télémétrie réseau est attendue.
- Démarrage des modèles (`telemetry_model`, `system_events_model`, `config_model`, `history_model`, `stats_aggregator`, `network_publisher`, `status_endpoint`) et des modules TinyBMS (`tinybms_client`, `tinybms_model`).
- Démarrage de la GUI via `gui_init()` / `gui_start()`.

## Fonctionnement détaillé
1. **Création de l'EventBus** : `event_bus_init` sur l'instance globale partagée par tous les modules.
2. **Chargement configuration/mode** : initialise `config_manager` et récupère le mode courant pour savoir si la télémétrie distante doit être attendue.
3. **Initialisation conditionnelle du réseau** : si le mode connecté est actif, appelle `net_client_init` et `remote_event_adapter_init` et conserve des flags (`s_remote_initialized`, `s_remote_started`) pour différer le start réel.
4. **Initialisation des modèles** : instancie les modèles métier et exporteurs (télémétrie, événements système, historique, stats, publication réseau, endpoint de statut) ainsi que la pile TinyBMS (client UART + modèle de registres).
5. **GUI** : appelle `gui_init` avec l'EventBus afin que les écrans puissent publier ou consommer des événements (changement de mode, affichage des stats).
6. **Souscriptions de contrôle** : s'abonne aux événements de changement de mode et de failover réseau pour relancer ou arrêter les modules distants.
7. **Start** : `hmi_main_start` déclenche la création des tasks internes si nécessaire (`hmi_create_core_tasks` placeholder), démarre les modules via leurs fonctions `*_start`, puis appelle `ensure_remote_modules_started` pour ouvrir la connectivité Wi-Fi/WebSocket si attendue.
8. **Publication d'état** : `publish_operation_mode_state` construit un `system_status_t` cohérent (connectivité, mode courant, `telemetry_expected`) et le publie afin que la GUI et les exporteurs reflètent immédiatement l'état.
9. **Bascule de mode** : `handle_user_change_mode` persiste le nouveau mode, met à jour l'état interne et relance (ou arrête) les modules réseau via `ensure_remote_modules_started`; en mode autonome, seuls TinyBMS et les modèles locaux restent actifs.
- Initialisation et démarrage conditionnel de `net_client` et `remote_event_adapter` lorsque la télémétrie réseau est attendue.
- Démarrage des modèles (`telemetry_model`, `system_events_model`, `config_model`, `history_model`, `stats_aggregator`, `network_publisher`, `status_endpoint`) et des modules TinyBMS (`tinybms_client`, `tinybms_model`).
- Démarrage de la GUI via `gui_init()`/`gui_start()`.

## Place dans le flux
`hmi_main` relie la configuration persistante, l'infrastructure EventBus et les modules fonctionnels. Il conditionne tout le reste du pipeline (réseau, UART TinyBMS, modèles, GUI) en fonction du mode courant et fournit les premiers événements de statut consommés par la suite de la chaîne.
