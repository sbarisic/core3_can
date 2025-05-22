#include <core3.h>
#include <core3_bt.h>
#include <core3_flash.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "driver/uart.h"
#include "esp_bt.h"
#include "esp_bt_defs.h"
#include "esp_bt_device.h"
#include "esp_bt_main.h"
#include "esp_gap_ble_api.h"
#include "esp_gatt_common_api.h"
#include "esp_gatts_api.h"

#define dprintf printf

#define spp_sprintf(s, ...) sprintf((char *)(s), ##__VA_ARGS__)
#define SPP_DATA_MAX_LEN (512)
#define SPP_CMD_MAX_LEN (20)
#define SPP_STATUS_MAX_LEN (20)
#define SPP_DATA_BUFF_MAX_LEN (2 * 1024)
/// Attributes State Machine
enum
{
    SPP_IDX_SVC,

    SPP_IDX_SPP_DATA_RECV_CHAR,
    SPP_IDX_SPP_DATA_RECV_VAL,

    SPP_IDX_SPP_DATA_NOTIFY_CHAR,
    SPP_IDX_SPP_DATA_NTY_VAL,
    SPP_IDX_SPP_DATA_NTF_CFG,

    SPP_IDX_SPP_COMMAND_CHAR,
    SPP_IDX_SPP_COMMAND_VAL,

    SPP_IDX_SPP_STATUS_CHAR,
    SPP_IDX_SPP_STATUS_VAL,
    SPP_IDX_SPP_STATUS_CFG,

    SPP_IDX_NB,
};

#define SPP_PROFILE_NUM 1
#define SPP_PROFILE_APP_IDX 0
#define ESP_SPP_APP_ID 0x56
#define SAMPLE_DEVICE_NAME "ESP_SPP_SERVER" // The Device Name Characteristics in GAP
#define SPP_SVC_INST_ID 0

/// SPP Service
static const uint16_t spp_service_uuid = 0xABF0;
/// Characteristic UUID
#define ESP_GATT_UUID_SPP_DATA_RECEIVE 0xABF1
#define ESP_GATT_UUID_SPP_DATA_NOTIFY 0xABF2
#define ESP_GATT_UUID_SPP_COMMAND_RECEIVE 0xABF3
#define ESP_GATT_UUID_SPP_COMMAND_NOTIFY 0xABF4

#define SPP_GATT_MTU_SIZE (512)

#define BLUETOOTH_TASK_PINNED_TO_CORE (0)

static const uint8_t spp_adv_data[23] = {
    /* Flags */
    0x02, 0x01, 0x06,
    /* Complete List of 16-bit Service Class UUIDs */
    0x03, 0x03, 0xF0, 0xAB,
    /* Complete Local Name in advertising */
    0x0F, 0x09, 'E', 'S', 'P', '_', 'S', 'P', 'P', '_', 'S', 'E', 'R', 'V', 'E', 'R'};

static volatile uint16_t spp_mtu_size = SPP_GATT_MTU_SIZE;
static volatile uint16_t spp_conn_id = 0xffff;
static volatile esp_gatt_if_t spp_gatts_if = 0xff;

static volatile bool enable_data_ntf = false;
static volatile bool is_connected = false;
static volatile bool is_advertising = false;
static esp_bd_addr_t spp_remote_bda = {
    0x0,
};

static uint16_t spp_handle_table[SPP_IDX_NB];

static esp_ble_adv_params_t spp_adv_params = {.adv_int_min = 0x20,
                                              .adv_int_max = 0x40,
                                              .adv_type = ADV_TYPE_IND,
                                              .own_addr_type = BLE_ADDR_TYPE_PUBLIC,
                                              .peer_addr = 0,
                                              .peer_addr_type = BLE_ADDR_TYPE_PUBLIC,
                                              .channel_map = ADV_CHNL_ALL,
                                              .adv_filter_policy = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY};

struct gatts_profile_inst
{
    esp_gatts_cb_t gatts_cb;
    uint16_t gatts_if;
    uint16_t app_id;
    uint16_t conn_id;
    uint16_t service_handle;
    esp_gatt_srvc_id_t service_id;
    uint16_t char_handle;
    esp_bt_uuid_t char_uuid;
    esp_gatt_perm_t perm;
    esp_gatt_char_prop_t property;
    uint16_t descr_handle;
    esp_bt_uuid_t descr_uuid;
};

