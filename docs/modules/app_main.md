# app_main.c

## Rôle
Point d'entrée ESP-IDF : initialise le stockage NVS puis délègue toute l'orchestration au module `hmi_main`. Aucun traitement métier n'est réalisé dans la boucle principale afin de laisser tourner les tasks créées par les modules métier.

## Entrées
- Aucun paramètre externe ; dépend uniquement du contexte NVS.

## Sorties
- Appelle `hmi_main_init()` puis `hmi_main_start()` pour démarrer l'EventBus, les modèles et la GUI.
- Boucle `vTaskDelay` passive pour maintenir l'application en vie.

## Place dans le flux
`app_main` est le bootstrap minimal : il prépare l'environnement ESP32 et passe la main au cœur HMI. Toute la logique applicative se trouve dans les modules démarrés par `hmi_main`.
