#include <core3.h>
#include <core3_can.h>
#include <ecumaster.h>
#include <core3_gmlan.h>

#include "driver/gpio.h"
#include "driver/twai.h"

#include "spi_flash_mmap.h"
#include "esp_partition.h"

static size_t mem_size = 0x10000;
static esp_partition_mmap_handle_t part_mmap_handle;
static const esp_partition_t *cal_part;
static const void *cal_memory;

const void *core3_flash_cal_offset(size_t offset)
{
    if (offset == 0)
        return cal_memory;

    return (const void *)(((size_t)cal_memory) + offset);
}

bool core3_flash_cal_erase()
{
    esp_err_t err = esp_partition_erase_range(cal_part, 0x0, mem_size);

    if (err != ESP_OK)
    {
        dprintf("esp_partition_erase_range() failed\n");
        return false;
    }

    return true;
}

bool core3_flash_cal_write(size_t offset, const void *src, size_t size)
{
    esp_err_t err = esp_partition_write(cal_part, 0x0, src, size);

    if (err != ESP_OK)
    {
        dprintf("core3_flash_cal_write() failed\n");
        return false;
    }

    return true;
}

bool core3_flash_map()
{
    esp_err_t err = esp_partition_mmap(cal_part, 0x0, mem_size, ESP_PARTITION_MMAP_DATA, &cal_memory, &part_mmap_handle);

    if (err != ESP_OK)
    {
        dprintf("esp_partition_mmap() failed\n");
        return false;
    }

    return true;
}

void core3_flash_unmap()
{
    esp_partition_munmap(part_mmap_handle);
}

bool core3_flash_init()
{
    cal_part = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_UNDEFINED, "cal");
    if (!core3_flash_map())
        return false;

    dprintf("core3_flash_init() done\n");
    return true;
}