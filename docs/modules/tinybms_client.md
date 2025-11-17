# tinybms_client

## Rôle
Client UART RS485 dédié au TinyBMS. Il gère l'initialisation du port série, la connexion, la lecture/écriture de registres et publie les événements de statut associés. Il agit comme driver bas niveau utilisé par `tinybms_model`.

## Entrées
- EventBus fourni via `tinybms_client_init(event_bus_t *bus)`.
- Appels explicites depuis `tinybms_model` ou la GUI : `tinybms_read_register(address, *out)`, `tinybms_write_register(address, value)`, `tinybms_restart()`.
- Paramètres matériels compilés (broches UART, baudrate) définis dans la configuration ESP-IDF.

## Sorties
- Publication d'événements sur l'EventBus :
  - Connexion/déconnexion : `EVENT_TINYBMS_CONNECTED`, `EVENT_TINYBMS_DISCONNECTED`.
  - Mise à jour de registre : `EVENT_TINYBMS_REGISTER_UPDATED` (données brutes converties en valeur utilisateur).
  - Alertes : `EVENT_TINYBMS_ALERT_TRIGGERED` / `EVENT_TINYBMS_ALERT_RECOVERED`.
- Statistiques de communication accessibles via `tinybms_get_stats()` et réinitialisables avec `tinybms_reset_stats()` (nombre de lectures/écritures, erreurs CRC, temps moyen de réponse).

## Fonctionnement détaillé
1. **Initialisation** : configure l'UART RS485 (mode half-duplex, pins DE/RE) et attache l'EventBus. Aucune tâche n'est créée à ce stade.
2. **Connexion/découverte** : `tinybms_client_start` tente une première lecture de registre pour valider le lien ; en cas de succès, publie `EVENT_TINYBMS_CONNECTED`.
3. **Lectures** : `tinybms_read_register` envoie une requête Modbus-like, vérifie la réponse (CRC, longueur) et convertit la valeur brute en flottant utilisateur via le descripteur de registre. Publie immédiatement un événement d'update pour que les modèles réagissent.
4. **Écritures** : `tinybms_write_register` applique les conversions inverses (valeur utilisateur → raw) et pousse la commande. Les erreurs de validation remontent via un code `esp_err_t` et peuvent être traduites en événement GUI.
5. **Restart** : `tinybms_restart` envoie la commande de reset BMS et publie une déconnexion si le lien tombe ; la reconnexion est gérée par les prochaines lectures explicites.
6. **Gestion des erreurs** : timeouts/CRC incrémentent les stats et peuvent déclencher des logs d'erreur ; le modèle supérieur décide de relancer ou d'afficher l'alerte.

## Place dans le flux
`tinybms_client` est la porte d'entrée matérielle vers le pack en mode autonome. Il fournit les données brutes au `tinybms_model` et signale l'état de la liaison UART, déclenchant l'affichage ou les actions correctives côté HMI.
