# Documentation du projet ESP32-P4

Ce répertoire centralise la documentation des modules composant le firmware HMI ESP32-P4. Chaque fiche décrit le rôle du module, ses points d'entrée et de sortie (événements, APIs publiques), ainsi que sa place dans la chaîne de flux globale.

## Structure

- `modules/` : fiches individuelles par module.
- Ajouter de nouvelles fiches pour tout composant créé ou modifié afin de conserver une vue à jour du flux.

## Flux global (résumé)

1. `app_main.c` initialise NVS puis délègue toute l'orchestration à `hmi_main.c`.
2. `hmi_main.c` prépare l'EventBus, configure le mode de fonctionnement (connecté S3 ou autonome TinyBMS), démarre les modèles (télémétrie, événements système, historique) et les interfaces de communication (Wi-Fi/WebSocket, UART TinyBMS).
3. Les callbacks réseau (`net_client` + `remote_event_adapter`) transforment les JSON entrants en événements internes publiés sur l'EventBus.
4. Les modèles (`telemetry_model`, `system_events_model`, `tinybms_model`, etc.) dérivent des structures métiers et republient des mises à jour consommées par la GUI LVGL.
5. Les modules de diffusion (`network_publisher`, `can_publisher`) exportent la télémétrie vers l'extérieur.

Consultez les fiches de `modules/` pour les détails spécifiques, notamment `mqtt_gateway.md` pour la passerelle MQTT TinyBMS.
