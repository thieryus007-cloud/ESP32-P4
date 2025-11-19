# Tâches d'amélioration performance & robustesse (ESP32-P4)

Ce document recense quatre tâches ciblées pour durcir les chemins critiques réseau/télémétrie. Chaque tâche inclut le constat, l'objectif et les étapes de mise en œuvre afin d'éviter les régressions tout en restant aligné avec l'architecture existante.

## 1. `components/network` – Sécuriser l'état partagé du `network_publisher`

**Constat.** La tâche FreeRTOS `publisher_task` et les callbacks `on_battery_status`, `on_pack_stats` et `on_system_status` manipulent simultanément `s_state` (snapshots batterie, tampon circulaire, drapeaux de connectivité) sans aucune synchronisation.【F:components/network/network_publisher.c†L33-L284】 Cela peut mener à des lectures partielles (ex : `has_batt` mis à jour alors que `last_batt` est encore en cours de copie) ou à des corruptions du tampon offline si un événement survient pendant un `buffer_push`/`buffer_pop`.

**Objectif.** Garantir que les accès à `s_state` sont atomiques, sans casser l'API publique (`network_publisher_*`) ni la cadence actuelle de publication.

**Plan d'exécution.**
1. Introduire un verrou léger (`portMUX_TYPE` ou `SemaphoreHandle_t`) encapsulé dans des helpers (`with_state_lock`, `buffer_locked_push`, etc.) pour protéger `last_batt`, `last_pack`, les drapeaux `has_*` et les compteurs de tampon.
2. Modifier `publisher_task`, `flush_buffer_if_online` et les callbacks EventBus pour n'accéder à `s_state` que via ces helpers. Prévoir des copies locales (stack) lorsque la section critique doit rester courte.
3. Étendre `network_publisher_get_metrics` afin qu'il retourne un instantané cohérent (lecture sous verrou) et ajouter, si possible, un compteur de collisions détectées pour diagnostiquer d'éventuels dépassements de tampon après refactor.

**Critères d'acceptation.**
- Publication HTTP/MQTT continue à la même cadence (`CONFIG_NETWORK_TELEMETRY_PERIOD_MS`).
- Aucun `buffer_*` n'est appelé hors section critique (revue statique).
- Les métriques rapportent un nombre de points publiés strictement monotone, même sous trafic événementiel soutenu.

## 2. `components/remote_event_adapter` – Throttler la persistance NVS

**Constat.** `remote_event_adapter_on_telemetry_json` appelle `save_cached_telemetry()` pour chaque trame WebSocket, ce qui entraîne un `nvs_open → nvs_set_blob → nvs_commit` bloquant à chaque mise à jour.【F:components/remote_event_adapter/remote_event_adapter.c†L588-L732】【F:components/remote_event_adapter/remote_event_adapter.c†L306-L337】 À 2 Hz de télémétrie, cela provoque >100 k écritures NVS/jour, use prématurée et latence dans le traitement JSON.

**Objectif.** Mettre en place un cache RAM + flush différé (debounce) pour la télémétrie/config, sans perdre la possibilité de restaurer l'état après reboot.

**Plan d'exécution.**
1. Ajouter un module interne de persistance avec :
   - un snapshot RAM (`pending_telemetry_cache`, `pending_config_cache`),
   - un timestamp (`last_flush_ms`) et un seuil configurable (ex : 5 s ou variation >X mV).
2. Remplacer les appels directs à `save_cached_*` par un marquage `dirty` + programmation d'un `esp_timer` ou d'une tâche basse priorité qui exécute réellement `nvs_set_blob` lorsque le seuil de temps/delta est atteint.
3. Garantir que `remote_event_adapter_init` lit toujours le dernier snapshot NVS (mécanisme existant) et documenter la fenêtre de données potentielles perdues (≤ intervalle de flush).

**Critères d'acceptation.**
- Profilage `esp_timer_get_time()` montre que `remote_event_adapter_on_telemetry_json` n'exécute plus d'appels NVS bloquants.
- Nombre d'écritures NVS réduit d'au moins 90 % en mode connecté.
- Au reboot, la télémétrie restaurée ne diffère pas de plus d'un flush intervalle.

