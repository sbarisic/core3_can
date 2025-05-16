#include <core3.h>
#include <core3_can.h>
#include <core3_flash.h>
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

typedef struct
{
    uint32_t ID;
    uint32_t *ValPtr;
    float Time;
    char Name[8];
} watcher_var;

static size_t var_count = 0;
static watcher_var variables[16];
static core3_io_digital core3_io_digitals[16];

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

adc_oneshot_unit_handle_t adc1_handle;
adc_cali_handle_t adc1_cali_handle;

int core3_analog(adc_channel_t analog, float *volt)
{
    int an_val = 0;
    adc_oneshot_read(adc1_handle, analog, &an_val);

    int mv = 0;
    if (adc_cali_raw_to_voltage(adc1_cali_handle, an_val, &mv) == ESP_OK)
    {
        *volt = mv / 1000.0f;
    }

    return an_val;
}

void init_gpio_pins()
{
    dprintf("init_gpio_pins()\n");

    // GPIO inputs
    // gpio_set_direction(GPIO0, GPIO_MODE_INPUT);
    // gpio_set_direction(GPIO2, GPIO_MODE_INPUT);

    gpio_set_direction(GPIOA0, GPIO_MODE_INPUT);
    gpio_set_pull_mode(GPIOA0, GPIO_FLOATING);

    gpio_set_direction(GPIOA1, GPIO_MODE_INPUT);
    gpio_set_pull_mode(GPIOA1, GPIO_FLOATING);

    gpio_set_direction(GPIOA2, GPIO_MODE_INPUT);
    gpio_set_pull_mode(GPIOA2, GPIO_FLOATING);

    gpio_set_direction(GPIOA3, GPIO_MODE_INPUT);
    gpio_set_pull_mode(GPIOA3, GPIO_FLOATING);

    adc_oneshot_unit_init_cfg_t init_config1 = {
        .unit_id = ADC_UNIT_1,
        .clk_src = (adc_oneshot_clk_src_t)0,
        .ulp_mode = ADC_ULP_MODE_DISABLE,
    };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config1, &adc1_handle));
    vTaskDelay(pdMS_TO_TICKS(10));

    adc_oneshot_chan_cfg_t config = {.atten = ADC_ATTEN_DB_12, .bitwidth = ADC_BITWIDTH_DEFAULT};

    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc1_handle, GPIOA0_CH, &config));
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc1_handle, GPIOA1_CH, &config));
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc1_handle, GPIOA2_CH, &config));
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc1_handle, GPIOA3_CH, &config));

    vTaskDelay(pdMS_TO_TICKS(10));

    dprintf("Calibration scheme version is %s", "Line Fitting");
    adc_cali_line_fitting_config_t cali_config = {
        .unit_id = ADC_UNIT_1,
        .atten = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
        .default_vref = 0};
    ESP_ERROR_CHECK(adc_cali_create_scheme_line_fitting(&cali_config, &adc1_cali_handle));
}

bool core3_var_set(const char *name, uint32_t var, uint32_t val, float time)
{
    for (size_t i = 0; i < var_count; i++)
    {
        if (variables[i].ID == var)
        {
            *variables[i].ValPtr = val;
            variables[i].Time = time;
            memcpy(variables[i].Name, name, 8);
            return true;
        }
    }

    size_t newidx = var_count++;
    variables[newidx].ID = var;
    variables[newidx].ValPtr = &core3_io_digitals[newidx].raw_value;
    variables[newidx].Time = time;
    memcpy(variables[newidx].Name, name, 8);
    return true;
}

uint32_t core3_var_get(uint32_t var)
{
    for (size_t i = 0; i < var_count; i++)
    {
        if (variables[i].ID == var)
        {
            return *variables[i].ValPtr;
        }
    }

    return 0;
}

static volatile btDataStruc *btResponse;
static uint8_t can_heartbeat = 0;

static uint64_t ms = 0;
static uint64_t var_stream_last = 0;
static uint64_t var_stream_interval = 40;

static uint64_t can_stream_last = 0;
static uint64_t can_stream_interval = 60;

static uint64_t io_poll_last = 0;
static uint64_t io_poll_interval = 80;

