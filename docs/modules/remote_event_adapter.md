# remote_event_adapter

## Rôle
Adaptateur entre les flux JSON reçus du backend S3 et les événements internes du firmware. Il convertit télémétrie, événements système, alertes et réponses HTTP en structures typées publiées sur l'EventBus, et relaie aussi les commandes utilisateur vers le backend.

## Entrées
- JSON bruts provenant de `/ws/telemetry`, `/ws/events`, `/ws/alerts` via les callbacks `remote_event_adapter_on_*` (appelées par `net_client`).
- Statut MQTT JSON via `remote_event_adapter_on_mqtt_status_json()`.
- Réponses HTTP issues de `net_client_send_http_request()` via `remote_event_adapter_on_http_response()`.
- Mode de fonctionnement via `remote_event_adapter_set_operation_mode()` pour savoir si la normalisation réseau est active.

## Sorties
- Publication d'événements métiers sur l'EventBus :
  - Télémétrie : `EVENT_REMOTE_TELEMETRY_UPDATE`.
  - Événements système : `EVENT_REMOTE_SYSTEM_EVENT` (logique UI/monitoring).
  - Alertes : `EVENT_ALERTS_RECEIVED`, `EVENT_ALERTS_ACKED`, etc.
  - Statut MQTT : `EVENT_REMOTE_MQTT_STATUS_UPDATED`.
  - Réponses HTTP normalisées avec corrélation : `EVENT_HTTP_RESPONSE_RECEIVED`.
- Signalement d'un retour en ligne via `remote_event_adapter_on_network_online()` (publie un système OK).

## Fonctionnement détaillé
1. **Initialisation** : `remote_event_adapter_init` stocke l'EventBus et prépare les structures de parsing JSON (généralement via cJSON/rapidjson selon implémentation). Il ne crée pas de tâche dédiée.
2. **Adaptation WebSocket** : chaque callback `remote_event_adapter_on_{telemetry,events,alerts}` reçoit un buffer JSON et le convertit en structures spécifiques (télémétrie batterie, événements de journal, liste d'alertes). Les événements typés sont ensuite publiés pour consommation par `telemetry_model`, `system_events_model`, GUI, etc.
3. **Adaptation HTTP** : `remote_event_adapter_on_http_response` associe la réponse à son `correlation_id`, parse le payload et publie un événement incluant le code HTTP et le contenu structuré. Cela centralise l'erreur/succès réseau pour la GUI et les modèles.
4. **Statut MQTT** : `remote_event_adapter_on_mqtt_status_json` convertit le JSON en structure de statut (connecté, topics actifs, latence éventuelle) et publie l'événement correspondant.
5. **Mode de fonctionnement** : si `telemetry_expected` est faux (mode autonome), les callbacks peuvent ignorer ou limiter le traitement pour éviter de polluer le bus avec des données réseau obsolètes.
6. **Robustesse** : en cas de parsing invalide, le module peut publier un événement d'erreur système ou simplement logguer l'anomalie ; la chaîne de flux reste ainsi résiliente aux payloads inattendus.

## Place dans le flux
`remote_event_adapter` est la passerelle de normalisation entre le monde réseau (JSON) et les structures internes. Il alimente les modèles et la GUI avec des événements typés et transporte les commandes utilisateur vers le backend via `net_client`. Sans lui, les modules métiers devraient gérer le parsing JSON eux-mêmes, ce qui casserait le découplage.
- JSON bruts provenant de `/ws/telemetry`, `/ws/events`, `/ws/alerts` via les callbacks `remote_event_adapter_on_*`.
- Statut MQTT JSON via `remote_event_adapter_on_mqtt_status_json()`.
- Réponses HTTP issues de `net_client_send_http_request()` via `remote_event_adapter_on_http_response()`.
- Mode de fonctionnement via `remote_event_adapter_set_operation_mode()`.

## Sorties
- Publication d'événements métiers (`EVENT_REMOTE_TELEMETRY_UPDATE`, `EVENT_REMOTE_SYSTEM_EVENT`, `EVENT_ALERTS_*`, etc.) sur l'EventBus pour consommation par les modèles (`telemetry_model`, `system_events_model`, GUI).
- Signalement d'un retour en ligne via `remote_event_adapter_on_network_online()`.

## Place dans le flux
`remote_event_adapter` est la passerelle de normalisation entre le monde réseau (JSON) et les structures internes. Il alimente les modèles et la GUI avec des événements typés et transporte les commandes utilisateur vers le backend via `net_client`.
