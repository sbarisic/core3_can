#include <core3.h>
#include <ecumaster.h>
#include <core3_map.h>
#include <core3_flash.h>

#include <nvs_flash.h>
#include <esp_log.h>

static DRAM_ATTR uint16_t AxisX_LFTF[] = {0, 10, 20, 30, 40, 50, 60, 70, 80, 90, 100, 110, 120, 130, 140, 150, 160, 170, 180, 190, 200, 210, 220, 230, 240, 250, 260, 270, 280, 290, 300};
static DRAM_ATTR uint16_t AxisY_LFTF[] = {
    750, 1000, 1250, 1500, 1750, 2000, 2250, 2500, 2750,
    3000, 3250, 3500, 3750, 4000, 4250, 4500, 4750,
    5000, 5250, 5500, 5750, 6000, 6250, 6500, 6750, 7000};

core3_map_t *MapLTFT = NULL;
bool emu_available = false;
emu_data_t emu;

volatile uint8_t octane_factor;
volatile uint8_t lftf_value;

size_t core3_round_up(size_t numToRound, size_t multiple)
{
    if (multiple == 0)
    {
        return numToRound;
    }

    size_t roundDown = ((size_t)(numToRound) / multiple) * multiple;
    size_t roundUp = roundDown + multiple;
    size_t roundCalc = roundUp;
    return (roundCalc);
}

bool core3_emu_available()
{
    return emu_available;
}

void core3_ecu_add_ecumaster_frame(emu_data_t emu_data)
{
    emu = emu_data;
    emu_available = true;
}

uint8_t core3_octane_factor_get()
{
    return octane_factor;
}

uint16_t fake_RPM = 1621;

uint16_t core3_ecu_getRPM()
{
    if (!emu_available)
        return fake_RPM;

    uint16_t RPM = emu.RPM;
    return RPM;
}

uint16_t fake_MAP = 82;

uint16_t core3_ecu_getMAP()
{
    if (!emu_available)
        return fake_MAP;

    uint16_t MAP = emu.MAP;
    return MAP;
}

uint8_t core3_long_term_fuel_trim()
{
    return lftf_value;
}

void core3_ecu_tick()
{
    uint16_t RPM = core3_ecu_getRPM();
    uint16_t MAP = core3_ecu_getMAP();

    lftf_value = core3_map_index(MapLTFT, MAP, RPM, NULL, NULL, NULL, NULL);
    // uint16_t MAP = core3_ecu_getMAP();

    // emu.LambdaCorrection;
}

uint8_t correction_to_byte(float cor)
{
    if (cor < 0.75)
        return 0;

    if (cor > 1.25)
        return 255;

    return (uint8_t)((cor - 0.75) / (1.25 - 0.75) * 255);
}

float byte_to_correction(uint8_t byte)
{
    return (75 + ((125 - 75) * (float)(byte / 255.0f))) / 100.0f;
}

bool core3_ecu_ltft_serialize(void (*Callback)(void *User1, uint8_t *mem, size_t size), void *User1)
{
    if (MapLTFT == NULL)
        return false;

    size_t map_size = 960;
    dprintf("mem_size = %u\n", map_size);

    uint8_t *flash_mem = (uint8_t *)malloc(960);

    memset(flash_mem, 0, map_size);

    size_t write_len = core3_map_serialize(MapLTFT, flash_mem);
    dprintf("write_len = %u\n", write_len);

    // core3_flash_cal_erase(0, 0);
    // core3_flash_cal_write(0x100, flash_mem, map_size);
    //  vTaskDelay(pdMS_TO_TICKS(500));

    Callback(User1, flash_mem, map_size);
    free(flash_mem);
    return true;
}

void core3_ecu_update_task(void *arg)
{
    while (true)
    {
        core3_ecu_tick();
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

void core3_ecu_cal_writeToFlash(void* User1, uint8_t *mem, size_t size)
{
    core3_flash_cal_erase(0, 0);
    core3_flash_cal_write(0x100, mem, size);
}

void core3_ecu_init()
{
    MapLTFT = core3_map_create(AxisX_LFTF, sizeof(AxisX_LFTF) / sizeof(*AxisX_LFTF), AxisY_LFTF, sizeof(AxisY_LFTF) / sizeof(*AxisY_LFTF));

    for (size_t y = 0; y < MapLTFT->y.len; y++)
    {
        for (size_t x = 0; x < MapLTFT->x.len; x++)
        {
            core3_map_set_raw(MapLTFT, x, y, correction_to_byte(1.0f));
        }
    }

    //core3_ecu_ltft_serialize(core3_ecu_cal_writeToFlash, NULL);

    const void *MapLTFT_Flash = core3_flash_cal_offset(0x100);
    core3_map_deserialize(MapLTFT_Flash, &MapLTFT);

    xTaskCreate(core3_ecu_update_task, "core3_ecu_update_task", 1024 * 15, NULL, CORE3_ECU_UPDATE_PRIORITY, NULL);
    /**dprintf("Indexing map\n");
    uint8_t map_val = core3_map_index(&MapLTFT, 5, 878, NULL, NULL, NULL, NULL);
    dprintf("MAP_VAL = 0x%02X, %d, %f\n", map_val, (int)map_val, byte_to_correction(map_val));

    dprintf("LTFT X = %d, Y = %d\n", MapLTFT.x.len, MapLTFT.y.len);*/
}

void core3_init()
{
    esp_log_level_set("*", ESP_LOG_NONE);

    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        dprintf("Doing nvs_flash_erase()\n");
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }

    // core3_flash_init();
}
