# mqtt_gateway

## Rôle
Passerelle MQTT dédiée au TinyBMS. Elle publie les registres lus via l'UART sur des topics configurables et traite des commandes JSON entrantes pour lire/écrire les registres TinyBMS. Le module propage aussi l'état de connexion MQTT sur l'EventBus pour que l'HMI et la télémétrie restent synchronisées.

## Cycle de vie
- `mqtt_gateway_init(event_bus_t *bus)` : charge la configuration persistée (broker + topic), construit le client MQTT, s'abonne à l'EventBus et publie un snapshot de configuration pour pré-remplir l'HMI.
- `mqtt_gateway_start()` : lance le client MQTT, déclenche une lecture complète des registres pour alimenter immédiatement les écrans et publie les valeurs en cache dès la connexion au broker.
- `mqtt_gateway_stop()` : arrête le client et notifie l'EventBus que le lien MQTT est inactif.

## Configuration
- Source : `config_manager` fournit `mqtt_broker` et `mqtt_topic` persistés ; l'HMI offre trois champs (`mqtt_broker`, `mqtt_topic_pub`, `mqtt_topic_sub`) via `EVENT_USER_INPUT_WRITE_CONFIG`.
- Application : chaque saisie utilisateur est persistée puis appliquée immédiatement (reconstruction du client si nécessaire).
- Topics : par défaut le publish et le subscribe réutilisent le même préfixe (`cfg->mqtt_topic`).

### Exemple de configuration (JSON HMI -> EventBus)
```json
{
  "config": {
    "mqtt_broker": "mqtt://192.168.0.50",
    "mqtt_topic_pub": "tinybms/out",
    "mqtt_topic_sub": "tinybms/in"
  },
  "mqtt_only": false
}
```

## Entrées
- Événements EventBus :
  - `EVENT_TINYBMS_REGISTER_UPDATED` pour chaque registre TinyBMS lu sur l'UART.
  - `EVENT_USER_INPUT_WRITE_CONFIG` pour appliquer et enregistrer les valeurs saisies dans l'HMI.
- MQTT : messages JSON reçus sur `topic_sub` avec la forme `{ "key"|"address", "read":true|false, "value":<float> }`.

## Sorties
- MQTT publish sur `topic_pub/<register.key>` avec la valeur convertie (précision définie par le catalogue TinyBMS).
- Publication EventBus `EVENT_MQTT_STATUS_UPDATED` contenant `enabled`, `connected` et `reason` pour alimenter la GUI et la télémétrie.
- Publication EventBus `EVENT_CONFIG_UPDATED` lors de l'initialisation pour synchroniser l'écran de configuration avec les valeurs persistées.

## Fonctionnement détaillé
1. **Construction client** : `rebuild_client` crée le client à partir des champs `mqtt_broker/topic_pub/topic_sub` et enregistre `on_mqtt_event`.
2. **Connexion** : à `MQTT_EVENT_CONNECTED`, le module s'abonne à `topic_sub`, publie toutes les valeurs en cache (lecture directe du `tinybms_model`) puis émet un statut MQTT connecté.
3. **Déconnexion** : `MQTT_EVENT_DISCONNECTED` marque l'état comme hors ligne et publie un statut MQTT avec la raison "disconnected".
4. **Publication des registres** : chaque `EVENT_TINYBMS_REGISTER_UPDATED` convertit la valeur utilisateur et la publie immédiatement sur le topic dédié.
5. **Commande JSON entrante** :
   - Résolution du registre via `key` ou `address`.
   - Si `read` est vrai, tente une lecture (`tinybms_model_read_register` sinon cache) puis republie la valeur.
   - Sinon, si `value` est numérique, déclenche `tinybms_model_write_register` pour écrire le registre TinyBMS.
6. **Synchronisation initiale** : une tâche courte `sync_task` lit l'ensemble des registres TinyBMS après le démarrage pour que l'HMI dispose d'un cache complet avant toute interaction MQTT.
7. **Persistance** : lors d'une écriture de configuration utilisateur, le module met à jour `config_manager_save`, reconstruit le client puis redémarre la session si elle était active.

## Interactions avec les autres modules
- **tinybms_model** : source principale des valeurs publiées et point d'entrée pour les lectures/écritures déclenchées par MQTT.
- **config_manager** : stockage persistant des paramètres broker/topic ; sert de snapshot pour pré-remplir l'HMI au boot.
- **telemetry_model / GUI LVGL** : consomme `EVENT_MQTT_STATUS_UPDATED` pour indiquer la connectivité MQTT et `EVENT_CONFIG_UPDATED` pour afficher les valeurs actuelles.
- **hmi_main** : orchestre l'initialisation/démarrage de la passerelle dans la séquence système.

## Exemples d'usage MQTT
- **Lire un registre par clé**
  ```json
  {"key":"cell1_voltage","read":true}
  ```
  → publie `tinybms/out/cell1_voltage` avec la valeur actuelle.

- **Écrire un seuil**
  ```json
  {"key":"charge_current_limit","value":12.5}
  ```
  → envoie une commande d'écriture TinyBMS et loggue le résultat.

- **Lire par adresse**
  ```json
  {"address":204, "read":true}
  ```
  → convertit l'adresse en registre et publie la valeur correspondante.

## Dépannage
- Vérifier les logs `mqtt_gateway` : connexion, souscription, publications et erreurs d'écriture sont tracées avec le niveau approprié.
- S'assurer que `config_manager` contient des topics cohérents ; le module réutilise le même préfixe pour publish/subscribe si seuls les champs persistés sont renseignés.
- Utiliser un `mosquitto_sub` sur `topic_pub/#` pour suivre les mises à jour en temps réel ; envoyer une commande de lecture sur `topic_sub` pour valider le chemin aller-retour.
- En cas d'absence de valeur, confirmer que `tinybms_model` dispose d'un cache (la tâche `sync_task` de démarrage relit tous les registres et republie le cache après connexion broker).
