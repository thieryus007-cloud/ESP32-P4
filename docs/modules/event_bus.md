# event_bus

## Rôle
Infrastructure publish/subscribe légère au centre du firmware. Chaque module peut publier des événements typés (`event_type_t`) et s'abonner via callbacks. Le bus assure le routage synchrone des données structurées entre producteurs (réseau, UART TinyBMS, modèles) et consommateurs (GUI, agrégateurs, exporteurs).

## Entrées
- `event_bus_init(event_bus_t *bus)` pour initialiser une instance (utilisé une fois dans `hmi_main`).
- `event_bus_subscribe(event_bus_t *bus, event_type_t type, event_callback_t callback, void *user_ctx)` pour enregistrer un consommateur sur un type.
- `event_bus_publish(event_bus_t *bus, const event_t *event)` pour émettre un événement vers tous les abonnés du type.

## Sorties
- Appels de callbacks enregistrés pour le type d'événement publié (exécutés dans le contexte de l'éditeur ; pas de file asynchrone par défaut).
- Métriques `event_bus_get_metrics()` exposant nombre d'abonnés et volume d'événements, utiles pour le diagnostic.

## Fonctionnement détaillé
1. **Initialisation unique** : `hmi_main` crée une instance globale `event_bus_t` et la partage à tous les modules au moment de leur `*_init`.
2. **Souscriptions** : chaque module enregistre ses callbacks sur les événements pertinents (ex. `telemetry_model` sur `EVENT_REMOTE_TELEMETRY_UPDATE`, `tinybms_client` sur commandes utilisateur). Le `user_ctx` permet de passer un pointeur d'état module.
3. **Publication synchrone** : `event_bus_publish` itère sur les abonnés et appelle immédiatement chaque callback. Aucune copie de données n'est faite ; les structures pointées par `event.data` doivent rester valides pendant l'appel.
4. **Sécurité/Performance** : pas de mutex interne ; le bus est prévu pour être appelé depuis un seul contexte principal (les tasks des modules) avec des sections critiques courtes. Si un module doit traiter longuement, il doit déférer à une queue interne pour ne pas bloquer les autres abonnés.

## Place dans le flux
Le bus d'événements constitue le tissu conjonctif du firmware. Il permet de découpler la collecte (réseau, UART) de la consommation (GUI, export) sans créer de dépendances directes. Toute évolution (nouvel exporteur, nouvelle source) passe par de nouveaux types d'événements et des souscriptions ciblées.