typedef struct spp_receive_data_node
{
    int32_t len;
    uint8_t *node_buff;
    struct spp_receive_data_node *next_node;
} spp_receive_data_node_t;

typedef struct spp_receive_data_buff
{
    int32_t node_num;
    int32_t buff_size;
    spp_receive_data_node_t *first_node;
} spp_receive_data_buff_t;

static void gatts_profile_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if,
                                        esp_ble_gatts_cb_param_t *param);

/* One gatt-based profile one app_id and one gatts_if, this array will store the gatts_if returned by ESP_GATTS_REG_EVT
 */
static struct gatts_profile_inst spp_profile_tab[SPP_PROFILE_NUM] = {
    [SPP_PROFILE_APP_IDX] =
        {
            .gatts_cb = gatts_profile_event_handler,
            .gatts_if = ESP_GATT_IF_NONE, /* Not get the gatt_if, so initial is ESP_GATT_IF_NONE */
            .app_id = 0,
            .conn_id = 0,
            .service_handle = 0,
            .service_id = {.id = {.uuid = {.len = 0, .uuid = {.uuid16 = 0}}, .inst_id = 0}, .is_primary = 0},
            .char_handle = 0,
            .char_uuid = {.len = 0, .uuid = {.uuid16 = 0}},
            .perm = 0,
            .property = 0,
            .descr_handle = 0,
            .descr_uuid = {.len = 0, .uuid = {.uuid16 = 0}},
        },
};

/*
 *  SPP PROFILE ATTRIBUTES
 ****************************************************************************************
 */

#define CHAR_DECLARATION_SIZE (sizeof(uint8_t))
static const uint16_t primary_service_uuid = ESP_GATT_UUID_PRI_SERVICE;
static const uint16_t character_declaration_uuid = ESP_GATT_UUID_CHAR_DECLARE;
static const uint16_t character_client_config_uuid = ESP_GATT_UUID_CHAR_CLIENT_CONFIG;

static const uint8_t char_prop_read_notify = ESP_GATT_CHAR_PROP_BIT_READ | ESP_GATT_CHAR_PROP_BIT_NOTIFY;
static const uint8_t char_prop_read_write = ESP_GATT_CHAR_PROP_BIT_WRITE_NR | ESP_GATT_CHAR_PROP_BIT_READ;
static const uint8_t spp_data_notity_char_prop = ESP_GATT_CHAR_PROP_BIT_READ | ESP_GATT_CHAR_PROP_BIT_INDICATE;

/// SPP Service - data receive characteristic, read&write without response
static const uint16_t spp_data_receive_uuid = ESP_GATT_UUID_SPP_DATA_RECEIVE;
static const uint8_t spp_data_receive_val[20] = {0x00};

/// SPP Service - data notify characteristic, notify&read
static const uint16_t spp_data_notify_uuid = ESP_GATT_UUID_SPP_DATA_NOTIFY;
static const uint8_t spp_data_notify_val[20] = {0x00};
static const uint8_t spp_data_notify_ccc[2] = {0x00, 0x00};

/// SPP Service - command characteristic, read&write without response
static const uint16_t spp_command_uuid = ESP_GATT_UUID_SPP_COMMAND_RECEIVE;
static const uint8_t spp_command_val[10] = {0x00};

/// SPP Service - status characteristic, notify&read
static const uint16_t spp_status_uuid = ESP_GATT_UUID_SPP_COMMAND_NOTIFY;
static const uint8_t spp_status_val[10] = {0x00};
static const uint8_t spp_status_ccc[2] = {0x00, 0x00};

