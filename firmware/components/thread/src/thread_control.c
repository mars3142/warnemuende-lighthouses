#include "thread_control.h"

#include <stdio.h>
#include <string.h>

#include "esp_log.h"
#include "esp_mac.h"
#include "esp_openthread.h"
#include "esp_openthread_lock.h"
#include "esp_openthread_netif_glue.h"
#include "esp_openthread_types.h"
#include "esp_ot_config.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs.h"
#include "openthread/coap.h"
#include "openthread/dataset.h"
#include "openthread/instance.h"
#include "openthread/ip6.h"
#include "openthread/link.h"
#include "openthread/logging.h"
#include "openthread/thread.h"
#include "openthread/thread_ftd.h"
#include "sdkconfig.h"

#include "beacon.h"
#include "outdoor.h"
#include <stdlib.h>

static const char *TAG = "thread";

static char s_device_name[32];
static otCoapResource s_wellknown_resource;
static otCoapResource s_beacon_resource;
static otCoapResource s_outdoor_resource;
static otCoapResource s_flicker_resource;
static otCoapResource s_group_resource;
static bool s_coap_started = false;
static bool s_announced = false;
static bool s_groups_restored = false;

#define THREAD_NVS_NAMESPACE "thread"
#define THREAD_NVS_KEY_NAME "name"
#define THREAD_NVS_KEY_GROUPS "groups"
#define THREAD_GROUPS_MAX_LEN 384

static const char *WELLKNOWN_CORE_PAYLOAD =
    "</beacon>;rt=\"maerklin.switch\";title=\"Blinken\";sim;obs,"
    //    "</outdoor>;rt=\"maerklin.switch\";title=\"Aussenlicht\";sim;obs,"
    //    "</flicker>;rt=\"maerklin.value\";min=0;max=100;step=5;title=\"Flackern in %\";obs,"
    "</group>;rt=\"maerklin.group\";title=\"Group\"";

// ─── CoAP Observe (RFC 7641) ──────────────────────────────────────────────────

#define MAX_OBSERVERS 4

typedef struct
{
    bool active;
    otIp6Address peer_addr;
    uint16_t peer_port;
    uint8_t token[OT_COAP_MAX_TOKEN_LENGTH];
    uint8_t token_len;
    uint32_t obs_seq; // 24-bit monotonic counter (RFC 7641 §4.4)
} coap_observer_t;

static coap_observer_t s_beacon_observers[MAX_OBSERVERS];
static coap_observer_t s_outdoor_observers[MAX_OBSERVERS];
static coap_observer_t s_flicker_observers[MAX_OBSERVERS];

static bool get_observe_option(const otMessage *msg, uint64_t *out)
{
    otCoapOptionIterator it;
    if (otCoapOptionIteratorInit(&it, msg) != OT_ERROR_NONE)
        return false;
    if (!otCoapOptionIteratorGetFirstOptionMatching(&it, OT_COAP_OPTION_OBSERVE))
        return false;
    return otCoapOptionIteratorGetOptionUintValue(&it, out) == OT_ERROR_NONE;
}

static bool observer_matches(const coap_observer_t *obs, const otMessageInfo *info, const otMessage *msg)
{
    if (!obs->active)
        return false;
    if (memcmp(&obs->peer_addr, &info->mPeerAddr, sizeof(otIp6Address)) != 0)
        return false;
    if (obs->peer_port != info->mPeerPort)
        return false;
    uint8_t tlen = otCoapMessageGetTokenLength(msg);
    return obs->token_len == tlen && memcmp(obs->token, otCoapMessageGetToken(msg), tlen) == 0;
}

