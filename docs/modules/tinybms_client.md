# tinybms_client

## Rôle
Client UART RS485 dédié au TinyBMS. Il gère l'initialisation du port série, la connexion, la lecture/écriture de registres et publie les événements de statut associés.

## Entrées
- EventBus fourni via `tinybms_client_init(event_bus_t *bus)`.
- Appels de lecture/écriture `tinybms_read_register()` / `tinybms_write_register()` / `tinybms_restart()` effectués par les modèles ou la GUI.

## Sorties
- Publication des événements de connexion/déconnexion, mise à jour de registre et statistiques (`EVENT_TINYBMS_*`).
- Statistiques de communication accessibles via `tinybms_get_stats()` et `tinybms_reset_stats()`.

## Place dans le flux
`tinybms_client` est la porte d'entrée matérielle vers le pack en mode autonome. Il fournit les données brutes au `tinybms_model` et signale l'état de la liaison UART, déclenchant l'affichage ou les actions correctives côté HMI.
