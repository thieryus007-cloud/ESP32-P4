# net_client

## Rôle
Client réseau responsable de la connexion Wi-Fi et de l'ouverture des WebSockets/HTTP vers le backend S3. Il agit comme point d'entrée pour la télémétrie distante et les commandes REST.

## Entrées
- Configuration réseau (`ssid`, `password`, `bridge_host`, `bridge_port`).
- Mode de fonctionnement via `net_client_set_operation_mode()` pour savoir si la télémétrie réseau est attendue.
- Messages JSON ou requêtes à envoyer via `net_client_send_command_ws()` et `net_client_send_http_request()`.

## Sorties
- Ouverture des flux `/ws/telemetry`, `/ws/events`, `/ws/alerts` et diffusion des données reçues vers `remote_event_adapter`.
- Publication d'événements de statut réseau (début/fin de requête, résultats) via l'EventBus.

## Place dans le flux
`net_client` est l'interface réseau côté ESP32. Il fournit les flux bruts JSON au `remote_event_adapter` et signale l'état de connectivité aux modèles système, déclenchant ainsi la mise à jour de la GUI et d'autres modules dépendants du backend.
