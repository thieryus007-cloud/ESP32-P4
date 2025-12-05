# TinyBMS Poller

## Vue d'ensemble

Le composant `tinybms_poller` implémente le polling périodique automatique des registres Live Data du TinyBMS. Il reproduit la stratégie de l'interface web de référence (`Exemple/TinyBMS-web`) pour maintenir le cache de `tinybms_model` à jour avec les données en temps réel.

## Architecture

```
┌─────────────────┐     ┌──────────────────┐     ┌─────────────────┐
│ tinybms_poller  │────▶│  tinybms_model   │────▶│   EVENT_BUS     │
│  (Task 2s)      │     │  (Read+Cache)    │     │                 │
└─────────────────┘     └──────────────────┘     └─────────────────┘
                               │                          │
                               ▼                          ▼
                        ┌──────────────────┐     ┌─────────────────┐
                        │  tinybms_client  │     │  GUI / CAN /    │
                        │   (UART RS485)   │     │  MQTT / etc.    │
                        └──────────────────┘     └─────────────────┘
```

## Fonctionnalités

### Polling Live Data (défaut: 2s)

Poll 29 registres en temps réel:
- **Cellules (0-15)**: Tensions individuelles des cellules
- **Pack (36, 38)**: Voltage et courant total du pack
- **Min/Max (40, 41)**: Tensions min/max des cellules
- **Températures (42, 43, 48)**: Capteurs externes et interne
- **État (45, 46)**: SOH et SOC
- **Status (50, 52)**: État BMS et bits de balancing

### Polling Configuration (optionnel, défaut: désactivé)

Poll 34 registres de configuration moins fréquemment (défaut: 30s).

### Délai Inter-Registres

Respecte un délai de 50ms entre chaque lecture individuelle pour éviter de saturer le BMS (même stratégie que l'interface web).

## Configuration

### Par défaut

```c
tinybms_poller_config_t config = {
    .live_data_period_ms = 2000,      // Poll live data toutes les 2 secondes
    .config_data_period_ms = 30000,   // Poll config toutes les 30 secondes
    .inter_register_delay_ms = 50,    // 50ms entre lectures (comme interface web)
    .enable_live_data = true,         // Activer polling live
    .enable_config_data = false,      // Désactiver polling config (on-demand)
};

tinybms_poller_init(&event_bus, &config);
tinybms_poller_start();
```

### Configuration personnalisée

```c
tinybms_poller_config_t config = tinybms_poller_get_default_config();
config.live_data_period_ms = 1000;  // Poll toutes les secondes
config.enable_config_data = true;   // Activer polling config

tinybms_poller_init(&event_bus, &config);
```

## API

### Initialisation et contrôle

```c
// Initialiser avec config par défaut
esp_err_t tinybms_poller_init(event_bus_t *bus, const tinybms_poller_config_t *config);

// Démarrer le polling
esp_err_t tinybms_poller_start(void);

// Arrêter le polling
esp_err_t tinybms_poller_stop(void);

// Déclencher un poll immédiat
esp_err_t tinybms_poller_trigger_now(void);
```

### Configuration dynamique

```c
// Mettre à jour la configuration
esp_err_t tinybms_poller_set_config(const tinybms_poller_config_t *config);

// Obtenir la configuration actuelle
esp_err_t tinybms_poller_get_config(tinybms_poller_config_t *config);
```

### Statistiques

```c
uint32_t total_polls, successful_reads, failed_reads;
tinybms_poller_get_stats(&total_polls, &successful_reads, &failed_reads);

// Réinitialiser les statistiques
tinybms_poller_reset_stats();
```

## Intégration avec les autres composants

### tinybms_model

Le poller utilise `tinybms_model_read_register()` qui:
1. Lit le registre via UART
2. Met à jour le cache interne
3. Publie `EVENT_TINYBMS_REGISTER_UPDATED`

### tinybms_adapter

Le `tinybms_adapter` lit maintenant les valeurs réelles du cache au lieu de valeurs par défaut:

```c
// Avant (valeurs par défaut)
dst->pack_voltage_v = 0.0f;
dst->pack_current_a = 0.0f;

// Après (lecture du cache)
dst->pack_voltage_v = get_cached_or_default(0x0024, 0.0f);
dst->pack_current_a = get_cached_or_default(0x0026, 0.0f);
```

### Interface LVGL

La GUI LVGL est déjà abonnée à `EVENT_TINYBMS_REGISTER_UPDATED` et reçoit automatiquement les mises à jour en temps réel.

## Comparaison avec l'interface web

| Aspect | Interface Web | ESP32-P4 (avec poller) |
|--------|--------------|------------------------|
| **Registres Live** | 29 registres | 29 registres ✅ |
| **Période de polling** | Variable | 2s (configurable) ✅ |
| **Délai inter-registres** | 50ms | 50ms ✅ |
| **Méthode de lecture** | Commande 0x09 | Commande 0x09 ✅ |
| **Événements** | Socket.IO | EventBus ✅ |
| **Cache** | Non | Oui (tinybms_model) ✅ |

## Performance

### Temps de cycle complet

- 29 registres × 50ms délai = ~1.45s minimum
- + temps de communication UART (~10-20ms par registre)
- **Total: ~2-3s par cycle complet**

### Charge CPU

- Tâche de priorité 5
- Stack size: 4096 bytes
- Utilisation CPU: <1% en moyenne

### Charge UART

- ~29 transactions toutes les 2 secondes
- Compatible avec les autres utilisations de tinybms_client (lecture/écriture on-demand)

## Logs et debug

```c
// Activer les logs debug
esp_log_level_set("tinybms_poller", ESP_LOG_DEBUG);
```

Logs typiques:
```
I (12345) tinybms_poller: TinyBMS poller initialized (live_period=2000ms, config_period=30000ms)
I (12346) tinybms_poller: TinyBMS poller started
D (14500) tinybms_poller: Live poll complete: 29 success, 0 failed
D (16500) tinybms_poller: Live poll complete: 29 success, 0 failed
```

## Dépendances

- `event_bus`: Pour la communication inter-composants
- `tinybms_model`: Pour la lecture et le cache des registres
- `tinybms_client`: Pour la communication UART (indirect via tinybms_model)
- `freertos`: Pour la tâche de polling

## Tests

Vérifier que le polling fonctionne:

```c
// Vérifier les statistiques
uint32_t polls, success, failed;
tinybms_poller_get_stats(&polls, &success, &failed);
ESP_LOGI("TEST", "Polls: %lu, Success: %lu, Failed: %lu", polls, success, failed);

// Vérifier le cache
float voltage = 0.0f;
if (tinybms_model_get_cached(36, &voltage) == ESP_OK) {
    ESP_LOGI("TEST", "Pack voltage from cache: %.2fV", voltage);
}
```

## Auteur

Créé pour aligner l'ESP32-P4 avec la stratégie de polling de l'interface web de référence (Exemple/TinyBMS-web).
