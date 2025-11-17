# remote_event_adapter

## Rôle
Adaptateur entre les flux JSON reçus du backend S3 et les événements internes du firmware. Il convertit télémétrie, événements système, alertes et réponses HTTP en structures typées publiées sur l'EventBus, et relaie aussi les commandes utilisateur vers le backend.

## Entrées
- JSON bruts provenant de `/ws/telemetry`, `/ws/events`, `/ws/alerts` via les callbacks `remote_event_adapter_on_*`.
- Statut MQTT JSON via `remote_event_adapter_on_mqtt_status_json()`.
- Réponses HTTP issues de `net_client_send_http_request()` via `remote_event_adapter_on_http_response()`.
- Mode de fonctionnement via `remote_event_adapter_set_operation_mode()`.

## Sorties
- Publication d'événements métiers (`EVENT_REMOTE_TELEMETRY_UPDATE`, `EVENT_REMOTE_SYSTEM_EVENT`, `EVENT_ALERTS_*`, etc.) sur l'EventBus pour consommation par les modèles (`telemetry_model`, `system_events_model`, GUI).
- Signalement d'un retour en ligne via `remote_event_adapter_on_network_online()`.

## Place dans le flux
`remote_event_adapter` est la passerelle de normalisation entre le monde réseau (JSON) et les structures internes. Il alimente les modèles et la GUI avec des événements typés et transporte les commandes utilisateur vers le backend via `net_client`.
