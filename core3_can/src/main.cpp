#include <core3.h>
#include <core3_can.h>
#include <core3_flash.h>
#include <core3_gmlan.h>
#include <core3_gpio.h>
#include <core3_map.h>
#include <ecumaster.h>
#include <esp_timer.h>

#include <core3_bt.h>
#include <core3_wifi.h>

#include "stdio.h"
#include "stdlib.h"
#include "math.h"

#include <esp_adc/adc_oneshot.h>

#define LED_PIN WS2812_PIN // digital pin used to drive the LED strip
#define LED_COUNT 1        // number of LEDs on the strip
#define RGB(R, G, B) ((R << 16) | (G << 8) | B)

typedef struct
{
    core3_can_msg frame;
    int64_t next_send;
    int16_t send_interval;

    uint8_t counter_offset_byte;
    uint8_t counter_offset_bit;
    uint8_t counter_bit_width;
} can_message;

typedef struct
{
    coreVarName_t ID;
    varType_t VarType;

    union
    {
        float *Float;
        uint32_t *Uint32;
    } ValPtr;

    float Time;
    char Name[8];
} watcher_var;

// ====================================== Variables ======================================

TimerHandle_t core3_tick_timer;

size_t var_count = 0;
watcher_var variables[32];

int64_t emu_tstp[8];
// static emu_data_t emu_data;
// static vehicle_data veh_data;

can_message tx_frames[16];
int tx_frames_count = 0;

SemaphoreHandle_t varSemaphore = NULL;

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

float core3_clock_sine(float phase, float divi)
{
    float bootms = (float)(esp_timer_get_time() / 1000);
    return sinf(bootms / 1000.0f * phase) * divi;
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

    adc_cali_line_fitting_config_t cali_config = {
        .unit_id = ADC_UNIT_1,
        .atten = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
        .default_vref = 0};
    ESP_ERROR_CHECK(adc_cali_create_scheme_line_fitting(&cali_config, &adc1_cali_handle));
}

bool core3_var_set(const char *name, coreVarName_t var, varType_t varType, float valf, uint32_t valu, float time)
{
    if (xSemaphoreTake(varSemaphore, portMAX_DELAY) == pdTRUE)
    {
        for (size_t i = 0; i < var_count; i++)
        {
            if (variables[i].ID == var)
            {
                variables[i].VarType = varType;

                if (varType == VARTYPE_FLOAT)
                {
                    *variables[i].ValPtr.Float = valf;
                }
                else if (varType == VARTYPE_UINT32)
                {
                    *variables[i].ValPtr.Uint32 = valu;
                }
                else
                {
                    dprintf("[ERROR] core3_var_set varType\n");
                }

                variables[i].Time = time;
                memcpy(variables[i].Name, name, sizeof(variables[i].Name));

                xSemaphoreGive(varSemaphore);
                return true;
            }
        }

        size_t newidx = var_count++;
        variables[newidx].ID = var;
        variables[newidx].VarType = VARTYPE_FLOAT;
        variables[newidx].ValPtr.Float = (float *)malloc(sizeof(float));
        variables[newidx].Time = time;
        memcpy(variables[newidx].Name, name, sizeof(variables[newidx].Name));

        xSemaphoreGive(varSemaphore);
        return true;
    }

    return false;
}

varType_t core3_var_get(coreVarName_t var, float *out_varf, uint32_t *out_varu, float *out_time)
{
    if (xSemaphoreTake(varSemaphore, portMAX_DELAY) == pdTRUE)
    {
        for (size_t i = 0; i < var_count; i++)
        {
            if (variables[i].ID == var)
            {
                if (out_time != NULL)
                    *out_time = variables[i].Time;

                varType_t varType = variables[i].VarType;

                if (varType == VARTYPE_FLOAT)
                {
                    if (out_varf != NULL)
                        *out_varf = *variables[i].ValPtr.Float;

                    xSemaphoreGive(varSemaphore);
                    return varType;
                }
                else if (varType == VARTYPE_UINT32)
                {
                    if (out_varu != NULL)
                        *out_varu = *variables[i].ValPtr.Uint32;

                    xSemaphoreGive(varSemaphore);
                    return varType;
                }
                else
                {
                    dprintf("[ERROR] core3_var_get varType\n");
                }

                break;
            }
        }

        if (out_time != NULL)
            *out_time = 0.0f;

        if (out_varf != NULL)
            *out_varf = 0;

        xSemaphoreGive(varSemaphore);
        return VARTYPE_FLOAT;
    }

    if (out_time != NULL)
        *out_time = 0.0f;

    if (out_varf != NULL)
        *out_varf = 0;

    return VARTYPE_FLOAT;
}

btDataStruc btResponse;

static uint64_t ms = 0;
static bool var_watch_enabled = false;

bool core3_var_watch_is_enabled()
{
    return var_watch_enabled;
}

void core3_var_watch_set(bool enabled)
{
    var_watch_enabled = enabled;

    dprintf("[ECU] Realtime Data %s\n", var_watch_enabled ? "ENABLED" : "DISABLED");
}

uint32_t core3_time_ms()
{
    return (uint32_t)(esp_timer_get_time() / 1000);
}

IRAM_ATTR void core3_tick(TimerHandle_t timer)
{
}

