# app_main.c

## Rôle
Point d'entrée ESP-IDF : réserve le stockage NVS et confie toute l'orchestration métier au module `hmi_main`. La fonction `app_main` ne crée aucune tâche applicative ; elle s'assure simplement que le socle ESP-IDF est prêt avant de déléguer l'initialisation au cœur HMI.

## Entrées
- Contexte NVS implicite (initialisé via `nvs_flash_init`).
- Aucun paramètre ou configuration directe : toute la configuration (mode de fonctionnement, Wi-Fi, seuils) est chargée plus tard par `hmi_main` et les modèles.

## Sorties
- Appelle `hmi_main_init()` puis `hmi_main_start()` pour lancer l'EventBus, les modèles, les clients réseau et la GUI.
- Exécute une boucle `vTaskDelay` infinie pour maintenir le firmware en vie une fois l'initialisation terminée.

## Fonctionnement détaillé
1. **Initialisation NVS** : réessaye un erase/reinit si nécessaire (comportement standard ESP-IDF) afin de garantir la disponibilité du stockage persistant utilisé par les modules de configuration.
2. **Délégation HMI** : appelle immédiatement `hmi_main_init`/`start` ; toutes les tâches utilisateur sont créées dans `hmi_main` ou les modules qu'il démarre.
3. **Boucle passive** : après délégation, `app_main` reste dormant. Toute la logique applicative vit dans les tasks créées par `hmi_main` (GUI, modèles, clients réseau, UART TinyBMS, agrégateurs).

## Place dans le flux
`app_main` est un bootstrap minimal : il met à disposition le stockage persistant puis passe la main au cœur HMI. Il ne publie ni ne consomme d'événements et n'a pas de dépendances métier, ce qui facilite le portage vers d'autres plateformes ESP-IDF.
Point d'entrée ESP-IDF : initialise le stockage NVS puis délègue toute l'orchestration au module `hmi_main`. Aucun traitement métier n'est réalisé dans la boucle principale afin de laisser tourner les tasks créées par les modules métier.

## Entrées
- Aucun paramètre externe ; dépend uniquement du contexte NVS.

## Sorties
- Appelle `hmi_main_init()` puis `hmi_main_start()` pour démarrer l'EventBus, les modèles et la GUI.
- Boucle `vTaskDelay` passive pour maintenir l'application en vie.

## Place dans le flux
`app_main` est le bootstrap minimal : il prépare l'environnement ESP32 et passe la main au cœur HMI. Toute la logique applicative se trouve dans les modules démarrés par `hmi_main`.
