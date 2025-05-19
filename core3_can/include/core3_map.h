#pragma once

#include "esp_attr.h"
#include <inttypes.h>
#include <stdint.h>
#include <stddef.h>

#if defined(_WIN32) || defined(_WIN64)
#define _CRT_SECURE_NO_WARNINGS
#define WIN32_LEAN_AND_MEAN
#define CORE2_WINDOWS
#endif

#define EMU_BASE 0x600

#if defined(__cplusplus)
extern "C"
{
#endif
    typedef struct
    {
        int len;
        uint16_t *axis;
    } PACKED_ATTR core3_map_axis_t;

    typedef struct
    {
        core3_map_axis_t x;
        core3_map_axis_t y;
        uint8_t *map_memory;
    } PACKED_ATTR core3_map_t;

    core3_map_t core3_map_create(uint16_t *x_axis, int x_len, uint16_t *y_axis, int y_len);
    uint8_t *core3_map_idx_raw(core3_map_t *map, size_t x, size_t y);
    uint8_t core3_map_index(core3_map_t *map, uint16_t X, uint16_t Y,
                            uint8_t **outA, uint8_t **outB, uint8_t **outC, uint8_t **outD);
    void core3_ecu_init();

#if defined(__cplusplus)
}
#endif
