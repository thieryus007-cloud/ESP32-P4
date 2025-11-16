## 1. Objectif et contraintes (rappel)

* **Plateforme** : ESP32-P4-WIFI6-Touch-LCD-7B, ESP-IDF uniquement.
* **Rôle HMI** :

  * Client distant de ton système actuel (S3),
  * Affiche TOUT ce que l’interface web affiche (télémétrie, événements, config, etc.),
  * Permet les mêmes actions utilisateur (commandes, ack, réglages).
* **Architecture logicielle** (comme projet existant) :

  * `app_main.c` = orchestration globale, pas de logique métier lourde.
  * **EventBus** central (publish / subscribe).
  * Tous les modules = indépendants, connectés par EventBus (pas d’appels circulaires).
  * Modules en **ESP-IDF natif** (pas d’Arduino, pas de mélange).

---

## 2. Architecture générale du firmware HMI (couches)

### 2.1. Couches logiques

1. **HAL & BSP** (fournis)

   * Drivers LCD, tactile, horloge, WiFi/ETH (ESP-IDF + BSP Waveshare).
   * Utilisés principalement au démarrage et par LVGL / net_client.

2. **Noyau système**

   * Initialisation IDF (NVS, log, clocks, etc.),
   * `EventBus` (copie ou version factorisée du projet S3),
   * Gestion des tasks FreeRTOS & priorités.

3. **Couche “Communication”**

   * `net_client` : gestion WiFi + WebSocket/HTTP client vers le S3.
   * EventBus <-> JSON via `remote_event_adapter`.

4. **Couche “Application”**

   * Modules qui manipulent un modèle de données logique :

     * `telemetry_model` (état batterie/global),
     * `system_events_model` (wifi, storage, alertes, etc.),
     * `config_model` (config courante téléchargée).
   * Ces modules publient des events “propres” pour la GUI.

5. **Couche “Présentation” (LVGL)**

   * `gui_lvgl` + sous-écrans (`screen_home`, `screen_battery`, `screen_events`, `screen_config`, …).
   * S’abonnent aux events de la couche application.
   * Publient des events `EVT_USER_INPUT_*` lorsqu’un bouton / slider est utilisé.

---

## 3. Arborescence projet (guideline)

fw_hmi_esp32p4/
fw_hmi_esp32p4/
├── CMakeLists.txt
├── sdkconfig            (généré par ESP-IDF)
├── idf_component.yml    (dépendances LVGL + BSP + esp_lvgl_port)
├── main/
│   ├── app_main.c
│   ├── hmi_main.c
│   └── hmi_main.h
└── components/
    ├── event_bus/
    │   ├── CMakeLists.txt
    │   ├── event_bus.c
    │   └── event_bus.h
    ├── event_types/
    │   ├── CMakeLists.txt
    │   └── event_types.h
    ├── logger/
    │   ├── CMakeLists.txt
    │   ├── logger.c
    │   └── logger.h
    ├── net_client/
    │   ├── CMakeLists.txt
    │   ├── net_client.c
    │   └── net_client.h
    ├── remote_event_adapter/
    │   ├── CMakeLists.txt
    │   ├── remote_event_adapter.c
    │   └── remote_event_adapter.h
    ├── telemetry_model/
    │   ├── CMakeLists.txt
    │   ├── telemetry_model.c
    │   └── telemetry_model.h
    ├── system_events_model/
    │   ├── CMakeLists.txt
    │   ├── system_events_model.c
    │   └── system_events_model.h
    ├── config_model/
    │   ├── CMakeLists.txt
    │   ├── config_model.c
    │   └── config_model.h
    └── gui_lvgl/
        ├── CMakeLists.txt
        ├── gui_init.c
        ├── gui_init.h
        ├── screen_home.c
        ├── screen_home.h
        ├── screen_battery.c
        ├── screen_battery.h
        ├── screen_events.c
        ├── screen_events.h
        ├── screen_config.c
        └── screen_config.h

Chaque répertoire `components/xxx` se compile comme un module ESP-IDF indépendant, avec ses propres tests et sans dépendances en spaghetti.

---

## 4. app_main.c & rôle des modules

### 4.1. `app_main.c` – orchestrateur minimal

Rôle : **assembler**, pas “faire”.

Pseudo-plan :

* Init de base (NVS, logs, timers).
* Init EventBus.
* Init logger (avec référence EventBus si besoin).
* Appel d’une fonction `hmi_main_start()` qui :

  * lance les initialisations de :

    * LVGL + écran,
    * WiFi + réseau,
    * net_client,
    * remote_event_adapter,
    * models,
    * GUI.

> **Important :** `app_main.c` ne connaît que les *interfaces publiques* des modules (`xxx_init(EventBus *bus)`), jamais leur interne.

### 4.2. Interfaces types pour les modules (pattern commun)

Tous les modules suivent le même pattern :

```c
// Exemple pour net_client
void net_client_init(EventBus *bus);
void net_client_start(void);
// éventuellement net_client_set_server(const char *host, uint16_t port);
```

```c
// Exemple pour gui_lvgl
void gui_init(EventBus *bus);
```

```c
// Exemple pour telemetry_model
void telemetry_model_init(EventBus *bus);
```

Ils :

* reçoivent un pointeur vers l’EventBus,
* s’abonnent à ce qui les intéresse,
* publient leurs propres events quand ils ont du nouveau.

---

## 5. EventBus & événements (contrat interne HMI)

On se cale sur ton modèle existant : types d’événements + payloads.

