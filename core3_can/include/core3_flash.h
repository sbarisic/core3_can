#pragma once

#include <stdlib.h>

#if defined(__cplusplus)
extern "C"
{
#endif

    const void *core3_flash_cal_offset(size_t offset);
    bool core3_flash_init();

#if defined(__cplusplus)
}
#endif