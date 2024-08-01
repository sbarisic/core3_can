#include <core3.h>
#include <core3_can.h>
#include <core3_gmlan.h>
#include <core3_gpio.h>
#include <ecumaster.h>
#include <esp_timer.h>

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

void task_can_receive(void *args)
{
    for (;;)
    {
        core3_can_msg rx_frame;

        if (core3_can_receive(&rx_frame))
        {

            if (core3_can_decode_emu_frame(&rx_frame, &emu_data))
            {
            }
            else if (core3_can_decode_gmlan_frame(&rx_frame, &veh_data))
            {
            }
            else
            {
                /*dprintf("Frame (EXT: %d, RTR: %d)", rx_frame.extd, rx_frame.rtr);
                dprintf(" from 0x%08lX, DLC %d, Data ", rx_frame.identifier, rx_frame.data_length_code);

                for (int i = 0; i < rx_frame.data_length_code; i++)
                    dprintf("0x%02X ", rx_frame.data[i]);

                dprintf("\n");*/
            }
        }

        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

void timer_can_send(void *args)
{
    for (size_t i = 0; i < tx_frames_count; i++)
    {
        int64_t ms = core3_clock_bootms();

        if (tx_frames[i].next_send < ms)
        {
            tx_frames[i].next_send = ms + (int64_t)tx_frames[i].send_interval;
            core3_can_send(&tx_frames[i].frame);
        }
    }
}

/*

*/

void setup_can_channels()
{
    // RPM
    tx_frames[tx_frames_count].frame.identifier = 0xC9;
    tx_frames[tx_frames_count].frame.data_length_code = 8;
    tx_frames[tx_frames_count].frame.data[0] = 0x80;
    tx_frames[tx_frames_count].frame.data[1] = 0x18;
    tx_frames[tx_frames_count].frame.data[2] = 0xA8;
    tx_frames[tx_frames_count].frame.data[3] = 0;
    tx_frames[tx_frames_count].frame.data[4] = 0x1D;
    tx_frames[tx_frames_count].frame.data[5] = 0x50;
    tx_frames[tx_frames_count].frame.data[6] = 0;
    tx_frames[tx_frames_count].frame.data[7] = 0;
    tx_frames[tx_frames_count].send_interval = 12;
    tx_frames_count++;

    // Oil pressure, fuel level
    tx_frames[tx_frames_count].frame.identifier = 0x4D1;
    tx_frames[tx_frames_count].frame.data_length_code = 8;
    tx_frames[tx_frames_count].frame.data[0] = 0xE9;
    tx_frames[tx_frames_count].frame.data[1] = 0;
    tx_frames[tx_frames_count].frame.data[2] = 0;
    tx_frames[tx_frames_count].frame.data[3] = 0;
    tx_frames[tx_frames_count].frame.data[4] = 0;
    tx_frames[tx_frames_count].frame.data[5] = 0;
    tx_frames[tx_frames_count].frame.data[6] = 0;
    tx_frames[tx_frames_count].frame.data[7] = 0;
    tx_frames[tx_frames_count].send_interval = 500;
    tx_frames_count++;

    // Throttle position
    tx_frames[tx_frames_count].frame.identifier = 0x3D1;
    tx_frames[tx_frames_count].frame.data_length_code = 8;
    tx_frames[tx_frames_count].frame.data[0] = 0x11;
    tx_frames[tx_frames_count].frame.data[1] = 0;
    tx_frames[tx_frames_count].frame.data[2] = 0;
    tx_frames[tx_frames_count].frame.data[3] = 0;
    tx_frames[tx_frames_count].frame.data[4] = 0;
    tx_frames[tx_frames_count].frame.data[5] = 0;
    tx_frames[tx_frames_count].frame.data[6] = 0;
    tx_frames[tx_frames_count].frame.data[7] = 0;
    tx_frames[tx_frames_count].send_interval = 100;
    tx_frames_count++;

    // Speed
    tx_frames[tx_frames_count].frame.identifier = 0x3E9;
    tx_frames[tx_frames_count].frame.data_length_code = 8;

    tx_frames[tx_frames_count].frame.data[0] = 0;
    tx_frames[tx_frames_count].frame.data[1] = 0xA0;
    tx_frames[tx_frames_count].frame.data[2] = 0x80;
    tx_frames[tx_frames_count].frame.data[3] = 0x7;
    tx_frames[tx_frames_count].frame.data[4] = 0;
    tx_frames[tx_frames_count].frame.data[5] = 0xA0;
    tx_frames[tx_frames_count].frame.data[6] = 0x80;
    tx_frames[tx_frames_count].frame.data[7] = 0x7;

    /*
        tx_frames[tx_frames_count].frame.data[0] = 0;
        tx_frames[tx_frames_count].frame.data[1] = 0;
        tx_frames[tx_frames_count].frame.data[2] = 0x80;
        tx_frames[tx_frames_count].frame.data[3] = 0;
        tx_frames[tx_frames_count].frame.data[4] = 0;
        tx_frames[tx_frames_count].frame.data[5] = 0;
        tx_frames[tx_frames_count].frame.data[6] = 0x80;
        tx_frames[tx_frames_count].frame.data[7] = 0;
    */

    tx_frames[tx_frames_count].send_interval = 100;
    tx_frames_count++;
}

void app_main()
{
    dprintf("Starting app!\n");

    gpio_set_direction(PIN_5V_EN, GPIO_MODE_OUTPUT);
    gpio_set_level(PIN_5V_EN, 1);

    gpio_set_direction(CAN_SE_PIN, GPIO_MODE_OUTPUT);
    gpio_set_level(CAN_SE_PIN, 0);

    core3_can_init(CORE3_CAN_TIMING_500KBPS, CORE3_CAN_MODE_NORMAL);

    setup_can_channels();

    int priority = 10;
    xTaskCreatePinnedToCore(task_can_receive, "task_can_receive", 1024 * 5, NULL, priority, NULL, 1);
    // xTaskCreatePinnedToCore(task_can_send, "task_can_send", 1024 * 5, NULL, priority, NULL, 1);

    esp_timer_create_args_t timer_can_send_args = {.callback = timer_can_send,
                                                   .arg = NULL,
                                                   .dispatch_method = ESP_TIMER_TASK,
                                                   .name = "timer_can_send",
                                                   .skip_unhandled_events = false};

    esp_timer_handle_t task_can_send_timer;
    ESP_ERROR_CHECK(esp_timer_create(&timer_can_send_args, &task_can_send_timer));

    esp_timer_start_periodic(task_can_send_timer, 1000);

    dprintf("Done!\n");
    while (true)
    {
        dprintf("RPM: %d, MAP: %d, TPS: %d\n", emu_data.RPM, emu_data.MAP, emu_data.TPS);
        // dprintf("TPS: %d)

        vTaskDelay(pdMS_TO_TICKS(500));
    }
}