static void observer_add(coap_observer_t *observers, const otMessageInfo *info, const otMessage *msg)
{
    // exact match (same addr+port+token) — already registered
    for (int i = 0; i < MAX_OBSERVERS; i++)
        if (observer_matches(&observers[i], info, msg))
            return;

    // same peer, new token — update slot in place (handles observer restart)
    for (int i = 0; i < MAX_OBSERVERS; i++)
    {
        if (!observers[i].active)
            continue;
        if (memcmp(&observers[i].peer_addr, &info->mPeerAddr, sizeof(otIp6Address)) != 0)
            continue;
        if (observers[i].peer_port != info->mPeerPort)
            continue;
        observers[i].token_len = otCoapMessageGetTokenLength(msg);
        memcpy(observers[i].token, otCoapMessageGetToken(msg), observers[i].token_len);
        observers[i].obs_seq = 0;

        char addr[OT_IP6_ADDRESS_STRING_SIZE];
        otIp6AddressToString(&info->mPeerAddr, addr, sizeof(addr));
        ESP_LOGI(TAG, "Observer re-registered (new token): %s", addr);
        return;
    }

    // new peer — find empty slot
    for (int i = 0; i < MAX_OBSERVERS; i++)
    {
        if (observers[i].active)
            continue;
        observers[i].active = true;
        observers[i].peer_addr = info->mPeerAddr;
        observers[i].peer_port = info->mPeerPort;
        observers[i].token_len = otCoapMessageGetTokenLength(msg);
        memcpy(observers[i].token, otCoapMessageGetToken(msg), observers[i].token_len);
        observers[i].obs_seq = 0;

        char addr[OT_IP6_ADDRESS_STRING_SIZE];
        otIp6AddressToString(&info->mPeerAddr, addr, sizeof(addr));
        ESP_LOGI(TAG, "Observer registered: %s", addr);
        return;
    }
    ESP_LOGW(TAG, "Observer list full — registration ignored");
}

static void observer_remove(coap_observer_t *observers, const otMessageInfo *info, const otMessage *msg)
{
    for (int i = 0; i < MAX_OBSERVERS; i++)
    {
        if (!observer_matches(&observers[i], info, msg))
            continue;
        observers[i].active = false;
        char addr[OT_IP6_ADDRESS_STRING_SIZE];
        otIp6AddressToString(&info->mPeerAddr, addr, sizeof(addr));
        ESP_LOGI(TAG, "Observer deregistered: %s", addr);
        return;
    }
}

static void notify_observers(otInstance *instance, coap_observer_t *observers, const char *payload)
{
    for (int i = 0; i < MAX_OBSERVERS; i++)
    {
        if (!observers[i].active)
            continue;

        otMessage *msg = otCoapNewMessage(instance, NULL);
        if (!msg)
            continue;

        otCoapMessageInit(msg, OT_COAP_TYPE_NON_CONFIRMABLE, OT_COAP_CODE_CONTENT);
        otCoapMessageSetToken(msg, observers[i].token, observers[i].token_len);

        observers[i].obs_seq = (observers[i].obs_seq + 1) & 0xFFFFFF;
        otCoapMessageAppendObserveOption(msg, observers[i].obs_seq);
        otCoapMessageAppendContentFormatOption(msg, OT_COAP_OPTION_CONTENT_FORMAT_TEXT_PLAIN);
        otCoapMessageSetPayloadMarker(msg);
        otMessageAppend(msg, payload, (uint16_t)strlen(payload));

        otMessageInfo info;
        memset(&info, 0, sizeof(info));
        info.mPeerAddr = observers[i].peer_addr;
        info.mPeerPort = observers[i].peer_port;

        if (otCoapSendRequest(instance, msg, &info, NULL, NULL) != OT_ERROR_NONE)
        {
            otMessageFree(msg);
            ESP_LOGW(TAG, "Failed to send Observe notification to slot %d", i);
        }
    }
}

static void coap_send_observe_response(otInstance *instance, otMessage *request, const otMessageInfo *msg_info,
                                       coap_observer_t *observers, const char *payload)
{
    otCoapType type = (otCoapMessageGetType(request) == OT_COAP_TYPE_CONFIRMABLE) ? OT_COAP_TYPE_ACKNOWLEDGMENT
                                                                                  : OT_COAP_TYPE_NON_CONFIRMABLE;
    otMessage *response = otCoapNewMessage(instance, NULL);
    if (!response)
        return;

    otCoapMessageInitResponse(response, request, type, OT_COAP_CODE_CONTENT);

    uint32_t seq = 0;
    for (int i = 0; i < MAX_OBSERVERS; i++)
    {
        if (!observers[i].active)
            continue;
        uint8_t tlen = otCoapMessageGetTokenLength(request);
        if (observers[i].token_len == tlen && memcmp(observers[i].token, otCoapMessageGetToken(request), tlen) == 0 &&
            memcmp(&observers[i].peer_addr, &msg_info->mPeerAddr, sizeof(otIp6Address)) == 0)
        {
            seq = observers[i].obs_seq;
            break;
        }
    }

    otCoapMessageAppendObserveOption(response, seq);
    otCoapMessageAppendContentFormatOption(response, OT_COAP_OPTION_CONTENT_FORMAT_TEXT_PLAIN);
    otCoapMessageSetPayloadMarker(response);
    otMessageAppend(response, payload, (uint16_t)strlen(payload));

    if (otCoapSendResponse(instance, response, msg_info) != OT_ERROR_NONE)
        otMessageFree(response);
}