/// Full HRS Database Description - Used to add attributes into the database
static const esp_gatts_attr_db_t spp_gatt_db[SPP_IDX_NB] = {
    // SPP -  Service Declaration
    [SPP_IDX_SVC] = {{ESP_GATT_AUTO_RSP},
                     {ESP_UUID_LEN_16, (uint8_t *)&primary_service_uuid, ESP_GATT_PERM_READ, sizeof(spp_service_uuid),
                      sizeof(spp_service_uuid), (uint8_t *)&spp_service_uuid}},

    // SPP -  data receive characteristic Declaration
    [SPP_IDX_SPP_DATA_RECV_CHAR] = {{ESP_GATT_AUTO_RSP},
                                    {ESP_UUID_LEN_16, (uint8_t *)&character_declaration_uuid, ESP_GATT_PERM_READ,
                                     CHAR_DECLARATION_SIZE, CHAR_DECLARATION_SIZE, (uint8_t *)&char_prop_read_write}},

    // SPP -  data receive characteristic Value
    [SPP_IDX_SPP_DATA_RECV_VAL] = {{ESP_GATT_AUTO_RSP},
                                   {ESP_UUID_LEN_16, (uint8_t *)&spp_data_receive_uuid,
                                    ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE, SPP_DATA_MAX_LEN,
                                    sizeof(spp_data_receive_val), (uint8_t *)spp_data_receive_val}},

    // SPP -  data notify characteristic Declaration
    [SPP_IDX_SPP_DATA_NOTIFY_CHAR] = {{ESP_GATT_AUTO_RSP},
                                      {ESP_UUID_LEN_16, (uint8_t *)&character_declaration_uuid, ESP_GATT_PERM_READ,
                                       CHAR_DECLARATION_SIZE, CHAR_DECLARATION_SIZE,
                                       (uint8_t *)&spp_data_notity_char_prop}},

    // SPP -  data notify characteristic Value
    [SPP_IDX_SPP_DATA_NTY_VAL] = {{ESP_GATT_AUTO_RSP},
                                  {ESP_UUID_LEN_16, (uint8_t *)&spp_data_notify_uuid, ESP_GATT_PERM_READ,
                                   SPP_DATA_MAX_LEN, sizeof(spp_data_notify_val), (uint8_t *)spp_data_notify_val}},

    // SPP -  data notify characteristic - Client Characteristic Configuration Descriptor
    [SPP_IDX_SPP_DATA_NTF_CFG] = {{ESP_GATT_AUTO_RSP},
                                  {ESP_UUID_LEN_16, (uint8_t *)&character_client_config_uuid,
                                   ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE, sizeof(uint16_t),
                                   sizeof(spp_data_notify_ccc), (uint8_t *)spp_data_notify_ccc}},

    // SPP -  command characteristic Declaration
    [SPP_IDX_SPP_COMMAND_CHAR] = {{ESP_GATT_AUTO_RSP},
                                  {ESP_UUID_LEN_16, (uint8_t *)&character_declaration_uuid, ESP_GATT_PERM_READ,
                                   CHAR_DECLARATION_SIZE, CHAR_DECLARATION_SIZE, (uint8_t *)&char_prop_read_write}},

    // SPP -  command characteristic Value
    [SPP_IDX_SPP_COMMAND_VAL] = {{ESP_GATT_AUTO_RSP},
                                 {ESP_UUID_LEN_16, (uint8_t *)&spp_command_uuid,
                                  ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE, SPP_CMD_MAX_LEN, sizeof(spp_command_val),
                                  (uint8_t *)spp_command_val}},

    // SPP -  status characteristic Declaration
    [SPP_IDX_SPP_STATUS_CHAR] = {{ESP_GATT_AUTO_RSP},
                                 {ESP_UUID_LEN_16, (uint8_t *)&character_declaration_uuid, ESP_GATT_PERM_READ,
                                  CHAR_DECLARATION_SIZE, CHAR_DECLARATION_SIZE, (uint8_t *)&char_prop_read_notify}},

    // SPP -  status characteristic Value
    [SPP_IDX_SPP_STATUS_VAL] = {{ESP_GATT_AUTO_RSP},
                                {ESP_UUID_LEN_16, (uint8_t *)&spp_status_uuid, ESP_GATT_PERM_READ, SPP_STATUS_MAX_LEN,
                                 sizeof(spp_status_val), (uint8_t *)spp_status_val}},

    // SPP -  status characteristic - Client Characteristic Configuration Descriptor
    [SPP_IDX_SPP_STATUS_CFG] = {{ESP_GATT_AUTO_RSP},
                                {ESP_UUID_LEN_16, (uint8_t *)&character_client_config_uuid,
                                 ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE, sizeof(uint16_t), sizeof(spp_status_ccc),
                                 (uint8_t *)spp_status_ccc}},
};

