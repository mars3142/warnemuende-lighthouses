#include "persistence.h"
#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "nvs_flash.h"

static const char *TAG = "persistence";

static nvs_handle_t persistence_handle;
static SemaphoreHandle_t persistence_mutex;

#include "esp_log.h"
#include "nvs.h"
#include "nvs_flash.h"

static const char *nvs_type_to_str(nvs_type_t type)
{
    switch (type)
    {
    case NVS_TYPE_U8:
        return "U8";
    case NVS_TYPE_I8:
        return "I8";
    case NVS_TYPE_U16:
        return "U16";
    case NVS_TYPE_I16:
        return "I16";
    case NVS_TYPE_U32:
        return "U32";
    case NVS_TYPE_I32:
        return "I32";
    case NVS_TYPE_U64:
        return "U64";
    case NVS_TYPE_I64:
        return "I64";
    case NVS_TYPE_STR:
        return "STR";
    case NVS_TYPE_BLOB:
        return "BLOB";
    case NVS_TYPE_ANY:
        return "ANY";
    default:
        return "UNKNOWN";
    }
}

void display_nvs_value(const char *namespace_name, const char *key, nvs_type_t type)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open(namespace_name, NVS_READONLY, &handle);
    if (err != ESP_OK)
    {
        return;
    }

    switch (type)
    {
    case NVS_TYPE_I8: {
        int8_t value;
        if (nvs_get_i8(handle, key, &value) == ESP_OK)
        {
            ESP_LOGI(TAG, "  -> Value (I8): %d", value);
        }
        break;
    }
    case NVS_TYPE_I32: {
        int32_t value;
        if (nvs_get_i32(handle, key, &value) == ESP_OK)
        {
            ESP_LOGI(TAG, "  -> Value (I32): %d", value);
        }
        break;
    }
    case NVS_TYPE_STR: {
        size_t length = 0;
        nvs_get_str(handle, key, NULL, &length);
        if (length > 0)
        {
            char *value = malloc(length);
            if (nvs_get_str(handle, key, value, &length) == ESP_OK)
            {
                ESP_LOGI(TAG, "  -> Value (STR): %s", value);
            }
            free(value);
        }
        break;
    }
    case NVS_TYPE_BLOB: {
        size_t length = 0;
        nvs_get_blob(handle, key, NULL, &length);
        if (length > 0)
        {
            ESP_LOGI(TAG, "  -> Value (BLOB): %d bytes", length);

            // Optional: Erste Bytes als Hex anzeigen
            uint8_t *blob = malloc(length);
            if (nvs_get_blob(handle, key, blob, &length) == ESP_OK)
            {
                ESP_LOG_BUFFER_HEX_LEVEL(TAG, blob, MIN(length, 32), ESP_LOG_INFO);
            }
            free(blob);
        }
        break;
    }
    default:
        break;
    }

    nvs_close(handle);
}

static void list_all_nvs_entries(void)
{
    ESP_LOGI(TAG, "========== NVS ENTRIES ==========");

    nvs_iterator_t it = NULL;
    esp_err_t err;

    // Iterator für alle Namespaces und Keys erstellen
    err = nvs_entry_find(NVS_DEFAULT_PART_NAME, NULL, NVS_TYPE_ANY, &it);

    while (err == ESP_OK)
    {
        nvs_entry_info_t info;
        nvs_entry_info(it, &info);

        ESP_LOGI(TAG, "Namespace: %-16s | Key: %-16s | Type: %s", info.namespace_name, info.key,
                 nvs_type_to_str(info.type));

        // Optional: Wert anzeigen
        display_nvs_value(info.namespace_name, info.key, info.type);

        err = nvs_entry_next(&it);
    }

    nvs_release_iterator(it);
    ESP_LOGI(TAG, "==================================");
}

static void check_nvs_stats(void)
{
    nvs_stats_t nvs_stats;
    esp_err_t err = nvs_get_stats(NULL, &nvs_stats);
    if (err == ESP_OK)
    {
        ESP_LOGI(TAG, "NVS: Used entries = %d, Free entries = %d, Total entries = %d", nvs_stats.used_entries,
                 nvs_stats.free_entries, nvs_stats.total_entries);

        size_t used_kb = (nvs_stats.used_entries * 32) / 1024; // Grobe Schätzung
        size_t free_kb = (nvs_stats.free_entries * 32) / 1024;
        ESP_LOGI(TAG, "NVS: ~%d KB used, ~%d KB free", used_kb, free_kb);
    }
}

void persistence_init(const char *namespace_name)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    list_all_nvs_entries();
    check_nvs_stats();

    ESP_ERROR_CHECK(nvs_open(namespace_name, NVS_READWRITE, &persistence_handle));

    persistence_mutex = xSemaphoreCreateMutex();
    if (persistence_mutex == NULL)
    {
        ESP_LOGE(TAG, "Failed to create mutex");
    }
}

void persistence_save(persistence_value_type_t value_type, const char *key, const void *value)
{
    if (persistence_mutex != NULL)
    {
        if (xSemaphoreTake(persistence_mutex, portMAX_DELAY) == pdTRUE)
        {
            esp_err_t err = ESP_ERR_INVALID_ARG;

            switch (value_type)
            {
            case VALUE_TYPE_STRING:
                err = nvs_set_str(persistence_handle, key, (char *)value);
                break;

            case VALUE_TYPE_INT8:
                err = nvs_set_i8(persistence_handle, key, *(int8_t *)value);
                break;

            case VALUE_TYPE_INT32:
                err = nvs_set_i32(persistence_handle, key, *(int32_t *)value);
                break;

            default:
                ESP_LOGE(TAG, "Unsupported value type");
                break;
            }

            if (err == ESP_OK)
            {
                ESP_ERROR_CHECK(nvs_commit(persistence_handle));
            }
            else
            {
                ESP_LOGE(TAG, "Error saving key %s: %s", key, esp_err_to_name(err));
            }

            xSemaphoreGive(persistence_mutex);
        }
    }
}

void *persistence_load(persistence_value_type_t value_type, const char *key, void *out)
{
    if (persistence_mutex != NULL)
    {
        if (xSemaphoreTake(persistence_mutex, portMAX_DELAY) == pdTRUE)
        {
            esp_err_t err = ESP_ERR_INVALID_ARG;

            switch (value_type)
            {
            case VALUE_TYPE_STRING:
                err = nvs_get_str(persistence_handle, key, (char *)out, NULL);
                break;

            case VALUE_TYPE_INT8:
                err = nvs_get_i8(persistence_handle, key, (int8_t *)out);
                break;

            case VALUE_TYPE_INT32:
                err = nvs_get_i32(persistence_handle, key, (int32_t *)out);
                break;

            default:
                ESP_LOGE(TAG, "Unsupported value type");
                break;
            }

            if (err != ESP_OK)
            {
                ESP_LOGE(TAG, "Error loading key %s: %s", key, esp_err_to_name(err));
            }

            xSemaphoreGive(persistence_mutex);
        }
    }

    return out;
}

void persistence_deinit()
{
    if (persistence_mutex != NULL)
    {
        vSemaphoreDelete(persistence_mutex);
        persistence_mutex = NULL;
    }

    nvs_close(persistence_handle);
}