// ─── NVS storage ────────────────────────────────────────────────────────────

static bool load_device_name(char *out, size_t max_len)
{
    nvs_handle_t handle;
    if (nvs_open(THREAD_NVS_NAMESPACE, NVS_READONLY, &handle) != ESP_OK)
        return false;

    size_t len = max_len;
    bool ok = (nvs_get_str(handle, THREAD_NVS_KEY_NAME, out, &len) == ESP_OK && len > 1);
    nvs_close(handle);
    return ok;
}

static bool load_group_memberships(char *out, size_t max_len)
{
    nvs_handle_t handle;
    if (nvs_open(THREAD_NVS_NAMESPACE, NVS_READONLY, &handle) != ESP_OK)
        return false;

    size_t len = max_len;
    esp_err_t err = nvs_get_str(handle, THREAD_NVS_KEY_GROUPS, out, &len);
    nvs_close(handle);

    return err == ESP_OK && len > 1;
}

static bool save_group_memberships(const char *groups)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open(THREAD_NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to open NVS: %s", esp_err_to_name(err));
        return false;
    }

    if (groups && groups[0] != '\0')
    {
        err = nvs_set_str(handle, THREAD_NVS_KEY_GROUPS, groups);
    }
    else
    {
        err = nvs_erase_key(handle, THREAD_NVS_KEY_GROUPS);
        if (err == ESP_ERR_NVS_NOT_FOUND)
            err = ESP_OK;
    }

    if (err == ESP_OK)
        err = nvs_commit(handle);

    nvs_close(handle);

    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to persist groups: %s", esp_err_to_name(err));
        return false;
    }
    return true;
}

static bool group_list_contains(const char *list, const char *addr)
{
    char tmp[THREAD_GROUPS_MAX_LEN] = {};
    strlcpy(tmp, list, sizeof(tmp));

    char *saveptr = NULL;
    char *token = strtok_r(tmp, ";", &saveptr);
    while (token)
    {
        if (strcmp(token, addr) == 0)
            return true;
        token = strtok_r(NULL, ";", &saveptr);
    }
    return false;
}

static bool group_list_add(char *list, size_t list_size, const char *addr)
{
    if (group_list_contains(list, addr))
        return false;

    size_t curr_len = strlen(list);
    size_t addr_len = strlen(addr);
    size_t needed = curr_len + (curr_len > 0 ? 1 : 0) + addr_len + 1;
    if (needed > list_size)
        return false;

    if (curr_len > 0)
        strcat(list, ";");
    strcat(list, addr);
    return true;
}

static bool group_list_remove(char *list, size_t list_size, const char *addr)
{
    char src[THREAD_GROUPS_MAX_LEN] = {};
    char dst[THREAD_GROUPS_MAX_LEN] = {};
    bool removed = false;

    strlcpy(src, list, sizeof(src));

    char *saveptr = NULL;
    char *token = strtok_r(src, ";", &saveptr);
    while (token)
    {
        if (strcmp(token, addr) == 0)
        {
            removed = true;
        }
        else
        {
            if (dst[0] != '\0')
                strlcat(dst, ";", sizeof(dst));
            strlcat(dst, token, sizeof(dst));
        }
        token = strtok_r(NULL, ";", &saveptr);
    }

    strlcpy(list, dst, list_size);
    return removed;
}

static void persist_group_membership(const char *addr, bool join)
{
    char groups[THREAD_GROUPS_MAX_LEN] = {};
    bool have_groups = load_group_memberships(groups, sizeof(groups));
    if (!have_groups)
        groups[0] = '\0';

    bool changed =
        join ? group_list_add(groups, sizeof(groups), addr) : group_list_remove(groups, sizeof(groups), addr);

    if (changed && !save_group_memberships(groups))
    {
        ESP_LOGW(TAG, "Failed to update persisted groups");
    }
}