static void gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param)
{
    switch (event)
    {
    case ESP_GAP_BLE_ADV_DATA_RAW_SET_COMPLETE_EVT:
        core3_var_watch_set(false);
        esp_ble_gap_start_advertising(&spp_adv_params);
        is_advertising = true;
        break;
    case ESP_GAP_BLE_ADV_START_COMPLETE_EVT:
        // advertising start complete event to indicate advertising start successfully or failed
        if (param->adv_start_cmpl.status != ESP_BT_STATUS_SUCCESS)
        {
            dprintf("[Bluetooth] Advert start failed, status %d\n", param->adv_start_cmpl.status);
            break;
        }
        dprintf("[Bluetooth] Advert start success\n");
        break;
    case ESP_GAP_BLE_ADV_STOP_COMPLETE_EVT:
        if (param->adv_start_cmpl.status != ESP_BT_STATUS_SUCCESS)
        {
            dprintf("[Bluetooth] Advert stop failed, status %d\n", param->adv_stop_cmpl.status);
            break;
        }
        dprintf("[Bluetooth] Advert stop success\n");
        break;
    case ESP_GAP_BLE_UPDATE_CONN_PARAMS_EVT:
    {

        dprintf("[Bluetooth] Conn params update, status %d, conn_int %d, latency %d, timeout %d\n",
                param->update_conn_params.status, param->update_conn_params.conn_int, param->update_conn_params.latency,
                param->update_conn_params.timeout);

        break;
    }
    default:
        break;
    }
}

static void restart_func(void *a)
{
    vTaskDelay(pdMS_TO_TICKS(250));
    esp_set_time_from_rtc();
    esp_restart();
}

