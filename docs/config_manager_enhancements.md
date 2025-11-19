# Config Manager – C++ Migration Notes

This document captures the **current** state of the configuration subsystem after the full
C++ migration of the firmware. Earlier iterations experimented with parallel
implementations (`config_manager_core.*`, `NetClient` backends, etc.). All of those
experiments have now been deleted so the tree only contains production ready code.

```
components/config_manager/
├── CMakeLists.txt         # Registers the component with ESP-IDF
├── config_manager.cpp     # Single translation unit compiled as C++17
└── config_manager.h       # Public API consumed by the rest of the firmware
```

The goal of this cleanup was twofold:

1. **Eliminate duplicate implementations.**
   Every feature is implemented once in `config_manager.cpp`. There is no
   shadow C++ core, no auto-generated shim, and no dead `*_c_api` files.
2. **Guarantee predictable linkage.**
   Because only one object file exports the `config_manager_*` symbols, CMake
   cannot accidentally select an obsolete `.c` file, and name mangling issues
   disappear.

## Public API

`config_manager.h` keeps the historic C interface so that all callers
(main app, remote event adapter, network stack, etc.) can continue to include a
small header without pulling additional C++ dependencies. The header is compiled
as C++ and therefore remains type-safe (initializers, `constexpr` defaults, etc.).

Key entry points:

| Function | Responsibility |
| --- | --- |
| `void config_manager_init(void);` | Initializes NVS and loads persisted configuration into the global cache. |
| `const hmi_persistent_config_t *config_manager_get(void);` | Returns a pointer to the read-only cached structure. |
| `esp_err_t config_manager_set(const hmi_persistent_config_t *cfg);` | Persists a caller-provided configuration after validation. |
| `void config_manager_reset_defaults(void);` | Restores the compile-time defaults (see `sdkconfig`). |

Every function is implemented inside `config_manager.cpp` and guarded with
minimal validation to prevent `NULL` dereferences when the firmware calls the
API from different FreeRTOS tasks.

## Implementation Highlights

### Default handling

`apply_defaults()` centralizes the logic that maps the Kconfig symbols to the
`hmi_persistent_config_t` structure. The helper is reused by both
`config_manager_init()` (power-on defaults) and
`config_manager_reset_defaults()` (factory reset requested through the GUI).

### Persistent storage

NVS access is concentrated inside `persist_config()` and `load_config()`. These
helpers are written in C++ but stick to the familiar ESP-IDF NVS API so that:

- Storage errors are logged with `ESP_LOGE` before propagating the `esp_err_t`.
- Partial writes are rejected (`nvs_commit` must succeed, otherwise an error is
  returned to the caller).
- Buffer sizes are checked to protect against incompatible layout versions.

### Thread-safety

The historical module never exposed a mutex, so the migration keeps the single
static `hmi_persistent_config_t` instance and relies on task-level serialization
for writes. This guarantees deterministic behaviour without inflating the memory
footprint. If future features require concurrent updates, it will be easy to
wrap the state with an `std::mutex` because we already compile as C++17.

### Event bus integration

Consumers rely on the event bus to react to configuration changes. The
C++ migration did not alter that contract—the `config_manager` component does
not emit events itself but remains a synchronous service that higher layers call
before broadcasting `EVENT_CONFIG_UPDATED` notifications.

## Testing and Maintenance

Although `idf.py test` is not part of this repository yet, the component can be
unit-tested by linking `config_manager.cpp` against a fake NVS backend. The
module intentionally avoids global constructors or destructors, so tests can
instantiate it repeatedly without leaking handles.

Going forward:

- Keep the CMake file listing **only** `config_manager.cpp` so duplicate objects
  never reappear.
- Prefer extending `config_manager.cpp` directly rather than cloning it into a
  parallel experimental directory.
- Document behavioural changes in this file to ensure documentation and code
  remain synchronized.