static void restore_group_memberships(otInstance *instance)
{
    if (s_groups_restored)
        return;

    s_groups_restored = true;

    char groups[THREAD_GROUPS_MAX_LEN] = {};
    if (!load_group_memberships(groups, sizeof(groups)))
    {
        return;
    }

    char *saveptr = NULL;
    char *token = strtok_r(groups, ";", &saveptr);
    while (token)
    {
        otIp6Address addr;
        if (otIp6AddressFromString(token, &addr) == OT_ERROR_NONE)
        {
            otIp6SubscribeMulticastAddress(instance, &addr);
        }
        token = strtok_r(NULL, ";", &saveptr);
    }
}

// ─── Fixed dataset from sdkconfig ────────────────────────────────────────────

static uint8_t hex_nibble(char c)
{
    if (c >= '0' && c <= '9')
        return (uint8_t)(c - '0');
    if (c >= 'a' && c <= 'f')
        return (uint8_t)(c - 'a' + 10);
    if (c >= 'A' && c <= 'F')
        return (uint8_t)(c - 'A' + 10);
    return 0;
}

static void hex_str_to_bytes(const char *hex, uint8_t *bytes, size_t len)
{
    for (size_t i = 0; i < len; i++)
        bytes[i] = (uint8_t)((hex_nibble(hex[2 * i]) << 4) | hex_nibble(hex[2 * i + 1]));
}

static void apply_fixed_dataset(otInstance *instance)
{
    otOperationalDataset dataset;
    memset(&dataset, 0, sizeof(dataset));

    dataset.mActiveTimestamp.mSeconds = 1;
    dataset.mComponents.mIsActiveTimestampPresent = true;

    dataset.mChannel = CONFIG_OPENTHREAD_NETWORK_CHANNEL;
    dataset.mComponents.mIsChannelPresent = true;

    dataset.mPanId = (otPanId)CONFIG_OPENTHREAD_NETWORK_PANID;
    dataset.mComponents.mIsPanIdPresent = true;

    strlcpy(dataset.mNetworkName.m8, CONFIG_OPENTHREAD_NETWORK_NAME, OT_NETWORK_NAME_MAX_SIZE + 1);
    dataset.mComponents.mIsNetworkNamePresent = true;

    hex_str_to_bytes(CONFIG_OPENTHREAD_NETWORK_EXTPANID, dataset.mExtendedPanId.m8, OT_EXT_PAN_ID_SIZE);
    dataset.mComponents.mIsExtendedPanIdPresent = true;

    hex_str_to_bytes(CONFIG_OPENTHREAD_NETWORK_MASTERKEY, dataset.mNetworkKey.m8, OT_NETWORK_KEY_SIZE);
    dataset.mComponents.mIsNetworkKeyPresent = true;

    if (otDatasetSetActive(instance, &dataset) != OT_ERROR_NONE)
        ESP_LOGE(TAG, "Failed to apply fixed Thread dataset");
    else
        ESP_LOGI(TAG, "Fixed Thread dataset applied (net=%s ch=%d)", CONFIG_OPENTHREAD_NETWORK_NAME,
                 CONFIG_OPENTHREAD_NETWORK_CHANNEL);
}

// ─── CoAP handlers ───────────────────────────────────────────────────────────

static void coap_send_response(otInstance *instance, otMessage *request, const otMessageInfo *msg_info, otCoapCode code,
                               const char *payload)
{
    otCoapType type = (otCoapMessageGetType(request) == OT_COAP_TYPE_CONFIRMABLE) ? OT_COAP_TYPE_ACKNOWLEDGMENT
                                                                                  : OT_COAP_TYPE_NON_CONFIRMABLE;
    otMessage *response = otCoapNewMessage(instance, NULL);
    if (!response)
        return;

    otCoapMessageInitResponse(response, request, type, code);

    if (payload && strlen(payload) > 0)
    {
        (void)otCoapMessageSetPayloadMarker(response);
        otMessageAppend(response, payload, (uint16_t)strlen(payload));
    }

    if (otCoapSendResponse(instance, response, msg_info) != OT_ERROR_NONE)
        otMessageFree(response);
}

