# net_client

## Rôle
Client réseau responsable de la connexion Wi-Fi station, de l'ouverture des WebSockets/HTTP vers le backend S3 et du suivi d'état réseau. Il sert de point d'entrée pour la télémétrie distante (channels `telemetry`, `events`, `alerts`) et l'envoi de commandes HTTP/WS.

## Entrées
- Configuration réseau (`ssid`, `password`, `bridge_host`, `bridge_port`) compilée via `menuconfig`.
- Mode de fonctionnement via `net_client_set_operation_mode(mode, telemetry_expected)` pour activer/désactiver le réseau selon le mode HMI.
- Demandes d'envoi de commandes :
  - WebSocket : `net_client_send_command_ws(channel, payload, len)`.
  - HTTP : `net_client_send_http_request(method, path, body, len, correlation_id)`.
- Événements TinyBMS d'alerte (`EVENT_TINYBMS_ALERT_TRIGGERED/RECOVERED`) captés pour publication réseau.

## Sorties
- Ouverture des flux `/ws/telemetry`, `/ws/events`, `/ws/alerts` et diffusion des données reçues vers `remote_event_adapter_on_*`.
- Publication d'événements système sur l'EventBus :
  - `EVENT_SYSTEM_STATUS_UPDATED` après chaque changement Wi-Fi/IP ou reachabilité serveur.
  - `EVENT_NETWORK_FAILOVER_ACTIVATED` lorsque les échecs Wi-Fi dépassent le seuil configuré.
- Callbacks HTTP complétés via `remote_event_adapter_on_http_response` pour que la normalisation JSON soit centralisée dans l'adaptateur.

## Fonctionnement détaillé
1. **Initialisation** : crée les structures Wi-Fi (netif + event loop), enregistre les handlers d'événements et stocke l'EventBus. Aucun thread supplémentaire n'est créé ici ; les callbacks ESP-IDF sont utilisés.
2. **Connexion Wi-Fi** : `wifi_init_sta` configure le STA, lance la connexion et attend `WIFI_CONNECTED_BIT` ou `WIFI_FAIL_BIT` avec un timeout. Les échecs successifs incrémentent `s_fail_sequences` et peuvent déclencher un failover via événement dédié.
3. **Suivi IP et reachabilité** : au `IP_EVENT_STA_GOT_IP`, le statut `system_status_t` est mis à jour et publié. Une fois Wi-Fi OK, les WebSockets/HTTP sont initiés.
4. **WebSockets** : `start_websocket` ouvre trois clients (`telemetry`, `events`, `alerts`) et associe chacun à une callback `handle_ws_event` qui transmet le payload brut à `remote_event_adapter_on_*`. Les reconnexions sont gérées par l'API esp-websocket (retry interne) ; en cas de déconnexion, l'état réseau est publié.
5. **HTTP client** : `net_client_send_http_request` construit un `esp_http_client_config_t` pointant vers le bridge et déclenche l'appel. Les réponses sont rapatriées vers l'adaptateur pour uniformiser le traitement JSON/corrélation.
6. **Publication d'état** : `publish_system_status` envoie l'état courant (Wi-Fi, serveur, mode, `telemetry_expected`) afin d'alimenter GUI et modèles système. Les transitions réseau (OK/erreur/failover) sont immédiatement reflétées.
7. **Arrêt/relance** : en cas de changement de mode ou d'arrêt réseau, `net_client_stop` ferme proprement WebSockets et Wi-Fi. Le prochain `net_client_start` recréera l'infrastructure.

## Place dans le flux
`net_client` est l'interface réseau côté ESP32. Il fournit les flux bruts JSON au `remote_event_adapter`, signale l'état de connectivité aux modèles système et transporte les commandes utilisateur vers le backend. Il est essentiel pour le mode connecté S3, mais reste inactif en mode autonome TinyBMS.
