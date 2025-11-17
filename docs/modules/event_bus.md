# event_bus

## Rôle
Infrastructure publish/subscribe légère utilisée par l'ensemble du firmware pour échanger des événements typés.

## Entrées
- `event_bus_subscribe(event_type_t type, event_callback_t callback, void *user_ctx)` pour enregistrer des consommateurs.
- `event_bus_publish(const event_t *event)` pour diffuser un événement vers les abonnés d'un type donné.

## Sorties
- Routage synchrone vers chaque abonné enregistré pour le type concerné.
- Métriques accessibles via `event_bus_get_metrics()` (nombre de subscribers, total d'événements publiés).

## Place dans le flux
Le bus d'événements relie les producteurs (réseau, TinyBMS, modèles) et les consommateurs (GUI, agrégateurs, exporteurs). Toute la chaîne de flux repose sur ce découplage.