static void wellknown_core_handler(void *context, otMessage *message, const otMessageInfo *msg_info)
{
    otInstance *instance = (otInstance *)context;

    if (otCoapMessageGetCode(message) != OT_COAP_CODE_GET)
    {
        coap_send_response(instance, message, msg_info, OT_COAP_CODE_METHOD_NOT_ALLOWED, NULL);
        return;
    }

    otCoapType type = (otCoapMessageGetType(message) == OT_COAP_TYPE_CONFIRMABLE) ? OT_COAP_TYPE_ACKNOWLEDGMENT
                                                                                  : OT_COAP_TYPE_NON_CONFIRMABLE;

    otMessage *response = otCoapNewMessage(instance, NULL);
    if (!response)
        return;

    otCoapMessageInitResponse(response, message, type, OT_COAP_CODE_CONTENT);
    otCoapMessageAppendContentFormatOption(response, OT_COAP_OPTION_CONTENT_FORMAT_LINK_FORMAT);
    otCoapMessageSetPayloadMarker(response);
    otMessageAppend(response, WELLKNOWN_CORE_PAYLOAD, (uint16_t)strlen(WELLKNOWN_CORE_PAYLOAD));

    if (otCoapSendResponse(instance, response, msg_info) != OT_ERROR_NONE)
        otMessageFree(response);
}

static void beacon_coap_handler(void *context, otMessage *message, const otMessageInfo *msg_info)
{
    otInstance *instance = (otInstance *)context;
    otCoapCode code = otCoapMessageGetCode(message);

    if (code == OT_LOG_LEVEL_NONE) // dummy check to keep code
    {
    }

    if (code == OT_COAP_CODE_GET)
    {
        uint64_t obs_val;
        bool has_observe = get_observe_option(message, &obs_val);

        if (has_observe && obs_val == 0)
            observer_add(s_beacon_observers, msg_info, message);
        else if (has_observe && obs_val == 1)
            observer_remove(s_beacon_observers, msg_info, message);

        const char *state = beacon_is_running() ? "1" : "0";
        if (has_observe && obs_val == 0)
            coap_send_observe_response(instance, message, msg_info, s_beacon_observers, state);
        else
            coap_send_response(instance, message, msg_info, OT_COAP_CODE_CONTENT, state);
    }
    else if (code == OT_COAP_CODE_PUT)
    {
        char buf[8] = {};
        uint16_t len = otMessageRead(message, otMessageGetOffset(message), buf, sizeof(buf) - 1);
        buf[len] = '\0';

        esp_err_t ret = (strcmp(buf, "1") == 0) ? beacon_start() : beacon_stop();
        coap_send_response(instance, message, msg_info,
                           ret == ESP_OK ? OT_COAP_CODE_CHANGED : OT_COAP_CODE_INTERNAL_ERROR, NULL);
        if (ret == ESP_OK)
            notify_observers(instance, s_beacon_observers, beacon_is_running() ? "1" : "0");
    }
}

static void outdoor_coap_handler(void *context, otMessage *message, const otMessageInfo *msg_info)
{
    otInstance *instance = (otInstance *)context;
    otCoapCode code = otCoapMessageGetCode(message);

    if (code == OT_COAP_CODE_GET)
    {
        uint64_t obs_val;
        bool has_observe = get_observe_option(message, &obs_val);

        if (has_observe && obs_val == 0)
            observer_add(s_outdoor_observers, msg_info, message);
        else if (has_observe && obs_val == 1)
            observer_remove(s_outdoor_observers, msg_info, message);

        const char *state = outdoor_is_running() ? "1" : "0";
        if (has_observe && obs_val == 0)
            coap_send_observe_response(instance, message, msg_info, s_outdoor_observers, state);
        else
            coap_send_response(instance, message, msg_info, OT_COAP_CODE_CONTENT, state);
    }
    else if (code == OT_COAP_CODE_PUT)
    {
        char buf[8] = {};
        uint16_t len = otMessageRead(message, otMessageGetOffset(message), buf, sizeof(buf) - 1);
        buf[len] = '\0';

        esp_err_t ret = (strcmp(buf, "1") == 0) ? outdoor_start() : outdoor_stop();
        coap_send_response(instance, message, msg_info,
                           ret == ESP_OK ? OT_COAP_CODE_CHANGED : OT_COAP_CODE_INTERNAL_ERROR, NULL);
        if (ret == ESP_OK)
            notify_observers(instance, s_outdoor_observers, outdoor_is_running() ? "1" : "0");
    }
}

