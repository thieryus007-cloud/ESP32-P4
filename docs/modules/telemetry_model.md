# telemetry_model

## Rôle
Modèle local de télémétrie. Il unifie les mesures en provenance du TinyBMS (mode autonome) ou du backend S3 (mode connecté) et calcule des structures dérivées (`battery_status_t`, `pack_stats_t`) prêtes à afficher dans la GUI et à exporter (MQTT/HTTP/CAN).

## Entrées
- EventBus fourni à l'initialisation via `telemetry_model_init(event_bus_t *bus)`.
- Événements de télémétrie TinyBMS (`EVENT_TINYBMS_REGISTER_UPDATED`, `EVENT_TINYBMS_CONNECTED/DISCONNECTED`).
- Événements de télémétrie distante (`EVENT_REMOTE_TELEMETRY_UPDATE`) relayés par `remote_event_adapter`.
- Mode attendu (`telemetry_expected`) déterminé par `hmi_main` pour savoir si la source principale est réseau ou locale.

## Sorties
- Publication des mises à jour :
  - `EVENT_BATTERY_STATUS_UPDATED` pour l'état batterie (SOC/SOH, courant, puissance, températures, flags TinyBMS/CAN).
  - `EVENT_PACK_STATS_UPDATED` pour les statistiques pack (tensions cellule min/max/moyenne/delta).

## Fonctionnement détaillé
1. **État interne** : conserve un cache `battery_status_t` et `pack_stats_t`, ainsi que des flags (`telemetry_expected`, `tinybms_connected`) et l'heure du dernier publish.
2. **Mode connecté (télémétrie réseau)** : les callbacks réseau alimentent directement le cache et déclenchent `publish_updates()`. Les événements TinyBMS sont ignorés pour éviter d'écraser les données distantes.
3. **Mode autonome (TinyBMS)** :
   - Souscrit à `EVENT_TINYBMS_REGISTER_UPDATED` pour appliquer chaque lecture via `apply_register_update` (conversion mV→V, mA→A, permille→%).
   - Lance une task `poll_tinybms_task` qui lit périodiquement un sous-ensemble de registres clés quand aucune télémétrie distante n'est attendue, assurant un rafraîchissement même sans événements push.
   - Recalcule les stats pack (min/max/delta/avg) après chaque mise à jour de cellule.
4. **Synchronisation** : les publications sont synchrones sur l'EventBus ; la validité des pointeurs repose sur le cache interne statique (pas de heap éphémère).
5. **Resilience** : si un registre est manquant ou invalide, la conversion est simplement ignorée, permettant à la GUI de conserver les dernières valeurs cohérentes.

## Place dans le flux
`telemetry_model` consolide les mesures brutes (UART ou WebSocket) et expose des vues prêtes à afficher. Il sert de source unique pour la GUI, les modules d'export réseau et toute future sortie CAN, garantissant la cohérence des données quelle que soit la source initiale.
Modèle local de télémétrie. Il s'abonne aux événements issus de TinyBMS ou du backend et calcule des structures dérivées (`battery_status_t`, `pack_stats_t`) pour la GUI et les exporteurs.

## Entrées
- EventBus fourni à l'initialisation via `telemetry_model_init(event_bus_t *bus)`.
- Événements de télémétrie (TinyBMS ou S3) publiés sur l'EventBus.

## Sorties
- Publication des mises à jour de statut batterie et pack (`EVENT_BATTERY_STATUS_UPDATED`, `EVENT_PACK_STATS_UPDATED`).

## Place dans le flux
`telemetry_model` consolide les mesures brutes (UART ou WebSocket) et expose des vues prêtes à afficher. Il sert de source unique pour la GUI et les modules d'export (MQTT/HTTP, CAN) afin d'assurer la cohérence des données.
