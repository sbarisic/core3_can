#include <core3.h>
#include <ecumaster.h>
#include <core3_map.h>
#include <core3_flash.h>
#include <core3_bt.h>

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
volatile bool use_dbw;
volatile uint8_t dbw_target;

uint16_t fake_RPM_base = 1621;
uint16_t fake_RPM = 0;
uint16_t fake_MAP_base = 85;
uint16_t fake_MAP = 0;
uint16_t fake_CLT = 95;
uint8_t fake_TPS = 0;
uint8_t fake_IAT = 25;
float fake_wboLambda = 1.0f;
float fake_lambdaTarget = 1.0f;
float fake_LambdaCorr = 1.0f;

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

uint8_t core3_ecu_octane_factor()
{
    return octane_factor;
}

uint8_t core3_ecu_long_term_fuel_trim()
{
    return lftf_value;
}

uint8_t core3_ecu_dbw_target()
{
    if (use_dbw)
        return dbw_target;

    return 0;
}

bool core3_ecu_errors(bool *errCLT, bool *errIAT, bool *errMAP, bool *errWBO, bool *Knock)
{
    if (emu_available)
    {
        return false;
    }

    bool hasError = false;

    if (errCLT != NULL)
    {
        *errCLT = (emu.cel & ERR_CLT) > 0;
        if (*errCLT)
            hasError = true;
    }

    if (errIAT != NULL)
    {
        *errIAT = (emu.cel & ERR_IAT) > 0;
        if (*errIAT)
            hasError = true;
    }

    if (errMAP != NULL)
    {
        *errMAP = (emu.cel & ERR_MAP) > 0;
        if (*errMAP)
            hasError = true;
    }

    if (errWBO != NULL)
    {
        *errWBO = (emu.cel & ERR_WBO) > 0;
        if (*errWBO)
            hasError = true;
    }

    if (Knock != NULL)
    {
        *Knock = (emu.cel & KNOCKING) > 0;
        if (*Knock)
            hasError = true;
    }

    return hasError;
}

void core3_ecu_data2()
{
    if (emu_available)
    {
        return;
    }
}

uint16_t calcFakeRPM()
{
    float range = 1000;
    return (uint16_t)(fake_RPM_base + (range / 2) + (core3_clock_sine(1.0f, range)));
}

uint16_t calcFakeMAP()
{
    float range = 40;
    return (uint16_t)(fake_MAP_base + (range / 2) + (core3_clock_sine(0.7f, range)));
}

void core3_ecu_data1(uint16_t *RPM, uint16_t *MAP, uint16_t *CLT, uint8_t *TPS, uint8_t *IAT,
                     float *WBOLam, float *LamTgt, float *LamCor)
{
    if (RPM != NULL)
        *RPM = emu_available ? emu.RPM : fake_RPM;

    if (MAP != NULL)
        *MAP = emu_available ? emu.MAP : fake_MAP;

    if (CLT != NULL)
        *CLT = emu_available ? emu.CLT : fake_CLT;

    if (TPS != NULL)
        *TPS = emu_available ? emu.TPS : fake_TPS;

    if (IAT != NULL)
        *IAT = emu_available ? emu.IAT : fake_IAT;

    if (WBOLam != NULL)
        *WBOLam = emu_available ? emu.wboLambda : fake_wboLambda;

    if (LamTgt != NULL)
        *LamTgt = emu_available ? emu.lambdaTarget : fake_lambdaTarget;

    if (LamCor != NULL)
        *LamCor = emu_available ? emu.LambdaCorrection : fake_LambdaCorr;
}

void core3_ecu_tick()
{
    fake_RPM = calcFakeRPM();
    fake_MAP = calcFakeMAP();

    uint16_t RPM = 0;
    uint16_t MAP = 0;
    core3_ecu_data1(&RPM, &MAP, NULL, NULL, NULL, NULL, NULL, NULL);

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

// int bt_stream_counter = 0;

void core3_ecu_update_task(void *arg)
{
    while (true)
    {
        core3_ecu_tick();

        /*bt_stream_counter++;
        if (bt_stream_counter > 3)
        {
            bt_stream_counter = 0;
        }*/

        vTaskDelay(pdMS_TO_TICKS(12));
    }
}

void core3_ecu_cal_writeToFlash(void *User1, uint8_t *mem, size_t size)
{
    core3_flash_cal_erase(0, 0);
    core3_flash_cal_write(0x100, mem, size);
}

void core3_ecu_init()
{
    dbw_target = 0;
    use_dbw = false;

    MapLTFT = core3_map_create(AxisX_LFTF, sizeof(AxisX_LFTF) / sizeof(*AxisX_LFTF), AxisY_LFTF, sizeof(AxisY_LFTF) / sizeof(*AxisY_LFTF));

    for (size_t y = 0; y < MapLTFT->y.len; y++)
    {
        for (size_t x = 0; x < MapLTFT->x.len; x++)
        {
            core3_map_set_raw(MapLTFT, x, y, correction_to_byte(1.0f));
        }
    }

    // core3_ecu_ltft_serialize(core3_ecu_cal_writeToFlash, NULL);

    const void *MapLTFT_Flash = core3_flash_cal_offset(0x100);
    core3_map_deserialize(MapLTFT_Flash, &MapLTFT);

    xTaskCreate(core3_ecu_update_task, "c3_ecu_update", 1024 * 15, NULL, CORE3_ECU_UPDATE_PRIORITY, NULL);
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