### 5.1. Catégories d’événements principaux

* **Données reçues du S3** (en lecture seule côté HMI) :

  * `EVT_REMOTE_TELEMETRY_UPDATE` → payload brut proche du JSON `battery` actuel.
  * `EVT_REMOTE_SYSTEM_EVENT` → événements type “wifi_ok”, “storage_warn”, “error”, etc.
  * `EVT_REMOTE_CONFIG_SNAPSHOT` → config globale.

* **Modèle interne dérivé** (nettoyé pour GUI) :

  * `EVT_BATTERY_STATUS_UPDATED` (SOC, U, I, P, température, état général).
  * `EVT_PACK_STATS_UPDATED` (min/max cell, delta, etc.).
  * `EVT_SYSTEM_STATUS_UPDATED` (icônes wifi, MQTT, TinyBMS, CAN, etc.).

* **Actions utilisateur HMI** :

  * `EVT_USER_INPUT_SET_TARGET_SOC`
  * `EVT_USER_INPUT_CHANGE_MODE` (ex. normal / eco / debug)
  * `EVT_USER_INPUT_ACK_ALARM`
  * `EVT_USER_INPUT_WRITE_CONFIG` (écriture d’un registre via API)

* **Retour commande (ack / erreur)** :

  * `EVT_REMOTE_CMD_RESULT` (ok / error, message, code).

### 5.2. Responsabilités par module

* `net_client` :

  * Parle **HTTP/WS** avec S3 (aucun autre module ne fait de réseau).
  * Pousse les JSON bruts en events `EVT_REMOTE_RAW_*` ou via queue dédiée vers `remote_event_adapter`.

* `remote_event_adapter` :

  * Convertit JSON → `EVT_REMOTE_*` structurés.
  * Convertit `EVT_USER_INPUT_*` → HTTP/WS (JSON) vers S3.

* `telemetry_model` / `system_events_model` / `config_model` :

  * Reçoivent `EVT_REMOTE_*`, construisent un état local propre,
  * Publient `EVT_BATTERY_STATUS_UPDATED`, `EVT_SYSTEM_STATUS_UPDATED`, etc.

* `gui_lvgl` :

  * Se contente de consommer `EVT_*_UPDATED` et d’émettre `EVT_USER_INPUT_*`.

---

## 6. Tâches, FreeRTOS & concurrence

Pour rester propre et prévisible :

* **Task réseau** (net_client)

  * Priorité moyenne-haute (communication fluide).
  * Gère WiFi + WS + HTTP.

* **Task remote_event_adapter**

  * Priorité moyenne.
  * Travail de parsing JSON et conversion en events.

* **Task LVGL / GUI**

  * En général créée par `esp_lvgl_port`, qui prend en charge `lv_timer_handler`.
  * `gui_lvgl` s’exécute dans le contexte LVGL / timers pour les updates.

* **EventBus**

  * Thread-safe : callbacks d’abonnés exécutés soit :

    * directement dans le contexte de publication (si rapide),
    * soit via une queue / task dédiée `EVENT_DISPATCH_TASK` (si tu veux un modèle déterministe comme sur S3).

**Principe** : aucun module ne bloque longtemps, tout ce qui est lourd (JSON, réseau) se fait en tâche dédiée.

---

## 7. Roadmap de mise en œuvre

On va suivre ce fil rouge pour coder ensuite :

1. **Squelette projet & EventBus**

   * Créer `fw_hmi_esp32p4` avec arbo ci-dessus.
   * Intégrer ton EventBus actuel (ou une variante identique) dans `components/event_bus`.
   * Créer `event_types` de base.

2. **`app_main.c` + `hmi_main.c`**

   * Mettre en place l’init globale, l’EventBus, les appels `xxx_init(bus)`.

3. **Intégration LVGL + écran (sans réseau)**

   * BSP Waveshare + `esp_lvgl_port`.
   * `gui_init()` qui affiche un écran de test.

4. **Module net_client (connexion S3 + WS / HTTP)**

   * Connexion WiFi.
   * WebSocket `/ws/telemetry` + `/ws/events`.
   * Afficher les JSON reçus dans les logs.

5. **Module remote_event_adapter**

   * Parser les JSON telemetry/events déjà utilisés par la web UI.
   * Publier `EVT_REMOTE_TELEMETRY_UPDATE`, `EVT_REMOTE_SYSTEM_EVENT`.

6. **Modules modèle (telemetry_model, system_events_model)**

   * Construire un état simplifié pour la GUI.
   * Publier `EVT_BATTERY_STATUS_UPDATED`, `EVT_SYSTEM_STATUS_UPDATED`.

7. **GUI LVGL v1 (lecture seule)**

   * Écran home avec toutes les infos principales déjà présentes sur le web dashboard (SOC, U, I, P, icônes status).

8. **GUI LVGL v2 (actions)**

   * Boutons / sliders → `EVT_USER_INPUT_*` → `remote_event_adapter` → HTTP/WS vers S3.
   * Support `EVT_REMOTE_CMD_RESULT` pour afficher les erreurs.

9. **Extensions (config, history, debug UART/CAN)**

   * Reprendre `/api/config`, `/api/registers`, `/api/history/*` comme le fait le web.
   * Créer des écrans LVGL dédiés.

---



* l’arborescence **en version “copiable”**,
* un **`app_main.c` complet minimal**,
* et les **headers d’interface** pour :

  * `event_bus`,
  * `net_client`,
  * `remote_event_adapter`,
  * `gui_lvgl`.
