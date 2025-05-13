#include <core3.h>
#include <core3_can.h>
#include <core3_gmlan.h>
#include <core3_gpio.h>
#include <ecumaster.h>
#include <esp_timer.h>

#include <core3_bt.h>
#include <core3_wifi.h>

#define LED_PIN WS2812_PIN // digital pin used to drive the LED strip
#define LED_COUNT 1        // number of LEDs on the strip
#define RGB(R, G, B) ((R << 16) | (G << 8) | B)

typedef struct
{
    core3_can_msg frame;
    int64_t next_send;
    int16_t send_interval;
} can_message;

// ====================================== Variables ======================================

int64_t emu_tstp[8];
emu_data_t emu_data;
vehicle_data veh_data;

can_message tx_frames[16];
int tx_frames_count = 0;

// Basic ==========================================================================================================

int64_t min64(int64_t a, int64_t b)
{
    if (a < b)
        return a;
    else
        return b;
}

int64_t core3_clock_bootms()
{
    return (int64_t)(esp_timer_get_time() / 1000);
}

int64_t timestamp_get(uint32_t can_id)
{
    return emu_tstp[can_id - EMU_BASE];
}

void timestamp_set(uint32_t can_id, int64_t val)
{
    emu_tstp[can_id - EMU_BASE] = val;
}

int64_t timestamp_get_last(uint32_t can_id)
{
    return core3_clock_bootms() - timestamp_get(can_id);
}

void timer_can_send(void *args)
{
    for (size_t i = 0; i < tx_frames_count; i++)
    {
        int64_t ms = core3_clock_bootms();

        if (tx_frames[i].next_send < ms)
        {
            // dprintf("SEND 0x%lx\n", tx_frames[i].frame.identifier);

            tx_frames[i].next_send = ms + (int64_t)tx_frames[i].send_interval;
            core3_can_send(&tx_frames[i].frame);
        }
    }
}

void setup_can_channels()
{
    tx_frames_count = 0;

    /*// Wakeup
    tx_frames[tx_frames_count].frame.identifier = 0x100;
    tx_frames[tx_frames_count].frame.data_length_code = 8;
    tx_frames[tx_frames_count].frame.data[0] = 0;
    tx_frames[tx_frames_count].frame.data[1] = 0;
    tx_frames[tx_frames_count].frame.data[2] = 0;
    tx_frames[tx_frames_count].frame.data[3] = 0;
    tx_frames[tx_frames_count].frame.data[4] = 0;
    tx_frames[tx_frames_count].frame.data[5] = 0;
    tx_frames[tx_frames_count].frame.data[6] = 0;
    tx_frames[tx_frames_count].frame.data[7] = 0;
    tx_frames[tx_frames_count].send_interval = 1000;
    tx_frames_count++;

    // Wakeup
    tx_frames[tx_frames_count].frame.identifier = 0x101;
    tx_frames[tx_frames_count].frame.data_length_code = 4;
    tx_frames[tx_frames_count].frame.data[0] = 0xFD;
    tx_frames[tx_frames_count].frame.data[1] = 0x02;
    tx_frames[tx_frames_count].frame.data[2] = 0x10;
    tx_frames[tx_frames_count].frame.data[3] = 0x04;
    tx_frames[tx_frames_count].frame.data[4] = 0;
    tx_frames[tx_frames_count].frame.data[5] = 0;
    tx_frames[tx_frames_count].frame.data[6] = 0;
    tx_frames[tx_frames_count].frame.data[7] = 0;
    tx_frames[tx_frames_count].send_interval = 1000;
    tx_frames_count++;

    // Wakeup
    tx_frames[tx_frames_count].frame.identifier = 0x632;
    tx_frames[tx_frames_count].frame.data_length_code = 4;
    tx_frames[tx_frames_count].frame.data[0] = 0;
    tx_frames[tx_frames_count].frame.data[1] = 0x48;
    tx_frames[tx_frames_count].frame.data[2] = 0x50;
    tx_frames[tx_frames_count].frame.data[3] = 0;
    tx_frames[tx_frames_count].frame.data[4] = 0;
    tx_frames[tx_frames_count].frame.data[5] = 0;
    tx_frames[tx_frames_count].frame.data[6] = 0;
    tx_frames[tx_frames_count].frame.data[7] = 0;
    tx_frames[tx_frames_count].send_interval = 1000;
    tx_frames_count++;

    // Wakeup
    tx_frames[tx_frames_count].frame.identifier = 0x170;
    tx_frames[tx_frames_count].frame.data_length_code = 3;
    tx_frames[tx_frames_count].frame.data[0] = 0x60;
    tx_frames[tx_frames_count].frame.data[1] = 0x00;
    tx_frames[tx_frames_count].frame.data[2] = 0x00;
    tx_frames[tx_frames_count].frame.data[3] = 0;
    tx_frames[tx_frames_count].frame.data[4] = 0;
    tx_frames[tx_frames_count].frame.data[5] = 0;
    tx_frames[tx_frames_count].frame.data[6] = 0;
    tx_frames[tx_frames_count].frame.data[7] = 0;
    tx_frames[tx_frames_count].send_interval = 100;
    tx_frames_count++;
    //*/
}

