#include <core3.h>
#include <core3_gpio.h>
#include <core3_can.h>
#include <ecumaster.h>

#include <esp_timer.h>

// ====================================== Variables ======================================
int64_t emu_tstp[8];
emu_data_t emu_data;

// Basic ==========================================================================================================

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
            dprintf("Can frame received!\n");

            if (!core3_can_decode_emu_frame(&rx_frame, &emu_data))
            {
                dprintf("Frame (EXT: %d, RTR: %d)", rx_frame.extd, rx_frame.rtr);
                dprintf(" from 0x%08lX, DLC %d, Data ", rx_frame.identifier, rx_frame.data_length_code);

                for (int i = 0; i < rx_frame.data_length_code; i++)
                    dprintf("0x%02X ", rx_frame.data[i]);

                dprintf("\n");
            }
        }

        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

void task_can_send(void *args)
{
    for (;;)
    {
        core3_can_msg msg;
        msg.identifier = 0x420;
        msg.data_length_code = 4;
        msg.data[0] = 0x1;
        msg.data[0] = 0x2;
        msg.data[0] = 0x3;
        msg.data[0] = 0x4;

        if (core3_can_send(&msg))
            dprintf("Can frame sent!\n");
        else
            dprintf("Failed to send CAN frame\n");

        vTaskDelay(pdMS_TO_TICKS(1500));
    }
}

void app_main()
{
    dprintf("Starting app!\n");

    gpio_set_direction(PIN_5V_EN, GPIO_MODE_OUTPUT);
    gpio_set_level(PIN_5V_EN, 1);

    gpio_set_direction(CAN_SE_PIN, GPIO_MODE_OUTPUT);
    gpio_set_level(CAN_SE_PIN, 0);

    core3_can_init(CORE3_CAN_TIMING_500KBPS, CORE3_CAN_MODE_NORMAL);

    int priority = 10;
    xTaskCreate(task_can_receive, "task_can_receive", 1024 * 5, NULL, priority, NULL);
    // xTaskCreate(task_can_send, "task_can_send", 1024 * 5, NULL, priority, NULL);

    dprintf("Done!\n");
    while (true)
    {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}