static uint64_t logic_last = 0;
static uint64_t logic_interval = 80;

static bool var_watch_enabled = false;

bool core3_var_watch_is_enabled()
{
    return var_watch_enabled;
}

void core3_var_watch_set(bool enabled)
{
    var_watch_enabled = enabled;
}

void core3_io_digital_calc(core3_io_digital *dig)
{
    if (dig->trigger_value == 0)
        return;

    if (dig->value == 0x0)
    {
        if (dig->raw_value > dig->trigger_value + dig->hyst)
            dig->value = 0xFF;
    }
    else
    {
        if (dig->raw_value < dig->trigger_value - dig->hyst)
            dig->value = 0x0;
    }
}

void core3_tick(TimerHandle_t timer)
{
    ms = esp_timer_get_time() / 1000;

    // dprintf("RPM: %d, MAP: %d, TPS: %d\n", emu_data.RPM, emu_data.MAP, emu_data.TPS);
    //  dprintf("TPS: %d)

    // can_channel_turn_on_IPC();

    if (ms >= logic_last + logic_interval)
    {
        logic_last = ms;

        for (size_t i = 0; i < sizeof(core3_io_digitals) / sizeof(*core3_io_digitals); i++)
        {
            core3_io_digitals[i].trigger_value = 1000;
            core3_io_digitals[i].hyst = 100;
            core3_io_digital_calc(&core3_io_digitals[i]);
        }

        uint32_t base_can_id = 0x640;
        int dig_id = 8;

        if (core3_io_digitals[dig_id].can_sent == 0xFF && core3_io_digitals[dig_id].can_sent == 0xFF &&
            core3_io_digitals[dig_id].can_sent == 0xFF)
        {
            core3_io_digitals[dig_id].can_sent = 0;
            core3_io_digitals[dig_id].hyst = 0;
            core3_io_digitals[dig_id].trigger_value = 0;
            core3_io_digitals[dig_id].can_id = base_can_id;
            ((uint16_t *)core3_io_digitals[dig_id].can_data)[0] = (uint16_t)core3_var_get(CORE3_VAR_ANALOG0);
            ((uint16_t *)core3_io_digitals[dig_id].can_data)[1] = (uint16_t)core3_var_get(CORE3_VAR_ANALOG1);
            ((uint16_t *)core3_io_digitals[dig_id].can_data)[2] = (uint16_t)core3_var_get(CORE3_VAR_ANALOG2);
            ((uint16_t *)core3_io_digitals[dig_id].can_data)[3] = (uint16_t)core3_var_get(CORE3_VAR_ANALOG3);
            dig_id++;

            core3_io_digitals[dig_id].can_sent = 0;
            core3_io_digitals[dig_id].hyst = 0;
            core3_io_digitals[dig_id].trigger_value = 0;
            core3_io_digitals[dig_id].can_id = base_can_id + 1;
            ((uint16_t *)core3_io_digitals[dig_id].can_data)[0] = core3_io_digitals[0].value;
            ((uint16_t *)core3_io_digitals[dig_id].can_data)[1] = core3_io_digitals[1].value;
            ((uint16_t *)core3_io_digitals[dig_id].can_data)[2] = core3_io_digitals[2].value;
            ((uint16_t *)core3_io_digitals[dig_id].can_data)[3] = core3_io_digitals[3].value;

            core3_io_digitals[dig_id].can_sent = 0;
            core3_io_digitals[dig_id].hyst = 0;
            core3_io_digitals[dig_id].trigger_value = 0;
            core3_io_digitals[dig_id].can_id = base_can_id + 2;
            (core3_io_digitals[dig_id].can_data)[0] = 0x0;
            (core3_io_digitals[dig_id].can_data)[1] = 0x0;
            (core3_io_digitals[dig_id].can_data)[2] = 0x0;
            (core3_io_digitals[dig_id].can_data)[3] = 0x0;
            (core3_io_digitals[dig_id].can_data)[4] = 0x0;
            (core3_io_digitals[dig_id].can_data)[5] = 0x0;
            (core3_io_digitals[dig_id].can_data)[6] = 0x0;
            (core3_io_digitals[dig_id].can_data)[7] = can_heartbeat++;
            dig_id++;
        }
    }

    if (ms >= io_poll_last + io_poll_interval)
    {
        io_poll_last = ms;

        float time = ms / 1000.0f;
        float v0, v1, v2, v3;

        core3_analog(GPIOA0_CH, &v0);
        core3_analog(GPIOA1_CH, &v1);
        core3_analog(GPIOA2_CH, &v2);
        core3_analog(GPIOA3_CH, &v3);

        core3_var_set("Analog0 ", CORE3_VAR_ANALOG0, *(uint32_t *)&v0, time);
        core3_var_set("Analog1 ", CORE3_VAR_ANALOG1, *(uint32_t *)&v1, time);
        core3_var_set("Analog2 ", CORE3_VAR_ANALOG2, *(uint32_t *)&v2, time);
        core3_var_set("Analog3 ", CORE3_VAR_ANALOG3, *(uint32_t *)&v3, time);

        core3_var_set("Digital0", CORE3_VAR_DIG0, core3_io_digitals[8].value, time);
        core3_var_set("Digital1", CORE3_VAR_DIG1, core3_io_digitals[8 + 1].value, time);
        core3_var_set("Digital2", CORE3_VAR_DIG2, core3_io_digitals[8 + 2].value, time);
        core3_var_set("Digital3", CORE3_VAR_DIG3, core3_io_digitals[8 + 3].value, time);
    }

    if (ms >= var_stream_last + var_stream_interval)
    {
        var_stream_last = ms;

        if (var_watch_enabled)
        {
            for (size_t i = 0; i < var_count; i++)
            {
                btResponse->ID = btDataID_VAR_WATCH_RESP;
                btResponse->Counter = btDataID_VAR_WATCH_RESP;
                btResponse->Data1 = variables[i].ID;
                btResponse->Data2 = *variables[i].ValPtr;

                memset((void *)btResponse->Data, 0, 32);
                ((float *)&btResponse->Data[0])[0] = variables[i].Time;
                ((float *)&btResponse->Data[0])[1] = *(float *)variables[i].ValPtr;
                memcpy((void *)&btResponse->Data[sizeof(float) + sizeof(float)], variables[i].Name, 8);

                core3_bt_send_data_len((uint8_t *)btResponse, sizeof(btDataStruc));
            }
        }
    }

    if (ms >= can_stream_last + can_stream_interval)
    {
        can_stream_last = ms;

        core3_can_msg msg;
        if (core3_can_rx_dequeue(&msg))
        {
            dprintf("Received CAN message!\n");
        }

        // TODO: Iterate over io_digitals and send all available can frames
    }
}

