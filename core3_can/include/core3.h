#pragma once

#include <malloc.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>

#include "driver/sdmmc_host.h"

#define dprintf printf

#define PIN_5V_EN GPIO_NUM_16

#define CAN_RX_PIN GPIO_NUM_26
#define CAN_TX_PIN GPIO_NUM_27
#define CAN_SE_PIN GPIO_NUM_23

#define RS485_EN_PIN GPIO_NUM_17 // 17 /RE
#define RS485_TX_PIN GPIO_NUM_22 // 21
#define RS485_RX_PIN GPIO_NUM_21 // 22
#define RS485_SE_PIN GPIO_NUM_19 // 22 /SHDN

#define SDCARD_PIN_MISO GPIO_NUM_2
#define SDCARD_PIN_MOSI GPIO_NUM_15
#define SDCARD_PIN_CLK GPIO_NUM_14
#define SDCARD_PIN_CS GPIO_NUM_13

#define WS2812_PIN 4

#define GPIO0 GPIO_NUM_18
#define GPIO1 GPIO_NUM_34 // ADC1_CH6
#define GPIO2 GPIO_NUM_5
#define GPIO3 GPIO_NUM_32 // ADC1_CH4
#define GPIO4 GPIO_NUM_35 // ADC1_CH7
#define GPIO5 GPIO_NUM_12 // ADC2_CH5
#define GPIO6 GPIO_NUM_33 // ADC1_CH5
#define GPIO7 GPIO_NUM_25 // ADC2_CH8

#define GPIOA0 GPIO1
#define GPIOA1 GPIO3
#define GPIOA2 GPIO4
#define GPIOA3 GPIO6
#define GPIOA4 GPIO7
#define GPIOA5 GPIO5

#define GPIOA0_CH ADC_CHANNEL_6
#define GPIOA1_CH ADC_CHANNEL_4
#define GPIOA2_CH ADC_CHANNEL_7
#define GPIOA3_CH ADC_CHANNEL_5
#define GPIOA4_CH ADC_CHANNEL_8
#define GPIOA5_CH ADC_CHANNEL_5

// Priorities
#define CORE3_PROGRAM_PRIORITY 3
#define CORE3_CAN_SEND_PRIORITY 4
#define CORE3_CAN_RECEIVE_PRIORITY 5

#if defined(__cplusplus)
extern "C"
{
#endif
    typedef enum
    {
        VARTYPE_FLOAT,
        VARTYPE_UINT32
    } varType_t;

    typedef enum
    {
        VAR_ANALOG0 = 1,
        VAR_ANALOG1 = 2,
        VAR_ANALOG2 = 3,
        VAR_ANALOG3 = 4,
        VAR_DIG0 = 8,
        VAR_DIG1 = 9,
        VAR_DIG2 = 10,
        VAR_DIG3 = 11
    } coreVarName_t;

    typedef struct PACKED_ATTR
    {
        uint8_t value;
        float raw_value;

        float trigger_value;
        float hyst;
    } core3_io_digital;

    void app_main();
    void core3_init();
    size_t core3_round_up(size_t numToRound, size_t multiple);

    uint32_t core3_time_ms();

    bool core3_var_watch_is_enabled();
    void core3_var_watch_set(bool enabled);

    varType_t core3_var_get(coreVarName_t var, float *out_varf, uint32_t *out_varu, float *out_time);
    bool core3_var_set(const char *name, coreVarName_t var, varType_t varType, float valf, uint32_t valu, float time);

    // core3.cpp

    void core3_ecu_tick();
    void core3_ecu_ltft_tick();
    bool core3_emu_available();
    uint8_t core3_octane_factor_get();
    uint8_t core3_long_term_fuel_trim();

#if defined(__cplusplus)
}
#endif