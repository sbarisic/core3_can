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

#if defined(__cplusplus)
extern "C"
{
#endif

    void app_main();
    void core3_init();

#if defined(__cplusplus)
}
#endif