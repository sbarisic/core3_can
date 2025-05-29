#pragma once

#ifndef OPSYS_SIM
#include "esp_attr.h"
#endif
#include <inttypes.h>
#include <stdint.h>
#include <stddef.h>

#ifdef OPSYS_SIM
#define PACKED_ATTR
#endif

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
#ifdef OPSYS_SIM
#pragma pack(push, 1)
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
        uint8_t element_size;
        uint8_t *map_memory;
    } PACKED_ATTR core3_map_t;

#ifdef OPSYS_SIM
#pragma pack(pop)
#endif

    core3_map_t *core3_map_create(uint8_t element_size, uint16_t *x_axis, int x_len, uint16_t *y_axis, int y_len);
    size_t core3_map_sizeof(int x_len, int y_len, uint8_t element_size);

    void core3_map_set_raw(core3_map_t *map, int x, int y, uint8_t val);
    void core3_map_set_index(core3_map_t *map, uint16_t X, uint16_t Y, uint8_t val);

    void core3_map_set_element(core3_map_t *map, uint16_t X, uint16_t Y, uint32_t val);
    uint32_t core3_map_get_element(core3_map_t *map, uint16_t X, uint16_t Y);

    uint8_t core3_map_idx_raw(core3_map_t *map, int x, int y);

    uint8_t core3_map_index(core3_map_t *map, uint16_t X, uint16_t Y,
                            uint8_t *outA, uint8_t *outB, uint8_t *outC, uint8_t *outD);

    size_t core3_map_serialize(core3_map_t *map, void *dest_memory);
    size_t core3_map_serialize_all(void *dst_memory, core3_map_t **tgt_maps, size_t map_count);
    bool core3_map_deserialize(const void *src_memory, core3_map_t **tgt_map_ptr, size_t *read_bytes);
    bool core3_map_deserialize_all(const void *src_memory, core3_map_t **tgt_maps, size_t map_len, size_t *map_count);

    void core3_ecu_init(bool isReInit);
    void core3_ecu_mark_dirty();
    void core3_ecu_mark_clear_time();

#if defined(__cplusplus)
}
#endif