static void gatts_profile_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if,
                                        esp_ble_gatts_cb_param_t *param)
{
    esp_ble_gatts_cb_param_t *p_data = (esp_ble_gatts_cb_param_t *)param;

    // dprintf(">> gatts_profile_event_handler event %d\n", event);

    switch (event)
    {
    case ESP_GATTS_REG_EVT:
        dprintf("[Bluetooth] GATT srv register, status %d, app_id %d, gatts_if %d\n", param->reg.status, param->reg.app_id,
                gatts_if);
        esp_ble_gap_set_device_name(SAMPLE_DEVICE_NAME);
        esp_ble_gap_config_adv_data_raw((uint8_t *)spp_adv_data, sizeof(spp_adv_data));
        esp_ble_gatts_create_attr_tab(spp_gatt_db, gatts_if, SPP_IDX_NB, SPP_SVC_INST_ID);
        break;

    case ESP_GATTS_READ_EVT:
        break;

    case ESP_GATTS_WRITE_EVT:
    {

        // ESP_LOGI(GATTS_TABLE_TAG, "Characteristic write, conn_id %d, handle %d", param->write.conn_id,
        // param->write.handle);
        // dprintf("[Bluetooth] Characteristic write, conn_id %d, handle %d, len %d\n", param->write.conn_id, param->write.handle,
        //        param->write.len);

        btDataStruc btData;

        if (param->write.len >= sizeof(btDataStruc))
        {
            memcpy(&btData, param->write.value, sizeof(btDataStruc));
            // dprintf("Got btData ID %d\n", btData.ID);

            if (btData.ID == btDataID_HELLO)
            {
                btDataStruc btResponse;
                btResponse.ID = btDataID_HELLO_RESP;
                btResponse.Counter = btData.Counter;
                btResponse.Data1 = 1;
                btResponse.Data2 = 2;
                btResponse.Data3 = 3;
                core3_bt_send_data_len((uint8_t *)&btResponse, sizeof(btDataStruc), true);
            }
            else if (btData.ID == btDataID_CAL_READ && btData.Data2 < 0xFF)
            {
                btDataStruc btResponse;
                btResponse.ID = btDataID_CAL_READ_RESP;
                btResponse.Counter = btData.Counter;
                btResponse.Data1 = btData.Data1;
                btResponse.Data2 = btData.Data2;
                btResponse.Data3 = btData.Data3;

                // if (!core3_ecu_ltft_serialize(send_mem, &btResponse))
                //{
                const void *flash_mem = core3_flash_cal_offset(btData.Data1);
                memcpy(&btResponse.Data, flash_mem, btData.Data2);
                //}

                while (!core3_bt_send_data_len((uint8_t *)&btResponse, sizeof(btDataStruc), false))
                    vTaskDelay(pdMS_TO_TICKS(1));
            }
            else if (btData.ID == btDataID_CAL_WRITE && btData.Data2 < 0xFF)
            {
                btDataStruc btResponse;
                btResponse.ID = btDataID_CAL_WRITE_RESP;
                btResponse.Counter = btData.Counter;
                btResponse.Data3 = btData.Data3;
                btResponse.Data1 = core3_flash_cal_write(btData.Data1, &btData.Data[0], btData.Data2) ? 0x1 : 0x0;

                while (!core3_bt_send_data_len((uint8_t *)&btResponse, sizeof(btDataStruc), false))
                    vTaskDelay(pdMS_TO_TICKS(1));
            }
            else if (btData.ID == btDataID_CAL_ERASE)
            {
                btDataStruc btResponse;
                btResponse.ID = btDataID_CAL_ERASE_RESP;
                btResponse.Counter = btData.Counter;
                btResponse.Data3 = btData.Data3;
                btResponse.Data1 = core3_flash_cal_erase(btData.Data1, btData.Data2);

                core3_bt_send_data_len((uint8_t *)&btResponse, sizeof(btDataStruc), true);
            }
            else if (btData.ID == btDataID_VAR_WATCH)
            {
                btDataStruc btResponse;
                btResponse.ID = btDataID_VAR_WATCH_RESP;
                btResponse.Counter = btData.Counter;

                if (btData.Data1 == 2)
                {
                    if (core3_var_watch_is_enabled())
                        core3_var_watch_set(false);
                    else
                        core3_var_watch_set(true);
                }
                else if (btData.Data1 == 0 || btData.Data1 == 1)
                    core3_var_watch_set((bool)btData.Data1);

                core3_bt_send_data_len((uint8_t *)&btResponse, sizeof(btDataStruc), true);
            }
            else if (btData.ID == btDataID_VAR_RBOOT)
            {
                btDataStruc btResponse;
                btResponse.ID = btDataID_VAR_RBOOT_RESP;
                btResponse.Counter = btData.Counter;
                core3_bt_send_data_len((uint8_t *)&btResponse, sizeof(btDataStruc), true);

                xTaskCreate(restart_func, "rebooting", 1024 * 4, NULL, 1, NULL);
            }
            else
            {
                // dprintf("[Bluetooth] Unknown btData.ID = %d\n", btData.ID);
            }
        }

        break;
    }

    case ESP_GATTS_EXEC_WRITE_EVT:
        break;

    case ESP_GATTS_RESPONSE_EVT:
        break;

    case ESP_GATTS_MTU_EVT:
        dprintf("[Bluetooth] MTU exchange, MTU %d\n", param->mtu.mtu);
        spp_mtu_size = p_data->mtu.mtu;
        break;

    case ESP_GATTS_CONF_EVT:
        break;

    case ESP_GATTS_UNREG_EVT:
        break;

    case ESP_GATTS_DELETE_EVT:
        break;

    case ESP_GATTS_START_EVT:
        break;

    case ESP_GATTS_STOP_EVT:
        break;

    case ESP_GATTS_CONNECT_EVT:
        dprintf("[Bluetooth] Connected, conn_id %u, remote " ESP_BD_ADDR_STR "\n", param->connect.conn_id,
                ESP_BD_ADDR_HEX(param->connect.remote_bda));

        spp_conn_id = p_data->connect.conn_id;
        spp_gatts_if = gatts_if;
        memcpy(&spp_remote_bda, &p_data->connect.remote_bda, sizeof(esp_bd_addr_t));
        is_connected = true;
        is_advertising = false;
        break;

    case ESP_GATTS_DISCONNECT_EVT:
        dprintf("[Bluetooth] Disconnected, remote " ESP_BD_ADDR_STR ", reason 0x%02x\n",
                ESP_BD_ADDR_HEX(param->disconnect.remote_bda), param->disconnect.reason);

        is_connected = false;
        spp_mtu_size = 23;
        enable_data_ntf = false;
        core3_var_watch_set(false);

        esp_ble_gap_start_advertising(&spp_adv_params);
        is_advertising = true;
        break;

    case ESP_GATTS_OPEN_EVT:
        break;

    case ESP_GATTS_CANCEL_OPEN_EVT:
        break;

    case ESP_GATTS_CLOSE_EVT:
        break;

    case ESP_GATTS_LISTEN_EVT:
        break;

    case ESP_GATTS_CONGEST_EVT:
        break;

    case ESP_GATTS_CREAT_ATTR_TAB_EVT:
    {
        dprintf("[Bluetooth] The number handle 0x%x\n", param->add_attr_tab.num_handle);

        if (param->add_attr_tab.status != ESP_GATT_OK)
        {
            dprintf("[Bluetooth] Create attribute table failed, error code 0x%x\n", param->add_attr_tab.status);
        }
        else if (param->add_attr_tab.num_handle != SPP_IDX_NB)
        {
            dprintf("[Bluetooth] Create attr table abnormally, num_handle (%d) doesn't equal to HRS_IDX_NB(%d)\n",
                    param->add_attr_tab.num_handle, SPP_IDX_NB);
        }
        else
        {
            memcpy(spp_handle_table, param->add_attr_tab.handles, sizeof(spp_handle_table));
            esp_ble_gatts_start_service(spp_handle_table[SPP_IDX_SVC]);
        }

        break;
    }

    default:
        break;
    }
}