static void flicker_coap_handler(void *context, otMessage *message, const otMessageInfo *msg_info)
{
    otInstance *instance = (otInstance *)context;
    otCoapCode code = otCoapMessageGetCode(message);

    if (code == OT_COAP_CODE_GET)
    {
        uint64_t obs_val;
        bool has_observe = get_observe_option(message, &obs_val);

        if (has_observe && obs_val == 0)
            observer_add(s_flicker_observers, msg_info, message);
        else if (has_observe && obs_val == 1)
            observer_remove(s_flicker_observers, msg_info, message);

        char payload[8];
        snprintf(payload, sizeof(payload), "%d", outdoor_get_flicker());
        if (has_observe && obs_val == 0)
            coap_send_observe_response(instance, message, msg_info, s_flicker_observers, payload);
        else
            coap_send_response(instance, message, msg_info, OT_COAP_CODE_CONTENT, payload);
    }
    else if (code == OT_COAP_CODE_PUT)
    {
        char buf[8] = {};
        uint16_t len = otMessageRead(message, otMessageGetOffset(message), buf, sizeof(buf) - 1);
        buf[len] = '\0';

        uint8_t val = (uint8_t)atoi(buf);
        if (val > 100)
            val = 100;

        esp_err_t ret = outdoor_set_flicker(val);
        coap_send_response(instance, message, msg_info,
                           ret == ESP_OK ? OT_COAP_CODE_CHANGED : OT_COAP_CODE_INTERNAL_ERROR, NULL);
        if (ret == ESP_OK)
        {
            char payload[8];
            snprintf(payload, sizeof(payload), "%d", val);
            notify_observers(instance, s_flicker_observers, payload);
        }
    }
}

static void group_coap_handler(void *context, otMessage *message, const otMessageInfo *msg_info)
{
    otInstance *instance = (otInstance *)context;
    otCoapCode code = otCoapMessageGetCode(message);

    if (code != OT_COAP_CODE_PUT)
    {
        coap_send_response(instance, message, msg_info, OT_COAP_CODE_METHOD_NOT_ALLOWED, NULL);
        return;
    }

    char buf[48] = {};
    uint16_t len = otMessageRead(message, otMessageGetOffset(message), buf, sizeof(buf) - 1);
    buf[len] = '\0';

    char *comma = strrchr(buf, ',');
    if (!comma)
        return;
    *comma = '\0';
    bool join = (*(comma + 1) == '1');

    otIp6Address addr;
    if (otIp6AddressFromString(buf, &addr) == OT_ERROR_NONE)
    {
        if (join)
            otIp6SubscribeMulticastAddress(instance, &addr);
        else
            otIp6UnsubscribeMulticastAddress(instance, &addr);
        persist_group_membership(buf, join);
    }
    coap_send_response(instance, message, msg_info, OT_COAP_CODE_CHANGED, NULL);
}

static void send_announce(otInstance *instance)
{
    otMessage *msg = otCoapNewMessage(instance, NULL);
    if (!msg)
        return;

    otCoapMessageInit(msg, OT_COAP_TYPE_NON_CONFIRMABLE, OT_COAP_CODE_POST);
    otCoapMessageAppendUriPathOptions(msg, "announce");
    otCoapMessageSetPayloadMarker(msg);
    otMessageAppend(msg, s_device_name, (uint16_t)strlen(s_device_name));

    otMessageInfo info;
    memset(&info, 0, sizeof(info));
    uint8_t mc_addr[16] = {0xff, 0x03, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0x01};
    memcpy(info.mPeerAddr.mFields.m8, mc_addr, sizeof(mc_addr));
    info.mPeerPort = OT_DEFAULT_COAP_PORT;

    otCoapSendRequest(instance, msg, &info, NULL, NULL);
}

