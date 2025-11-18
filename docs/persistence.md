# Persistance des données sur ESP32-P4

Ce document résume où et comment le firmware HMI stocke les informations persistantes dans la NVS. Chaque section indique
le namespace, la clé et le scénario dans lequel les données sont lues/écrites.

## Mode de fonctionnement HMI
- Fichier : `main/operation_mode.c`
- Namespace : `hmi_mode` ; clé : `mode` (entier signé)
- Utilisation : au boot, `operation_mode_init()` charge le mode choisi (connecté S3 ou autonome TinyBMS). En cas d'échec, le
  mode par défaut (Kconfig) est persisté pour les redémarrages suivants. Tout changement de mode via l'IHM est immédiatement
  écrit en NVS grâce à `operation_mode_set()`.

## Configuration persistante HMI
- Fichier : `components/config_manager/config_manager.c`
- Namespace : `hmi_cfg` ; clé : `persist_v1` (blob)
- Utilisation : `config_manager_init()` charge la configuration (seuils d'alertes, rétention des journaux, destinations MQTT/HTTP).
  Si aucune configuration valide n'est trouvée, les valeurs par défaut issues du Kconfig sont appliquées puis sauvegardées.
  Les mises à jour complètes sont écrites via `config_manager_save()` qui stocke l'intégralité de la structure `hmi_persistent_config_t`.

## Journal diagnostic circulaire
- Fichier : `components/diagnostic_logger/diagnostic_logger.c`
- Namespace : `diag_log` ; clé : `ring` (blob compressé RLE)
- Utilisation : `diagnostic_logger_init()` charge le journal compressé pour afficher l'historique série après un reboot.
  Chaque ajout d'entrée (`append_entry`) recalcule le ring buffer en mémoire puis appelle `persist_ring()` pour sauvegarder
  le bloc compressé et mettre à jour l'état de santé du stockage.

## Cache hors-ligne des télémétries/configuration
- Fichier : `components/remote_event_adapter/remote_event_adapter.c`
- Namespace : `remote_cache` ; clés : `tele` (télémétrie + stats pack) et `cfg` (configuration HMI)
- Utilisation : lors d'une perte réseau, les dernières valeurs reçues sont conservées dans la NVS via `save_cached_telemetry()`
  et `save_cached_config()`. Au prochain démarrage, `load_cached_state()` réinjecte ces structures dans l'EventBus pour
  restaurer la dernière vue connue avant d'avoir à recontacter le backend.

## Langue de l'IHM
- Fichier : `components/gui_lvgl/ui_i18n.c`
- Namespace : `ui_i18n` ; clé : `lang` (octet)
- Utilisation : la langue sélectionnée dans l'interface est mémorisée via `ui_i18n_set_language()`, qui persiste le choix
  dans `save_language_to_nvs()`. Au démarrage, `ui_i18n_init()` lit la valeur afin de recharger les libellés localisés
  sans action utilisateur.
