#include <core3.h>
#include <core3_can.h>
#include <ecumaster.h>

#include "driver/gpio.h"
#include "driver/twai.h"

#define BYTE_TO_BINARY_PATTERN "%c%c%c%c%c%c%c%c"
#define BYTE_TO_BINARY(byte)         \
    ((byte) & 0x80 ? '1' : '0'),     \
        ((byte) & 0x40 ? '1' : '0'), \
        ((byte) & 0x20 ? '1' : '0'), \
        ((byte) & 0x10 ? '1' : '0'), \
        ((byte) & 0x08 ? '1' : '0'), \
        ((byte) & 0x04 ? '1' : '0'), \
        ((byte) & 0x02 ? '1' : '0'), \
        ((byte) & 0x01 ? '1' : '0')

typedef enum
{
    CAN_INTERV_10MS = 10,
    CAN_INTERV_20MS = 20,
    CAN_INTERV_50MS = 50,
    CAN_INTERV_100MS = 100,
    CAN_INTERV_500MS = 500,
    CAN_INTERV_1000MS = 1000,

} canIntervalMs_t;

typedef struct
{
    core3_can_msg msg;

    canIntervalMs_t send_interval_ms;
    uint32_t last_sent_ms;

    int counter_offset_byte;
    int counter_offset_bit;
    int counter_bits;

    void (*onUpdate)(core3_can_msg *msg);

} can_message_ex;

typedef struct
{
    union
    {
        uint8_t raw;
        struct
        {
            bool b0 : 1;
            bool b1 : 1;
            bool b2 : 1;
            bool b3 : 1;
            bool b4 : 1;
            bool b5 : 1;
            bool b6 : 1;
            bool b7 : 1;
        } bits;
    };
} PACKED_ATTR bytebits;

QueueHandle_t rx_queue = NULL;

static int msg_tx_list_size = 32;
static int msg_tx_list_count = 0;
static can_message_ex msg_tx_list[32];

static bool canSw1 = false;
static bool canSw2 = false;
static bool canSw3 = false;
static bool canSw4 = false;

can_message_ex *core3_can_msg_create()
{
    if (msg_tx_list_count >= msg_tx_list_size)
    {
        return NULL;
    }

    can_message_ex *msg = &msg_tx_list[msg_tx_list_count];
    msg_tx_list_count++;

    msg->onUpdate = NULL;
    msg->counter_bits = 0;
    msg->counter_offset_bit = -1;
    msg->counter_offset_byte = -1;
    return msg;
}

void core3_can_msg_ecumaster_dbw(core3_can_msg *msg, uint8_t dbw_target)
{
    msg->identifier = 0x667;
    msg->data_length_code = 2;

    for (size_t i = 0; i < 8; i++)
    {
        msg->data[i] = 0;
    }

    if (dbw_target != 0)
    {
        uint16_t dbw_tgt = (dbw_target * 1000) / 255;
        msg->data[0] = (dbw_tgt >> 8) & 0xFF;
        msg->data[1] = (dbw_tgt) & 0xFF;
    }
}

uint8_t core3_bits_mask(uint8_t bits)
{
    uint8_t mask = 0;

    for (size_t i = 0; i < bits; i++)
    {
        mask = (mask << 1) | 0x1;
    }

    return mask;
}

void core3_read_bits(uint8_t byte, uint8_t *bits)
{
    bytebits bb = {0};

    bb.raw = byte;
    bits[0] = bb.bits.b0;
    bits[1] = bb.bits.b1;
    bits[2] = bb.bits.b2;
    bits[3] = bb.bits.b3;
    bits[4] = bb.bits.b4;
    bits[5] = bb.bits.b5;
    bits[6] = bb.bits.b6;
    bits[7] = bb.bits.b7;
}