static void gatts_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param)
{
    /* If event is register event, store the gatts_if for each profile */
    if (event == ESP_GATTS_REG_EVT)
    {
        if (param->reg.status == ESP_GATT_OK)
        {
            spp_profile_tab[SPP_PROFILE_APP_IDX].gatts_if = gatts_if;
        }
        else
        {
            dprintf("[Bluetooth] Reg app failed, app_id %04x, status %d\n", param->reg.app_id, param->reg.status);
            return;
        }
    }

    do
    {
        int idx;
        for (idx = 0; idx < SPP_PROFILE_NUM; idx++)

        {
            /* ESP_GATT_IF_NONE, not specify a certain gatt_if, need to call every profile cb function */
            if (gatts_if == ESP_GATT_IF_NONE || gatts_if == spp_profile_tab[idx].gatts_if)
            {
                if (spp_profile_tab[idx].gatts_cb)
                {
                    spp_profile_tab[idx].gatts_cb(event, gatts_if, param);
                }
            }
        }
    } while (0);
}

SemaphoreHandle_t sendQueueSemaphore = NULL;
size_t send_queue_len = 0;
size_t send_queue_idx = 0;
size_t send_queue_queued_bytes = 0;
uint8_t *send_queue_memory = NULL;
bool send_queue_valid = false;

void init_bt_queue()
{
    if (send_queue_valid)
        return;

    send_queue_valid = true;
    dprintf("[Bluetooth] Init queue\n");

    send_queue_len = sizeof(btDataStruc) * 11;
    send_queue_idx = 0;
    send_queue_memory = (uint8_t *)malloc(send_queue_len);

    if (send_queue_memory == NULL)
        dprintf("[Bluetooth] send_queue_memory alloc failed\n");

    vSemaphoreCreateBinary(sendQueueSemaphore);

    if (sendQueueSemaphore == NULL)
    {
        dprintf("[Bluetooth][ERR] Cannot create BT queue semaphore\n");
        return;
    }
}

