#pragma once

#include "esp_system.h"

#if defined(__cplusplus)
extern "C"
{
#endif

    esp_err_t core3_wifi_init();
    bool core3_wifi_delay_until_connected();

#if defined(__cplusplus)
}
#endif