void core3_write_bits(uint8_t *byte, uint8_t *bits)
{
    bytebits bb = {0};

    bb.bits.b0 = bits[0] > 0x0;
    bb.bits.b1 = bits[1] > 0x0;
    bb.bits.b2 = bits[2] > 0x0;
    bb.bits.b3 = bits[3] > 0x0;
    bb.bits.b4 = bits[4] > 0x0;
    bb.bits.b5 = bits[5] > 0x0;
    bb.bits.b6 = bits[6] > 0x0;
    bb.bits.b7 = bits[7] > 0x0;

    *byte = bb.raw;
}

void core3_can_print(core3_can_msg *msg)
{
    dprintf("ID 0x%04X DLC %d - " BYTE_TO_BINARY_PATTERN " " BYTE_TO_BINARY_PATTERN " " BYTE_TO_BINARY_PATTERN " " BYTE_TO_BINARY_PATTERN " " BYTE_TO_BINARY_PATTERN " " BYTE_TO_BINARY_PATTERN " " BYTE_TO_BINARY_PATTERN " " BYTE_TO_BINARY_PATTERN "\n",
            (unsigned int)msg->identifier,
            msg->data_length_code,
            BYTE_TO_BINARY(msg->data[0]),
            BYTE_TO_BINARY(msg->data[1]),
            BYTE_TO_BINARY(msg->data[2]),
            BYTE_TO_BINARY(msg->data[3]),
            BYTE_TO_BINARY(msg->data[4]),
            BYTE_TO_BINARY(msg->data[5]),
            BYTE_TO_BINARY(msg->data[6]),
            BYTE_TO_BINARY(msg->data[7]));

    dprintf("                  %02X, %02X, %02X, %02X, %02X, %02X, %02X, %02X\n",
            msg->data[0],
            msg->data[1],
            msg->data[2],
            msg->data[3],
            msg->data[4],
            msg->data[5],
            msg->data[6],
            msg->data[7]);
}

void core3_can_update(can_message_ex *msg)
{
    if (msg->onUpdate != NULL)
        msg->onUpdate(&msg->msg);

    if (msg->counter_bits >= 0 && msg->counter_offset_byte >= 0 && msg->counter_offset_bit >= 0)
    {
        uint8_t cnt_byte = msg->msg.data[msg->counter_offset_byte];
        uint8_t bitmask = core3_bits_mask(msg->counter_bits) << msg->counter_offset_bit;

        // dprintf("cnt_byte = %2X, bitmask = %2X\n", cnt_byte, bitmask);

        uint8_t cnt = (cnt_byte & bitmask) >> msg->counter_offset_bit;

        // dprintf("cnt = %2X, cnt + 1 = %2X\n", cnt, cnt + 1);
        cnt = cnt + 1;
        cnt = (cnt) & (bitmask >> msg->counter_offset_bit);

        cnt_byte = cnt_byte & ~bitmask;
        cnt_byte = cnt_byte | (cnt << msg->counter_offset_bit);

        msg->msg.data[msg->counter_offset_byte] = cnt_byte;
    }
}

void canUpdate_dbw(core3_can_msg *msg)
{
    core3_can_msg_ecumaster_dbw(msg, 255);
}

void canUpdate_ecuOutput(core3_can_msg *msg)
{
    msg->data_length_code = 8;

    msg->data[0] = core3_octane_factor_get();
    msg->data[1] = core3_long_term_fuel_trim();
    msg->data[2] = 0;
    msg->data[3] = 0;

    // Switches
    msg->data[4] = canSw1 ? 0xFF : 0x0;
    msg->data[5] = canSw2 ? 0xFF : 0x0;
    msg->data[6] = canSw3 ? 0xFF : 0x0;
    msg->data[7] = canSw4 ? 0xFF : 0x0;
}

