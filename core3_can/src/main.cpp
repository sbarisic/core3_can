#include <core3.h>
#include <core3_flash.h>
#include <core3_can.h>
#include <core3_gmlan.h>
#include <core3_gpio.h>
#include <ecumaster.h>
#include <esp_timer.h>

#include <core3_bt.h>
#include <core3_wifi.h>

#include <esp_adc/adc_oneshot.h>

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

static int64_t emu_tstp[8];
// static emu_data_t emu_data;
// static vehicle_data veh_data;

static can_message tx_frames[16];
static int tx_frames_count = 0;

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

int core3_analog()
{
    return 0;
}

void init_gpio_pins()
{
    dprintf("init_gpio_pins()\n");

    // GPIO inputs
    // gpio_set_direction(GPIO0, GPIO_MODE_INPUT);
    // gpio_set_direction(GPIO2, GPIO_MODE_INPUT);

    /*gpio_set_direction(GPIOA0, GPIO_MODE_INPUT);
    gpio_set_pull_mode(GPIOA0, GPIO_FLOATING);

    adc_oneshot_unit_handle_t adc1_handle;
    adc_oneshot_unit_init_cfg_t init_config1 = {
        .unit_id = ADC_UNIT_1,
        .clk_src = (adc_oneshot_clk_src_t)0,
        .ulp_mode = ADC_ULP_MODE_DISABLE,
    };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config1, &adc1_handle));
    vTaskDelay(pdMS_TO_TICKS(10));

    adc_oneshot_chan_cfg_t config = {
        .atten = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_DEFAULT};
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc1_handle, GPIOA0_CH, &config));
    vTaskDelay(pdMS_TO_TICKS(10));

    int an_val = 0;
    if (adc_oneshot_read(adc1_handle, GPIOA0_CH, &an_val) == ESP_OK)
    {
        dprintf("A0 = %d\n", an_val);
    }
    else
    {
        dprintf("Read failed\n");
    }*/
}

void core3_program(void *arg)
{
    core3_flash_init();

    dprintf("Cal string: %s\n", (const char *)core3_flash_cal_offset(0x0));

    core3_bt_init();

    core3_can_init(CORE3_CAN_TIMING_33_3KBPS, CORE3_CAN_MODE_NORMAL);
    setup_can_channels();

    dprintf("Done!\n");
    while (true)
    {
        // dprintf("RPM: %d, MAP: %d, TPS: %d\n", emu_data.RPM, emu_data.MAP, emu_data.TPS);
        //  dprintf("TPS: %d)

        // can_channel_turn_on_IPC();

        if (core3_bt_is_connected())
        {
            core3_can_msg msg;
            if (core3_can_rx_dequeue(&msg))
            {
                dprintf("Received CAN message!\n");
            }
        }

        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

void app_main()
{
    dprintf("Starting app!\n");

    gpio_set_direction(PIN_5V_EN, GPIO_MODE_OUTPUT);
    gpio_set_level(PIN_5V_EN, 1);

    gpio_set_direction(SDCARD_PIN_CS, GPIO_MODE_OUTPUT);
    gpio_set_level(SDCARD_PIN_CS, 1);

    vTaskDelay(pdMS_TO_TICKS(250));

    init_gpio_pins();

    core3_init();

    xTaskCreate(core3_program, "core3_program", 1024 * 60, NULL, CORE3_PROGRAM_PRIORITY, NULL);
}