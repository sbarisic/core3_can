#pragma once

#include "esp_system.h"

#if defined(__cplusplus)
extern "C"
{
#endif

    esp_err_t core3_bt_init();

    bool core3_bt_send_data_len(uint8_t *dat, int len);
    //void core3_bt_send_data(const char *dat);
    bool core3_bt_is_connected();

#if defined(__cplusplus)
}
#endif