void core3_can_test()
{
    can_message_ex *msg = core3_can_msg_create();
    msg->counter_bits = 3;
    msg->counter_offset_bit = 0;
    msg->counter_offset_byte = 0;
    msg->msg.identifier = 0x69;
    msg->msg.data_length_code = 8;
    msg->send_interval_ms = CAN_INTERV_1000MS;

    uint8_t bits[8];
    bits[0] = 1;
    bits[1] = 0;
    bits[2] = 1;
    bits[3] = 0;
    bits[4] = 1;
    bits[5] = 1;
    bits[6] = 1;
    bits[7] = 1;
    core3_write_bits(&msg->msg.data[1], bits);

    msg = core3_can_msg_create();
    msg->send_interval_ms = CAN_INTERV_500MS;
    msg->onUpdate = canUpdate_dbw;

    // ECU -> Ecumaster Analog/Switches
    msg = core3_can_msg_create();
    msg->send_interval_ms = CAN_INTERV_500MS;
    msg->msg.identifier = 0x760;
    msg->onUpdate = canUpdate_ecuOutput;

    // Opel messages
}

bool core3_can_send(core3_can_msg *msg)
{
    if (msg == NULL)
        return false;

    core3_can_print(msg);
    return true;

    /*twai_message_t message;

    memcpy(message.data, msg->data, TWAI_FRAME_MAX_DLC);
    message.data_length_code = msg->data_length_code;
    message.dlc_non_comp = msg->dlc_non_comp;
    message.extd = msg->extd;
    message.identifier = msg->identifier;
    message.reserved = msg->reserved;
    message.rtr = msg->rtr;
    message.self = msg->self;
    message.ss = msg->ss;

    esp_err_t err = twai_transmit(&message, portMAX_DELAY);

    if (err == ESP_OK)
        return true;

    // dprintf("core3_can_send esp_err: 0x%X\n", err);
    return false;*/
}

bool core3_can_rx_enqueue(core3_can_msg *msg)
{
    if (msg == NULL)
    {
        dprintf("core3_can_rx_enqueue - NULL pointer\n");
        return false;
    }

    if (rx_queue == NULL)
    {
        rx_queue = xQueueCreate(32, sizeof(core3_can_msg));

        if (rx_queue == NULL)
        {
            dprintf("core3_can_rx_enqueue - queue could not be created\n");
            return false;
        }
    }

    if (xQueueSendToBack(rx_queue, (const void *)msg, pdMS_TO_TICKS(5)) == pdPASS)
    {
        return true;
    }

    dprintf("core3_can_rx_enqueue - Enqueue failed\n");
    return false;
}

bool core3_can_rx_dequeue(core3_can_msg *msg)
{
    if (msg == NULL)
    {
        dprintf("core3_can_rx_dequeue - NULL pointer\n");
        return false;
    }

    memset(msg, 0, sizeof(core3_can_msg));

    if (rx_queue == NULL)
    {
        // dprintf("core3_can_rx_dequeue - Queue not created\n");
        return false;
    }

    if (xQueueReceive(rx_queue, (void *)msg, portMAX_DELAY) == pdPASS)
    {
        return true;
    }

    dprintf("core3_can_rx_dequeue - Dequeue failed\n");
    return false;
}

bool core3_can_receive(core3_can_msg *msg)
{
    if (msg == NULL)
        return false;

    twai_message_t message;
    esp_err_t err = twai_receive(&message, portMAX_DELAY);

    if (err == ESP_OK)
    {
        // dprintf("Frame received!\n");

        memcpy(msg->data, message.data, TWAI_FRAME_MAX_DLC);
        msg->data_length_code = message.data_length_code;
        msg->dlc_non_comp = message.dlc_non_comp;
        msg->extd = message.extd;
        msg->identifier = message.identifier;
        msg->reserved = message.reserved;
        msg->rtr = message.rtr;
        msg->self = message.self;
        msg->ss = message.ss;

        return true;
    }

    // dprintf("core3_can_receive esp_err: 0x%X\n", err);
    return false;
}

void parse_ecumaster_msg(core3_can_msg *msg)
{
    emu_data_t emu;
    if (core3_can_decode_emu_frame(msg, &emu))
    {
        core3_ecu_add_ecumaster_frame(emu);
    }
}

