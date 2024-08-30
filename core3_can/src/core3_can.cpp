#include <core3.h>
#include <core3_can.h>
#include <ecumaster.h>

#include "driver/gpio.h"
#include "driver/twai.h"

bool core3_can_send(core3_can_msg *msg)
{
    if (msg == NULL)
        return false;

    twai_message_t message;

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

int core3_can_init(core3_can_timing timing, core3_can_mode mode)
{
    dprintf("core3_can_init\n");

    twai_general_config_t g_config = TWAI_GENERAL_CONFIG_DEFAULT(CAN_TX_PIN, CAN_RX_PIN, (twai_mode_t)mode);
    g_config.tx_queue_len = 0;
    g_config.rx_queue_len = 16;

    twai_filter_config_t f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL();
    twai_timing_config_t t_config;

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

    dprintf("core3_can_init - CAN ok\n");
    return ESP_OK;
}