void variables_stream_task(void *arg)
{
    while (true)
    {
        if (var_watch_enabled)
        {
            for (size_t i = 0; i < var_count; i++)
            {
                btResponse.ID = btDataID_VAR_WATCH_RESP;
                btResponse.Counter = btDataID_VAR_WATCH_RESP;
                btResponse.Data1 = variables[i].ID;
                btResponse.Data2 = *variables[i].ValPtr.Uint32;

                memset((void *)btResponse.Data, 0, 32);
                ((float *)&btResponse.Data[0])[0] = variables[i].Time;

                if (variables[i].ValPtr.Float != NULL)
                    ((float *)&btResponse.Data[0])[1] = *variables[i].ValPtr.Float;

                memcpy((void *)&btResponse.Data[sizeof(float) + sizeof(float)], variables[i].Name, 8);

                core3_bt_send_data_len((uint8_t *)&btResponse, sizeof(btDataStruc), false);
            }
        }

        vTaskDelay(pdMS_TO_TICKS(40));
    }
}

void update_variables_task(void *arg)
{
    float v0, v1, v2, v3;
    uint16_t RPM = 0;
    uint16_t MAP = 0;
    bool errCLT = false;
    bool errIAT = false;
    bool errMAP = false;
    bool errWBO = true;
    bool Knock = false;

    while (true)
    {
        core3_ecu_data1(&RPM, &MAP, NULL, NULL, NULL, NULL, NULL, NULL);
        core3_ecu_errors(&errCLT, &errIAT, &errMAP, &errWBO, &Knock);

        float time = ms / 1000.0f;
        core3_analog(GPIOA0_CH, &v0);
        core3_analog(GPIOA1_CH, &v1);
        core3_analog(GPIOA2_CH, &v2);
        core3_analog(GPIOA3_CH, &v3);

        core3_var_set("Analog0 ", VAR_ANALOG0, VARTYPE_FLOAT, v0, 0, time);
        core3_var_set("Analog1 ", VAR_ANALOG1, VARTYPE_FLOAT, v1, 0, time);
        core3_var_set("Analog2 ", VAR_ANALOG2, VARTYPE_FLOAT, v2, 0, time);
        core3_var_set("Analog3 ", VAR_ANALOG3, VARTYPE_FLOAT, v3, 0, time);

        core3_var_set("RPM     ", VAR_RPM, VARTYPE_FLOAT, (float)RPM, 0, time);
        core3_var_set("MAP     ", VAR_MAP, VARTYPE_FLOAT, (float)MAP, 0, time);
        core3_var_set("LTFT    ", VAR_LTFT, VARTYPE_FLOAT, byte_to_correction(core3_ecu_long_term_fuel_trim()), 0, time);
        core3_var_set("OCT.FAC ", VAR_OCTANE_FACTOR, VARTYPE_FLOAT, core3_ecu_octane_factor() / 255.0f, 0, time);

        core3_var_set("ERR.CLT ", VAR_ERR_CLT, VARTYPE_FLOAT, (errCLT ? 1.0f : 0.0f), 0, time);
        core3_var_set("ERR.IAT ", VAR_ERR_IAT, VARTYPE_FLOAT, (errIAT ? 1.0f : 0.0f), 0, time);
        core3_var_set("ERR.MAP ", VAR_ERR_MAP, VARTYPE_FLOAT, (errMAP ? 1.0f : 0.0f), 0, time);
        core3_var_set("ERR.WBO ", VAR_ERR_WBO, VARTYPE_FLOAT, (errWBO ? 1.0f : 0.0f), 0, time);
        core3_var_set("ERR.KNCK", VAR_KNOCK, VARTYPE_FLOAT, (Knock ? 1.0f : 0.0f), 0, time);

        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

void core3_program(void *arg)
{
    gpio_set_direction(PIN_5V_EN, GPIO_MODE_OUTPUT);
    gpio_set_level(PIN_5V_EN, 1);

    gpio_set_direction(SDCARD_PIN_CS, GPIO_MODE_OUTPUT);
    gpio_set_level(SDCARD_PIN_CS, 1);

    vTaskDelay(pdMS_TO_TICKS(50));

    init_gpio_pins();
    // dprintf("Cal string: %s\n", (const char *)core3_flash_cal_offset(0x0));

    core3_bt_init();

    core3_can_init(CORE3_CAN_TIMING_33_3KBPS, CORE3_CAN_MODE_NORMAL);
    setup_can_channels();

    while (!core3_bt_is_advertising())
    {
        vTaskDelay(pdMS_TO_TICKS(10));
    }

    core3_ecu_init();
    dprintf("Done!\n");

    // btResponse = (btDataStruc *)malloc(sizeof(btDataStruc));
    memset((void *)&btResponse, 0, sizeof(btDataStruc));

    core3_tick_timer = xTimerCreate("core3_tick", pdMS_TO_TICKS(10), pdTRUE, NULL, core3_tick);
    xTimerStart(core3_tick_timer, pdMS_TO_TICKS(500));

    xTaskCreate(update_variables_task, "update_var", 1024 * 20, NULL, CORE3_VAR_UPDATE_PRIORITY, NULL);
    xTaskCreate(variables_stream_task, "var_stream", 1024 * 20, NULL, CORE3_VAR_STREAM_PRIORITY, NULL);
}

void app_main()
{
    dprintf("Starting app!\n");

    core3_init();
    core3_flash_init();

    vSemaphoreCreateBinary(varSemaphore);
    core3_program(NULL);

    while (true)
    {
        // vTaskList((char *) pcWriteBuffer);
        // vTaskGetRunTimeStats((char *)pcWriteBuffer);
        // printf("Run Times:\n%s\n", pcWriteBuffer);

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}