## 3. `components/telemetry_model` – Synchroniser la fusion TinyBMS / télémétrie distante

**Constat.** Le modèle stocke `s_state` (batterie, statistiques pack, drapeaux de connexion) et l'actualise à la fois depuis la tâche `poll_tinybms_task` et depuis les callbacks EventBus (TinyBMS register/mode/MQTT). Aucun verrou ne protège ces accès concurrentiels.【F:components/telemetry_model/telemetry_model.c†L25-L196】 Résultat : les cellules peuvent être recalculées pendant qu'un événement GUI lit un tableau partiellement mis à jour, ou des flags comme `telemetry_expected` peuvent basculer au milieu d'un cycle de polling.

**Objectif.** Fournir un modèle de données cohérent quel que soit le producteur (mode autonome ou connecté) tout en minimisant l'overhead CPU.

**Plan d'exécution.**
1. Introduire un mutex (`SemaphoreHandle_t`) ou un spinlock spécifique au modèle afin de protéger toutes les écritures/lectures de `s_state`. Les fonctions `publish_updates` et `apply_register_update` liront/écriront via des copies locales sous verrou.
2. Ajouter un champ `pending_publish`/`dirty_mask` pour ne publier que lorsque des deltas significatifs surviennent (évite les rafales d'événements lors de la boucle de polling) tout en respectant le besoin de réactivité GUI.
3. Coupler le changement de `telemetry_expected` (événement mode) avec un reset atomique du cache pack afin d'éviter que les valeurs S3 obsolètes ne persistent lorsqu'on repasse en autonome.

**Critères d'acceptation.**
- Tests en mode autonome montrent des mises à jour cohérentes des cellules (min/max/delta jamais négatifs ou inversés).
- Aucun warning ThreadSanitizer/Helgrind lorsqu'on exécute les tests unitaires ciblant `telemetry_model` (si disponibles).
- Les métriques GUI ne montrent plus de flashs intermittents lorsque S3 et TinyBMS alternent rapidement.

## 4. `components/tinybms_client` – Décharger les appels bloquants via une tâche de service

**Constat.** Les APIs `tinybms_read_register`/`tinybms_write_register` bloquent l'appelant pendant tout l'I/O UART : prise du mutex, boucle d'attente `xQueueReceive`, retries et vérification readback.【F:components/tinybms_client/tinybms_client.c†L428-L520】【F:components/tinybms_client/tinybms_client.c†L150-L220】 Dans `tinybms_client_start`, même l'initialisation se fait via ce chemin bloquant.【F:components/tinybms_client/tinybms_client.c†L401-L425】 Une GUI qui déclenche une lecture pendant que `telemetry_model` sonde les clés critiques attendra potentiellement plusieurs centaines de millisecondes, impactant la réactivité.

**Objectif.** Sérialiser les transactions UART dans une tâche dédiée afin que les producteurs (GUI, modèles, réseau) postent des requêtes non bloquantes, tout en conservant la logique de retry/vérification existante.

**Plan d'exécution.**
1. Créer une `QueueHandle_t` de requêtes (`read`, `write`, `restart`) contenant adresse, payload et un `TaskHandle_t`/callback pour notifier le résultat.
2. Implémenter `tinybms_client_worker_task` qui lit la queue, appelle `read_register_internal`/`write_register_internal`, puis notifie le demandeur via `xTaskNotify` ou EventBus. Le mutex UART devient interne à cette tâche (les API publiques postent simplement et attendent la notification, avec timeout court).
3. Mettre à jour `tinybms_model` et les autres consommateurs pour exploiter cette interface (par exemple via promises ou événements), et enrichir les stats (`queue_depth_max`, `avg_latency_ms`).

**Critères d'acceptation.**
- Les appels `tinybms_read_register` ne bloquent plus la tâche appelante au-delà du temps d'attente de notification (configurable, < 50 ms typiquement).
- Sous charge (polling + GUI), aucun dépassement de `TINYBMS_TIMEOUT_MS` n'est observé et la file conserve une profondeur finie.
- Les stats publiées (`EVENT_TINYBMS_STATS_UPDATED`) incluent désormais latence moyenne et nombre de requêtes en file, confirmant la sérialisation centralisée.
