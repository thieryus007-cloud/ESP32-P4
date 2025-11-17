# tinybms_model

## Rôle
Modèle haut niveau des registres TinyBMS. Il maintient un cache cohérent, valide les écritures, orchestre les lectures complètes ou ciblées et publie les événements de mise à jour pour la GUI et les autres modèles (télémétrie, historique).

## Entrées
- EventBus fourni via `tinybms_model_init(event_bus_t *bus)`.
- Lectures/écritures déclenchées par la GUI ou des tâches internes : `tinybms_model_read_all`, `tinybms_model_read_register`, `tinybms_model_write_register`.
- Événements de connexion TinyBMS (`EVENT_TINYBMS_CONNECTED/DISCONNECTED`) et d'alertes.

## Sorties
- Publication des événements :
  - `EVENT_TINYBMS_REGISTER_UPDATED` après chaque lecture réussie (inclut clé, adresse, valeur utilisateur et brute).
  - `EVENT_TINYBMS_CONFIG_CHANGED` lorsque des écritures modifient la configuration persistante du BMS.
  - Relais d'alertes vers le bus pour diffusion réseau ou GUI.
- Expose un snapshot de configuration via `tinybms_model_get_config()` et des métriques de cache via `tinybms_model_get_stats()`.

## Fonctionnement détaillé
1. **Cache structuré** : conserve les registres connus avec leurs métadonnées (adresse, type, facteurs d'échelle) pour éviter les lectures répétées et offrir une validation centralisée.
2. **Lecture complète** : `tinybms_model_read_all` itère sur la liste des registres documentés et déclenche les lectures séquentielles via `tinybms_client`. Chaque réponse est convertie et publiée, permettant à la GUI d'afficher rapidement un état global.
3. **Lecture ciblée** : `tinybms_model_read_register` valide la clé/adresse, interroge le client UART et met à jour le cache. Utile pour les refresh ponctuels déclenchés par l'utilisateur ou une alerte.
4. **Écriture sécurisée** : `tinybms_model_write_register` vérifie la plage autorisée et la typologie (lecture seule, lecture/écriture) avant d'appeler `tinybms_write_register`. En cas de succès, publie un événement de configuration mise à jour pour synchroniser la GUI et le backend.
5. **Gestion des alerts** : relaie les événements d'alerte TinyBMS (surchauffe, surtension, etc.) et peut déclencher une lecture ciblée pour rafraîchir les données critiques.
6. **Fallback de télémétrie** : en mode autonome, ses mises à jour nourrissent `telemetry_model` via les événements `EVENT_TINYBMS_REGISTER_UPDATED`, assurant une cohérence de la chaîne sans backend.

## Place dans le flux
`tinybms_model` transforme les lectures brutes UART en données structurées prêtes à afficher ou exporter. Il joue le rôle de tampon cohérent entre le client UART et les consommateurs (GUI, `telemetry_model`, export MQTT/HTTP), centralisant la validation et la cohérence des registres TinyBMS.
