#include "matter.h"

#include <app/clusters/on-off-server/on-off-server.h>
#include <app/server/Server.h>
#include <esp_log.h>
#include <esp_matter.h>

extern "C"
{
#include "beacon.h"
#include "outdoor.h"
}

static const char *TAG = "matter";

static uint16_t s_beacon_endpoint_id = 0;
static uint16_t s_outdoor_endpoint_id = 0;

static esp_err_t identification_cb(esp_matter::identification::callback_type_t type, uint16_t endpoint_id,
                                   uint8_t effect_id, uint8_t effect_variant, void *priv_data)
{
    return ESP_OK;
}

static esp_err_t attribute_update_cb(esp_matter::attribute::callback_type_t type, uint16_t endpoint_id,
                                     uint32_t cluster_id, uint32_t attribute_id, esp_matter_attr_val_t *val,
                                     void *priv_data)
{
    if (type != esp_matter::attribute::PRE_UPDATE)
        return ESP_OK;

    if (cluster_id != chip::app::Clusters::OnOff::Id ||
        attribute_id != chip::app::Clusters::OnOff::Attributes::OnOff::Id)
        return ESP_OK;

    if (endpoint_id == s_beacon_endpoint_id)
        return val->val.b ? beacon_start() : beacon_stop();

    if (endpoint_id == s_outdoor_endpoint_id)
        return val->val.b ? outdoor_start() : outdoor_stop();

    return ESP_OK;
}

static void matter_event_cb(const ChipDeviceEvent *event, intptr_t arg)
{
}

extern "C" esp_err_t matter_init(void)
{
    esp_matter::node::config_t node_config;
    esp_matter::node_t *node = esp_matter::node::create(&node_config, attribute_update_cb, identification_cb);
    if (!node)
    {
        ESP_LOGE(TAG, "Failed to create Matter node");
        return ESP_FAIL;
    }

    esp_matter::endpoint::on_off_light::config_t beacon_config;
    esp_matter::endpoint_t *beacon_ep =
        esp_matter::endpoint::on_off_light::create(node, &beacon_config, esp_matter::ENDPOINT_FLAG_NONE, NULL);
    if (!beacon_ep)
    {
        ESP_LOGE(TAG, "Failed to create beacon endpoint");
        return ESP_FAIL;
    }
    s_beacon_endpoint_id = esp_matter::endpoint::get_id(beacon_ep);

    esp_matter::endpoint::on_off_light::config_t outdoor_config;
    esp_matter::endpoint_t *outdoor_ep =
        esp_matter::endpoint::on_off_light::create(node, &outdoor_config, esp_matter::ENDPOINT_FLAG_NONE, NULL);
    if (!outdoor_ep)
    {
        ESP_LOGE(TAG, "Failed to create outdoor endpoint");
        return ESP_FAIL;
    }
    s_outdoor_endpoint_id = esp_matter::endpoint::get_id(outdoor_ep);

    ESP_LOGI(TAG, "beacon endpoint: %d, outdoor endpoint: %d", s_beacon_endpoint_id, s_outdoor_endpoint_id);

    return esp_matter::start(matter_event_cb);
}
