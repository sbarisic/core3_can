#pragma once

#include "esp_system.h"

#if defined(__cplusplus)
extern "C"
{
#endif

    typedef enum
    {
        btDataID_CAL_READ = 0x1,      // uint32_T Data1 - Offset, uint32_t Data2 - Length
        btDataID_CAL_READ_RESP = 0x2, // 32 bytes of data

        btDataID_CAL_WRITE,
        btDataID_CAL_WRITE_RESP,

        btDataID_CAL_ERASE,
        btDataID_CAL_ERASE_RESP,

        btDataID_RAM_READ,
        btDataID_RAM_READ_RESP,

        btDataID_VAR_WATCH,
        btDataID_VAR_WATCH_RESP,

        btDataID_VAR_RBOOT,
        btDataID_VAR_RBOOT_RESP,
    } btDataID;

    typedef struct PACKED_ATTR
    {
        uint8_t ID;
        uint8_t Counter;
        uint32_t Data1;
        uint32_t Data2;
        uint8_t Data[32];
    } btDataStruc;

    esp_err_t core3_bt_init();

    bool core3_bt_send_data_len(uint8_t *dat, int len);
    // void core3_bt_send_data(const char *dat);
    bool core3_bt_is_connected();

#if defined(__cplusplus)
}
#endif