void can_channel_test()
{

    can_message frame;
    frame.frame.identifier = 0x108;
    frame.frame.data_length_code = 8;
    frame.frame.data[0] = 0x23;
    frame.frame.data[1] = 0x20;
    frame.frame.data[2] = 0x98;
    frame.frame.data[3] = 0x00;
    frame.frame.data[4] = 0x04;
    frame.frame.data[5] = 0xE5;
    frame.frame.data[6] = 0x00;
    frame.frame.data[7] = 0x00;
    core3_can_send(&frame.frame);
}

void can_channel_turn_on_IPC()
{

    can_message frame;
    frame.frame.identifier = 0x148;
    frame.frame.data_length_code = 8;
    frame.frame.data[0] = 0x20;
    frame.frame.data[1] = 0x20;
    frame.frame.data[2] = 0x20;
    frame.frame.data[3] = 0x20;
    frame.frame.data[4] = 0x36;
    frame.frame.data[5] = 0x36;
    frame.frame.data[6] = 0xB0;
    frame.frame.data[7] = 0x46;
    core3_can_send(&frame.frame);

    frame.frame.identifier = 0x142;
    frame.frame.data_length_code = 8;
    frame.frame.data[0] = 0x43;
    frame.frame.data[1] = 0x4F;
    frame.frame.data[2] = 0x4F;
    frame.frame.data[3] = 0x4C;
    frame.frame.data[4] = 0x41;
    frame.frame.data[5] = 0x4E;
    frame.frame.data[6] = 0x54;
    frame.frame.data[7] = 0x20;
    core3_can_send(&frame.frame);
}

void print_runtime()
{
    int64_t us = esp_timer_get_time();
    float s = us / 1000000.0f;
    dprintf("Time since boot: %.2f s\n", s);
}

void app_main()
{
    dprintf("Starting app!\n");
    core3_init();

    gpio_set_direction(PIN_5V_EN, GPIO_MODE_OUTPUT);
    gpio_set_level(PIN_5V_EN, 1);

    gpio_set_direction(CAN_SE_PIN, GPIO_MODE_OUTPUT);
    gpio_set_level(CAN_SE_PIN, 0);

    core3_bt_init();

    core3_can_init(CORE3_CAN_TIMING_33_3KBPS, CORE3_CAN_MODE_NORMAL);
    setup_can_channels();

    esp_timer_create_args_t timer_can_send_args = {.callback = timer_can_send,
                                                   .arg = NULL,
                                                   .dispatch_method = ESP_TIMER_TASK,
                                                   .name = "timer_can_send",
                                                   .skip_unhandled_events = false};

    esp_timer_handle_t task_can_send_timer;
    ESP_ERROR_CHECK(esp_timer_create(&timer_can_send_args, &task_can_send_timer));

    esp_timer_start_periodic(task_can_send_timer, 1000 * 2);

    // print_runtime();

    /*if (core3_wifi_init() == ESP_OK)
    {
        dprintf("Delaying until WiFi connected ... ");

        if (core3_wifi_delay_until_connected())
            dprintf("OK\n");
        else
            dprintf("FAIL\n");
    }

    print_runtime();*/

    int counter = 0;
    char print_buf[512];

    dprintf("Done!\n");
    while (true)
    {
        // dprintf("RPM: %d, MAP: %d, TPS: %d\n", emu_data.RPM, emu_data.MAP, emu_data.TPS);
        //  dprintf("TPS: %d)

        // can_channel_turn_on_IPC();

        if (core3_bt_is_connected())
        {
            sprintf(print_buf, "Hello BLE Data %d\n", counter++);
            core3_bt_send_data_len((uint8_t *)print_buf, strlen(print_buf));

            core3_can_msg msg;
            if (core3_can_rx_dequeue(&msg))
            {
                dprintf("Received CAN message!\n");
            }
        }

        vTaskDelay(pdMS_TO_TICKS(50));
    }
}