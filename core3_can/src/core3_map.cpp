#include <core3.h>
#include <core3_map.h>

size_t core3_map_sizeof(int x_len, int y_len)
{
    size_t x_axis_size = sizeof(core3_map_axis_t);
    x_axis_size += x_len * sizeof(uint16_t);

    size_t y_axis_size = sizeof(core3_map_axis_t);
    y_axis_size += y_len * sizeof(uint16_t);

    size_t map_mem_len = sizeof(uint8_t *) + x_len * y_len;

    return x_axis_size + y_axis_size + map_mem_len;
}

void find_axis_idx(core3_map_axis_t *axis, uint16_t val, uint8_t *out_lower, uint8_t *out_higher, float *out_lerp)
{
    for (size_t i = 1; i < axis->len; i++)
    {
        uint16_t prev = axis->axis[i - 1];
        uint16_t cur = axis->axis[i];

        dprintf("prev %d, cur %d\n", (int)prev, (int)cur);

        if (i == 1 && val < prev)
        {
            *out_lower = 0;
            *out_higher = 0;
            *out_lerp = 0;
            return;
        }

        if (i + 1 >= axis->len && val >= cur)
        {
            *out_lower = axis->len - 1;
            *out_higher = axis->len - 1;
            *out_lerp = 0;
            return;
        }

        if (val >= prev && val < cur)
        {
            *out_lower = i - 1;
            *out_higher = i;

            float max_val = cur - prev;
            float real_val = val - prev;
            *out_lerp = real_val / max_val;

            return;
        }
    }

    *out_lower = 0;
    *out_higher = 0;
    *out_lerp = 0;
    return;
}

uint8_t lerp_u8(uint8_t a, uint8_t b, float f)
{
    return (uint8_t)(a * (1.0f - f) + (b * f));
}

uint16_t lerp_u16(uint16_t a, uint16_t b, float f)
{
    return (uint16_t)(a * (1.0f - f) + (b * f));
}

uint8_t core3_map_idx_raw(core3_map_t *map, int x, int y)
{
    if (x < 0 || x >= map->x.len || y < 0 || y >= map->y.len)
        return 0;

    size_t idx = y * map->x.len + x;
    return map->map_memory[idx];
}

uint8_t core3_map_index(core3_map_t *map, uint16_t X, uint16_t Y, uint8_t *outA, uint8_t *outB, uint8_t *outC, uint8_t *outD)
{
    uint8_t X_Low;
    uint8_t X_High;
    float X_Lerp;
    find_axis_idx(&map->x, X, &X_Low, &X_High, &X_Lerp);

    uint8_t Y_Low;
    uint8_t Y_High;
    float Y_Lerp;
    find_axis_idx(&map->y, Y, &Y_Low, &Y_High, &Y_Lerp);

    dprintf("Indexing XLow %d, XHigh %d, YLow %d, YHigh %d, Xf %f, Yf %f\n",
            (int)X_Low, (int)X_High, (int)Y_Low, (int)Y_High, X_Lerp, Y_Lerp);

    uint8_t A = core3_map_idx_raw(map, X_Low, Y_Low);
    uint8_t B = core3_map_idx_raw(map, X_Low, Y_High);
    uint8_t C = core3_map_idx_raw(map, X_High, Y_High);
    uint8_t D = core3_map_idx_raw(map, X_High, Y_Low);

    if (outA != NULL)
        *outA = A;

    if (outB != NULL)
        *outB = B;

    if (outC != NULL)
        *outC = C;

    if (outD != NULL)
        *outD = D;

    dprintf("A %d, B %d, C %d, D %d\n", (int)A, (int)B, (int)C, (int)D);

    uint8_t HighMid = lerp_u8(B, C, X_Lerp);
    uint8_t LowMid = lerp_u8(A, D, X_Lerp);
    uint8_t Mid = lerp_u8(LowMid, HighMid, Y_Lerp);

    return Mid;
}

size_t core3_map_serialize(core3_map_t *map, void *dest_memory)
{
    size_t idx = (size_t)dest_memory;

    size_t unused1 = sizeof(uint16_t *);

    // X Axis
    {
        *(int *)idx = map->x.len;
        idx += sizeof(int);

        for (size_t i = 0; i < map->x.len; i++)
        {
            ((uint16_t *)idx)[i] = map->x.axis[i];
        }
        idx += map->x.len * sizeof(uint16_t);
    }

    // Y Axis
    {
        *(int *)idx = map->y.len;
        idx += sizeof(int);

        for (size_t i = 0; i < map->y.len; i++)
        {
            ((uint16_t *)idx)[i] = map->y.axis[i];
        }
        idx += map->y.len * sizeof(uint16_t);
    }

    // Memory
    {
        size_t mem_len = map->x.len * map->y.len;

        for (size_t i = 0; i < mem_len; i++)
        {
            ((uint16_t *)idx)[i] = map->map_memory[i];
        }

        idx += mem_len;
    }

    size_t written_bytes = idx - (size_t)dest_memory;

    dprintf("\n");

    for (size_t i = 0; i < 32; i += 8)
    {
        for (size_t j = 0; j < 8; j++)
        {
            dprintf("%02X ", ((uint8_t *)dest_memory)[i + j]);

            if (j == 3)
                dprintf("| ");
        }

        dprintf("\n");
    }

    dprintf("\n");

    return written_bytes;
}

void core3_map_deserialize(void *src_memory, core3_map_t *tgt_map)
{
    size_t idx = (size_t)src_memory;

    int x_len = *(int *)src_memory;
    idx += sizeof(int);
    uint16_t *x_axis = (uint16_t *)idx;
    idx += x_len * sizeof(uint16_t);

    int y_len = *(int *)idx;
    idx += sizeof(int);
    uint16_t *y_axis = (uint16_t *)idx;
    idx += y_len * sizeof(uint16_t);

    idx += sizeof(uint8_t *);
    uint8_t *map_memory = (uint8_t *)idx;

    tgt_map->x.len = x_len;
    tgt_map->x.axis = x_axis;
    tgt_map->y.len = y_len;
    tgt_map->y.axis = y_axis;
    tgt_map->map_memory = map_memory;
}

core3_map_t core3_map_create(uint16_t *x_axis, int x_len, uint16_t *y_axis, int y_len)
{
    core3_map_t map = {0};

    map.x.len = x_len;
    map.x.axis = (uint16_t *)malloc(sizeof(uint16_t) * x_len);
    for (size_t i = 0; i < x_len; i++)
    {
        map.x.axis[i] = x_axis[i];
    }

    map.y.len = y_len;
    map.y.axis = (uint16_t *)malloc(sizeof(uint16_t) * y_len);
    for (size_t i = 0; i < y_len; i++)
    {
        map.y.axis[i] = y_axis[i];
    }

    map.map_memory = (uint8_t *)malloc(x_len * y_len);

    return map;
}