bool core3_bt_send_data_len(uint8_t *dat, int len, bool no_wait)
{
    if (!is_connected)
        return false;

    init_bt_queue();

    if (sendQueueSemaphore == NULL)
        return false;

    if (len != sizeof(btDataStruc))
    {
        dprintf("[Bluetooth][ERR] Sending BT packet of wrong size %d\n", len);
        return false;
    }

    if (no_wait)
    {
        if (esp_ble_gatts_send_indicate(spp_gatts_if, spp_conn_id, spp_handle_table[SPP_IDX_SPP_DATA_NTY_VAL], len,
                                        (uint8_t *)dat, false) != ESP_OK)
        {
            return false;
        }
    }
    else
    {
        bool retVal = false;

        if (xSemaphoreTake(sendQueueSemaphore, 2) == pdTRUE)
        {
            /*if ((send_queue_idx + len) <= send_queue_len)
            {
            }
            else
            {
                send_queue_idx = 0;
            }*/

            if ((send_queue_idx + len) <= send_queue_len)
            {
                memcpy(&send_queue_memory[send_queue_idx], dat, len);
                send_queue_idx += len;
                send_queue_queued_bytes += len;
                retVal = true;
            }
            else
            {
            }

            xSemaphoreGive(sendQueueSemaphore);
        }

        return retVal;
    }

    return true;
}

bool core3_bt_is_connected()
{
    return is_connected;
}

bool core3_bt_is_advertising()
{
    return is_advertising;
}

void bt_send_task(void *arg)
{
    dprintf("[Bluetooth] bt_send_task running\n");

    while (true)
    {
        if (xSemaphoreTake(sendQueueSemaphore, portMAX_DELAY) == pdTRUE)
        {
            if (send_queue_queued_bytes > 0)
            {
                size_t send_len = send_queue_idx;
                send_queue_idx = 0;
                send_queue_queued_bytes = 0;

                // esp_ble_gatts_send_indicate(spp_gatts_if, spp_conn_id, spp_handle_table[SPP_IDX_SPP_DATA_NTY_VAL], len,
                //                     (uint8_t *)dat, false);

                // dprintf("BT Sending len %d\n", send_len);

                if (esp_ble_gatts_send_indicate(spp_gatts_if, spp_conn_id, spp_handle_table[SPP_IDX_SPP_DATA_NTY_VAL], send_len,
                                                (uint8_t *)send_queue_memory, false) != ESP_OK)
                {
                    // dprintf("send_indicate failed\n");
                }

                memset(send_queue_memory, 0, send_queue_len);
            }

            xSemaphoreGive(sendQueueSemaphore);
        }

        vTaskDelay(pdMS_TO_TICKS(40));
    }
}

esp_err_t core3_bt_init()
{
    esp_err_t err = ESP_OK;

    err = esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT);
    if (err != ESP_OK)
    {
        dprintf("[Bluetooth] esp_bt_controller_mem_release FAILED, %X\n", err);
        return err;
    }

    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    err = esp_bt_controller_init(&bt_cfg);
    if (err != ESP_OK)
    {
        dprintf("[Bluetooth] esp_bt_controller_init FAILED, 0x%X\n", err);
        return err;
    }

    err = esp_bt_controller_enable(ESP_BT_MODE_BLE);
    if (err != ESP_OK)
    {
        dprintf("[Bluetooth] esp_bt_controller_enable FAILED, 0x%X\n", err);
        return err;
    }

    // Bluedroid

    esp_bluedroid_config_t bluedroid_cfg = BT_BLUEDROID_INIT_CONFIG_DEFAULT();
    err = esp_bluedroid_init_with_cfg(&bluedroid_cfg);
    if (err != ESP_OK)
    {
        dprintf("[Bluetooth] esp_bluedroid_init_with_cfg FAILED, 0x%X\n", err);
        return err;
    }

    err = esp_bluedroid_enable();
    if (err != ESP_OK)
    {
        dprintf("[Bluetooth] esp_bluedroid_enable FAILED, 0x%X\n", err);
        return err;
    }

    esp_ble_gatts_register_callback(gatts_event_handler);
    esp_ble_gap_register_callback(gap_event_handler);
    esp_ble_gatts_app_register(ESP_SPP_APP_ID);

    err = esp_ble_gatt_set_local_mtu(SPP_GATT_MTU_SIZE);
    if (err != ESP_OK)
    {
        dprintf("[Bluetooth] esp_ble_gatt_set_local_mtu FAILED, 0x%X\n", err);
        return err;
    }

    dprintf("[Bluetooth] OK\n");

    init_bt_queue();
    xTaskCreate(bt_send_task, "bt_send_task", 1024 * 15, NULL, CORE3_BT_SEND_PRIORITY, NULL);
    return ESP_OK;
}