#pragma once

#include <stdio.h>
#include <stdarg.h>

#ifdef OPSYS_SIM
#define fpurge(input)
#endif

int pdMS_TO_TICKS(int ms);
void vTaskDelay(size_t ticks);
uint32_t esp_random();