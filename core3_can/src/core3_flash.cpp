#include <core3.h>
#include <core3_flash.h>

#include "esp_partition.h"
#include "spi_flash_mmap.h"

static size_t mem_size = 0x10000;
static esp_partition_mmap_handle_t part_mmap_handle;
static const esp_partition_t *cal_part;
static const void *cal_memory;

const void * IRAM_ATTR core3_flash_cal_offset(size_t offset)
{
    if (offset == 0)
        return cal_memory;

    return (const void *)((size_t)cal_memory + offset);
}

bool core3_flash_cal_erase(size_t offset, size_t size)
{
    if (offset == 0x0 && size == 0x0)
        size = CONFIG_WL_SECTOR_SIZE;
    else
        size = core3_round_up(size, CONFIG_WL_SECTOR_SIZE);

    dprintf("core3_flash_cal_erase(%u, %u)\n", offset, size);

    esp_err_t err = esp_partition_erase_range(cal_part, offset, size);

    if (err != ESP_OK)
    {
        dprintf("esp_partition_erase_range() failed\n");
        return false;
    }

    return true;
}

bool core3_flash_cal_write(size_t offset, const void *src, size_t size)
{
    dprintf("core3_flash_cal_write(%u, %u)\n", offset, size);

    esp_err_t err = esp_partition_write_raw(cal_part, offset, src, size);

    if (err != ESP_OK)
    {
        dprintf("core3_flash_cal_write() failed\n");
        return false;
    }

    return true;
}

bool core3_flash_map()
{
    esp_err_t err =
        esp_partition_mmap(cal_part, 0x0, mem_size, ESP_PARTITION_MMAP_DATA, &cal_memory, &part_mmap_handle);

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
    {
        dprintf("core3_flash_init - core3_flash_map() failed\n");
        return false;
    }

    bool is_success = true;

    const uint8_t *write_check = (const uint8_t *)core3_flash_cal_offset(0x0);
    if (*write_check == 0xFF)
    {
        size_t write_data_len = 64;
        uint8_t write_data[write_data_len];

        for (size_t i = 0; i < write_data_len; i++)
        {
            write_data[i] = 0x0;
        }

        write_data[0] = 0x0;
        sprintf((char *)write_data, "Hello World!");

        // if (core3_flash_cal_erase(0x0, write_data_len))
        //{
        if (!core3_flash_cal_write(0x0, write_data, write_data_len))
            is_success = false;
        //}
        // else
        //    is_success = false;
    }
    else
    {
        dprintf("core3_flash_init() ... magic OK\n");
    }

    // core3_flash_cal_erase(0, 0);
    return is_success;
}