void core3_program(void *arg)
{
    core3_flash_init();

    dprintf("Cal string: %s\n", (const char *)core3_flash_cal_offset(0x0));

    core3_bt_init();
    core3_can_init(CORE3_CAN_TIMING_33_3KBPS, CORE3_CAN_MODE_NORMAL);
    setup_can_channels();

    dprintf("Done!\n");

    btResponse = (btDataStruc *)malloc(sizeof(btDataStruc));
    memset((void *)btResponse, 0, sizeof(btDataStruc));

    TimerHandle_t core3_tick_timer = xTimerCreate("core3_tick", pdMS_TO_TICKS(20), pdTRUE, NULL, core3_tick);
    xTimerStart(core3_tick_timer, pdMS_TO_TICKS(10));

    while (true)
    {
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

void app_main()
{
    dprintf("Starting app!\n");

    gpio_set_direction(PIN_5V_EN, GPIO_MODE_OUTPUT);
    gpio_set_level(PIN_5V_EN, 1);

    gpio_set_direction(SDCARD_PIN_CS, GPIO_MODE_OUTPUT);
    gpio_set_level(SDCARD_PIN_CS, 1);

    vTaskDelay(pdMS_TO_TICKS(50));

    init_gpio_pins();
    core3_init();

    xTaskCreate(core3_program, "core3_program", 1024 * 60, NULL, CORE3_PROGRAM_PRIORITY, NULL);
}