void core3_can_task_receive(void *args)
{
    while (true)
    {
        core3_can_msg msg;
        if (core3_can_receive(&msg))
        {
            // TODO: Process known received CAN frames

            /**if (core3_can_decode_emu_frame(&rx_frame, &emu_data))
           {
               dprintf("EcuMaster Frame\n");
           }
           else if (core3_can_decode_gmlan_frame(&rx_frame, &veh_data))
           {
               dprintf("GMLAN Frame\n");
           }
           else {}*/

            if (msg.extd != 1)
            {
                parse_ecumaster_msg(&msg);

                // core3_can_rx_enqueue(&msg);
            }
        }
    }
}

void core3_can_task_send(void *args)
{
    while (true)
    {
        uint32_t ms = core3_time_ms();

        for (size_t i = 0; i < msg_tx_list_count; i++)
        {
            can_message_ex *msg = &msg_tx_list[i];

            if (ms >= msg->last_sent_ms + msg->send_interval_ms)
            {
                core3_can_update(msg);
                msg->last_sent_ms = ms;

                core3_can_send(&msg->msg);
            }
        }

        // TODO: Send CAN frames
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

int core3_can_init(core3_can_timing timing, core3_can_mode mode)
{
    dprintf("core3_can_init\n");

    twai_general_config_t g_config = TWAI_GENERAL_CONFIG_DEFAULT(CAN_TX_PIN, CAN_RX_PIN, (twai_mode_t)mode);
    g_config.tx_queue_len = 0;
    g_config.rx_queue_len = 16;

    twai_filter_config_t f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL();
    twai_timing_config_t t_config;

    core3_can_test();

    switch (timing)
    {
    case CORE3_CAN_TIMING_25KBPS:
        t_config = TWAI_TIMING_CONFIG_25KBITS();
        break;

    case CORE3_CAN_TIMING_33_3KBPS:
        memset((void *)&t_config, 0, sizeof(twai_timing_config_t));
        t_config.brp = 120;
        t_config.tseg_1 = 15;
        t_config.tseg_2 = 4;
        t_config.sjw = 3;
        t_config.triple_sampling = false;
        break;

    case CORE3_CAN_TIMING_50KBPS:
        t_config = TWAI_TIMING_CONFIG_50KBITS();
        break;

    case CORE3_CAN_TIMING_100KBPS:
        t_config = TWAI_TIMING_CONFIG_100KBITS();
        break;

    case CORE3_CAN_TIMING_125KBPS:
        t_config = TWAI_TIMING_CONFIG_125KBITS();
        break;

    case CORE3_CAN_TIMING_250KBPS:
        t_config = TWAI_TIMING_CONFIG_250KBITS();
        break;

    case CORE3_CAN_TIMING_500KBPS:
        t_config = TWAI_TIMING_CONFIG_500KBITS();
        break;

    case CORE3_CAN_TIMING_800KBPS:
        t_config = TWAI_TIMING_CONFIG_800KBITS();
        break;

    case CORE3_CAN_TIMING_1MBPS:
        t_config = TWAI_TIMING_CONFIG_1MBITS();
        break;

    default:
        dprintf("core3_can_init - Unknown timing %d\n", (int)timing);
        return ESP_FAIL;
    }

    if (twai_driver_install(&g_config, &t_config, &f_config) != ESP_OK)
    {
        dprintf("core3_can_init - Driver install failed\n");
        return ESP_FAIL;
    }

    // gpio_pulldown_en(CAN_RX_PIN);
    // gpio_pulldown_en(CAN_TX_PIN);

    if (twai_start() != ESP_OK)
    {
        dprintf("core3_can_init - Failed to start driver\n");
        return ESP_FAIL;
    }

    xTaskCreate(core3_can_task_receive, "core3_can_task_receive", 1024 * 5, NULL, CORE3_CAN_RECEIVE_PRIORITY, NULL);
    xTaskCreate(core3_can_task_send, "core3_can_task_send", 1024 * 5, NULL, CORE3_CAN_SEND_PRIORITY, NULL);

    dprintf("core3_can_init - CAN ok\n");
    return ESP_OK;
}