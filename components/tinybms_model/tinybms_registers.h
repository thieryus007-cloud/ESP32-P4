/**
 * @file tinybms_registers.h
 * @brief TinyBMS Register Definitions and Catalog
 *
 * Generated from Exemple/mac-local/data/registers.json
 * 34 registers across 5 groups: battery, charger, safety, advanced, system
 */

#ifndef TINYBMS_REGISTERS_H
#define TINYBMS_REGISTERS_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Register groups
 */
typedef enum {
    REG_GROUP_BATTERY = 0,
    REG_GROUP_CHARGER,
    REG_GROUP_SAFETY,
    REG_GROUP_ADVANCED,
    REG_GROUP_SYSTEM,
    REG_GROUP_MAX
} register_group_t;

/**
 * @brief Register value class
 */
typedef enum {
    VALUE_CLASS_NUMERIC = 0,
    VALUE_CLASS_ENUM
} value_class_t;

/**
 * @brief Register data type
 */
typedef enum {
    TYPE_UINT16 = 0,
    TYPE_INT16,
    TYPE_ENUM
} register_type_t;

/**
 * @brief Enum value entry
 */
typedef struct {
    uint16_t value;
    const char *label;
} enum_entry_t;

/**
 * @brief Register descriptor
 */
typedef struct {
    uint16_t address;
    const char *key;
    const char *label;
    const char *unit;
    register_group_t group;
    const char *comment;
    register_type_t type;
    bool read_only;
    float scale;
    uint8_t precision;

    bool has_min;
    int32_t min_raw;
    bool has_max;
    int32_t max_raw;
    uint16_t step_raw;
    uint16_t default_raw;

    value_class_t value_class;
    const enum_entry_t *enum_values;
    uint8_t enum_count;
} register_descriptor_t;

/**
 * @brief Register cache entry
 */
typedef struct {
    uint16_t address;
    uint16_t raw_value;
    bool valid;
    uint32_t last_update_ms;
} register_cache_entry_t;

// Register count
#define TINYBMS_REGISTER_COUNT 34

/**
 * @brief Get the register catalog
 *
 * @return Pointer to array of register descriptors
 */
const register_descriptor_t* tinybms_get_register_catalog(void);

/**
 * @brief Get register descriptor by address
 *
 * @param address Register address
 * @return Pointer to descriptor, or NULL if not found
 */
const register_descriptor_t* tinybms_get_register_by_address(uint16_t address);

/**
 * @brief Get register descriptor by key
 *
 * @param key Register key string
 * @return Pointer to descriptor, or NULL if not found
 */
const register_descriptor_t* tinybms_get_register_by_key(const char *key);

/**
 * @brief Convert raw value to user value
 *
 * @param desc Register descriptor
 * @param raw_value Raw register value
 * @return User value (scaled, precision applied)
 */
float tinybms_raw_to_user(const register_descriptor_t *desc, uint16_t raw_value);

/**
 * @brief Convert user value to raw value
 *
 * @param desc Register descriptor
 * @param user_value User value
 * @param raw_value Output: raw value
 * @return ESP_OK on success, ESP_ERR_INVALID_ARG if out of range
 */
esp_err_t tinybms_user_to_raw(const register_descriptor_t *desc, float user_value,
                               uint16_t *raw_value);

/**
 * @brief Validate a raw value against register constraints
 *
 * @param desc Register descriptor
 * @param raw_value Value to validate
 * @return true if valid
 */
bool tinybms_validate_raw(const register_descriptor_t *desc, uint16_t raw_value);

/**
 * @brief Get enum label for a value
 *
 * @param desc Register descriptor (must be enum type)
 * @param value Enum value
 * @return Label string, or NULL if not found
 */
const char* tinybms_get_enum_label(const register_descriptor_t *desc, uint16_t value);

/**
 * @brief Get group name string
 *
 * @param group Register group
 * @return Group name string
 */
const char* tinybms_get_group_name(register_group_t group);

#ifdef __cplusplus
}
#endif

#endif // TINYBMS_REGISTERS_H
