#pragma once

#include <stdlib.h>

#if defined(__cplusplus)
extern "C"
{
#endif

    bool core3_flash_cal_erase(size_t offset, size_t size);
    bool core3_flash_cal_write(size_t offset, const void *src, size_t size);
    const void *core3_flash_cal_offset(size_t offset);
    bool core3_flash_init();

#if defined(__cplusplus)
}
#endif