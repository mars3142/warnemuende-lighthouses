#pragma once

/**
 * @brief Supported value types for NVS persistence.
 */
typedef enum
{
    VALUE_TYPE_STRING,
    VALUE_TYPE_INT8,
    VALUE_TYPE_INT32,
} persistence_value_type_t;

/**
 * @brief Initializes the NVS flash and opens the given namespace.
 *
 * Erases and re-initialises the flash partition when a version mismatch or
 * no-free-pages error is detected. Creates the mutex used for thread-safe
 * access. Must be called once before any other persistence function.
 *
 * @param namespace_name  NVS namespace to open (max 15 characters).
 */
void persistence_init(const char *namespace_name);

/**
 * @brief Saves a value to NVS under the given key.
 *
 * Thread-safe. Commits the value immediately so it survives a reboot.
 *
 * @param value_type  Type of the value (STRING, INT8, or INT32).
 * @param key         NVS key string (max 15 characters).
 * @param value       Pointer to the value to store.
 */
void persistence_save(persistence_value_type_t value_type, const char *key, const void *value);

/**
 * @brief Loads a value from NVS into the caller-provided buffer.
 *
 * Thread-safe. Returns @p out unchanged when the key does not exist or an
 * error occurs (the error is logged but not propagated).
 *
 * @param value_type  Type of the value (STRING, INT8, or INT32).
 * @param key         NVS key string (max 15 characters).
 * @param out         Buffer that receives the loaded value.
 * @return            @p out for convenience.
 */
void *persistence_load(persistence_value_type_t value_type, const char *key, void *out);

/**
 * @brief Closes the NVS handle and deletes the mutex.
 *
 * Should be called on graceful shutdown. Subsequent calls to
 * persistence_save / persistence_load are no-ops.
 */
void persistence_deinit();