static void setup_coap(otInstance *instance)
{
    if (s_coap_started)
        return;
    s_coap_started = true;
    otCoapStart(instance, OT_DEFAULT_COAP_PORT);

    s_wellknown_resource =
        (otCoapResource){.mUriPath = ".well-known/core", .mHandler = wellknown_core_handler, .mContext = instance};
    otCoapAddResource(instance, &s_wellknown_resource);

    s_beacon_resource = (otCoapResource){.mUriPath = "beacon", .mHandler = beacon_coap_handler, .mContext = instance};
    otCoapAddResource(instance, &s_beacon_resource);

    s_outdoor_resource =
        (otCoapResource){.mUriPath = "outdoor", .mHandler = outdoor_coap_handler, .mContext = instance};
    otCoapAddResource(instance, &s_outdoor_resource);

    s_flicker_resource =
        (otCoapResource){.mUriPath = "flicker", .mHandler = flicker_coap_handler, .mContext = instance};
    otCoapAddResource(instance, &s_flicker_resource);

    s_group_resource = (otCoapResource){.mUriPath = "group", .mHandler = group_coap_handler, .mContext = instance};
    otCoapAddResource(instance, &s_group_resource);
}

static void state_changed_cb(otChangedFlags flags, void *context)
{
    otInstance *instance = (otInstance *)context;

    if (!(flags & OT_CHANGED_THREAD_ROLE))
        return;

    otDeviceRole role = otThreadGetDeviceRole(instance);
    ESP_LOGI(TAG, "Thread role: %s", otThreadDeviceRoleToString(role));

    if (role == OT_DEVICE_ROLE_CHILD || role == OT_DEVICE_ROLE_ROUTER)
    {
        beacon_set_status(BEACON_STATUS_CONNECTED);
        setup_coap(instance);
        restore_group_memberships(instance);
        if (!s_announced)
        {
            s_announced = true;
            send_announce(instance);
        }
    }
    else if (role == OT_DEVICE_ROLE_DETACHED)
    {
        beacon_set_status(BEACON_STATUS_SEARCHING);
    }
}

static void thread_task(void *arg)
{
    esp_openthread_platform_config_t config = {
        .radio_config = ESP_OPENTHREAD_DEFAULT_RADIO_CONFIG(),
        .host_config = ESP_OPENTHREAD_DEFAULT_HOST_CONFIG(),
        .port_config = ESP_OPENTHREAD_DEFAULT_PORT_CONFIG(),
    };

    // Initialize the ESP-OpenThread stack
    ESP_ERROR_CHECK(esp_openthread_init(&config));

    otInstance *instance = esp_openthread_get_instance();

    // Initialize the esp_netif for OpenThread as seen in IDF examples
    esp_netif_config_t cfg = ESP_NETIF_DEFAULT_OPENTHREAD();
    esp_netif_t *openthread_netif = esp_netif_new(&cfg);
    assert(openthread_netif != NULL);

    // Initialize the glue and attach it to the netif
    void *glue_handle = esp_openthread_netif_glue_init(&config);
    assert(glue_handle != NULL);
    ESP_ERROR_CHECK(esp_netif_attach(openthread_netif, glue_handle));

    otSetStateChangedCallback(instance, state_changed_cb, instance);

    esp_openthread_lock_acquire(portMAX_DELAY);

#if CONFIG_OPENTHREAD_FTD
    // Allow router role for mesh range extension, but never win leader election
    otThreadSetLocalLeaderWeight(instance, 1);
#endif

    ESP_ERROR_CHECK(otIp6SetEnabled(instance, true));

    if (!load_device_name(s_device_name, sizeof(s_device_name)))
    {
        strlcpy(s_device_name, "Lighthouse", sizeof(s_device_name));
    }

    if (!otDatasetIsCommissioned(instance))
    {
        apply_fixed_dataset(instance);
    }
    else
    {
        ESP_LOGI(TAG, "Using stored Thread dataset");
    }

    ESP_LOGI(TAG, "Enabling Thread");
    otThreadSetEnabled(instance, true);

    esp_openthread_lock_release();

    // Run the main loop (this blocks until deinit)
    esp_openthread_launch_mainloop();

    // Cleanup
    esp_openthread_netif_glue_deinit();
    esp_netif_destroy(openthread_netif);
    esp_openthread_deinit();
    vTaskDelete(NULL);
}

esp_err_t thread_init(void)
{
    BaseType_t ret = xTaskCreate(thread_task, "thread_task", 10240, NULL, 5, NULL);
    return (ret == pdPASS) ? ESP_OK : ESP_ERR_NO_MEM;
}
