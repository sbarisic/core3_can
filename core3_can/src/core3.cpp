#include <core3.h>
#include <ecumaster.h>
#include <core3_map.h>
#include <core3_flash.h>
#include <core3_bt.h>

#include <nvs_flash.h>
#include <esp_log.h>
#include "math.h"

static DRAM_ATTR uint16_t AxisX_LFTF[] = {0, 10, 20, 30, 40, 50, 60, 70, 80, 90, 100, 110, 120, 130, 140, 150, 160, 170, 180, 190, 200, 210, 220, 230, 240, 250, 260, 270, 280, 290, 300};
static DRAM_ATTR uint16_t AxisY_LFTF[] = {
    750, 1000, 1250, 1500, 1750, 2000, 2250, 2500, 2750,
    3000, 3250, 3500, 3750, 4000, 4250, 4500, 4750,
    5000, 5250, 5500, 5750, 6000, 6250, 6500, 6750, 7000};

static core3_map_t *MapLTFT = NULL;
static bool emu_available = false;
static emu_data_t emu;

uint8_t lam_cor_log[32];

static volatile uint8_t octane_factor;
static volatile uint8_t ltft_value;
static volatile bool use_dbw;
static volatile uint8_t dbw_target;

static bool learning_enabled = true;

static uint16_t fake_RPM_base = 3000;
static uint16_t fake_RPM = 0;
static uint16_t fake_MAP_base = 100;
static uint16_t fake_MAP = 0;
static uint16_t fake_CLT = 95;
static uint8_t fake_TPS = 0;
static uint8_t fake_IAT = 25;
static float fake_wboLambda = 1.0f;
static float fake_lambdaTarget = 1.0f;

void core3_ecu_lam_cor_push(uint8_t wbo)
{
    for (size_t i = 1; i < sizeof(lam_cor_log); i++)
    {
        lam_cor_log[i - 1] = lam_cor_log[i];
    }

    lam_cor_log[sizeof(lam_cor_log) - 1] = wbo;
}

uint8_t core3_ecu_lam_cor_avg(int ticks)
{
    size_t sum = 0;

    for (size_t i = 0; i < ticks; i++)
    {
        size_t idx = sizeof(lam_cor_log) - i - 1;
        sum = sum + lam_cor_log[idx];
    }

    return (uint8_t)(sum / ticks);
}

void core3_ecu_lam_cor_minmax(int ticks, float *lmin, float *lmax, float *lrange)
{
    uint8_t vmin = lam_cor_log[sizeof(lam_cor_log) - 1];
    uint8_t vmax = lam_cor_log[sizeof(lam_cor_log) - 1];

    for (size_t i = 0; i < ticks; i++)
    {
        size_t idx = sizeof(lam_cor_log) - i - 1;
        uint8_t val = lam_cor_log[idx];

        vmin = val < vmin ? val : vmin;
        vmax = val > vmax ? val : vmax;
    }

    if (lmin != NULL)
        *lmin = byte_to_correction(vmin);

    if (lmax != NULL)
        *lmax = byte_to_correction(vmax);

    if (lrange != NULL)
        *lrange = byte_to_correction(vmax) - byte_to_correction(vmin);
}

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
    return ltft_value;
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
    float range = 2500;
    return (uint16_t)(fake_RPM_base + (range / 2) + (core3_clock_sine(0.313f * 0.5f, range)));
}

uint16_t calcFakeMAP()
{
    float range = 130;
    return (uint16_t)(fake_MAP_base + (range / 2) + (core3_clock_sine(0.241f * 0.5f, range)));
}

float core3_ecu_lambda_cor()
{
    if (emu_available)
        return emu.LambdaCorrection;

    return 1 + (sinf(fake_MAP / 40.0f) * 0.25f);
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
        *LamCor = byte_to_correction(ltft_value) * core3_ecu_lambda_cor();
}

uint8_t stft_value;

float core3_ecu_stft(uint16_t RPM, uint16_t MAP)
{
    // float lam_cor = byte_to_correction(core3_ecu_lam_cor_avg(10));
    float cur_ltft = byte_to_correction(ltft_value);
    float cur_stft = cur_ltft * core3_ecu_lambda_cor();
    return cur_stft;
}

void core3_ecu_tick()
{
    fake_RPM = calcFakeRPM();
    fake_MAP = calcFakeMAP();

    uint16_t RPM = 0;
    uint16_t MAP = 0;
    float LamCor = 0;
    core3_ecu_data1(&RPM, &MAP, NULL, NULL, NULL, NULL, NULL, &LamCor);

    // core3_ecu_lam_cor_push(LamCor);

    ltft_value = core3_map_index(MapLTFT, MAP, RPM, NULL, NULL, NULL, NULL);
    float stft_value = core3_ecu_stft(RPM, MAP);
    float stft_inv = 2 - stft_value;
    core3_ecu_lam_cor_push(correction_to_byte(stft_inv));

    if (learning_enabled)
    {
        float last_range = 0;
        int ticks_to_be_stable = 25;
        float lambda_range_hyst = 0.10 / 14.7f;
        float lambda_stable_hyst = 0.05 / 14.7f;

        core3_ecu_lam_cor_minmax(ticks_to_be_stable, NULL, NULL, &last_range);

        if (stft_value + (lambda_stable_hyst / 2) > 1 && stft_value - (lambda_stable_hyst / 2) < 1)
        {
        }
        else if (last_range < lambda_range_hyst)
        {
            float ltft_t = byte_to_correction(ltft_value);
            float ltft_new = lerp(ltft_t, ltft_t * stft_inv, 0.5f);
            ltft_value = correction_to_byte(ltft_new);

            // dprintf("LEARN %f -> %f\n", ltft_t, ltft_new);
            core3_map_set_index(MapLTFT, MAP, RPM, ltft_value);
        }
    }

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

void core3_ecu_cal_writeToFlash(void *User1, uint8_t *mem, size_t size)
{
    core3_flash_cal_erase(0, 0);
    core3_flash_cal_write((size_t)User1, mem, size);
}

void core3_ecu_cal_write_all()
{
    uint8_t temp_buf[1024] = {0};
    size_t len = core3_map_serialize(MapLTFT, temp_buf);

    dprintf("[ECU] Cal writing %d bytes\n", len);

    // core3_flash_cal_erase(0, 0);
    // core3_flash_cal_write(maps_offset, temp_buf, len);

    core3_ecu_cal_writeToFlash((void *)0x100, temp_buf, len);
    dprintf("[ECU] Maps written (%d bytes)\n", len);
}

void core3_ecu_update_task(void *arg);

void core3_ecu_init(bool isReInit)
{
    dbw_target = 0;
    use_dbw = false;

    bool eraseCal = false;
    size_t maps_offset = 0x100;

    dprintf("[ECU] %sInit, cal offset %d bytes\n", isReInit ? "Re" : "", maps_offset);

    // core3_ecu_ltft_serialize(core3_ecu_cal_writeToFlash, NULL);

    const void *MapLTFT_Flash = core3_flash_cal_offset(maps_offset);
    if (MapLTFT_Flash == NULL)
    {
        dprintf("[ECU] Oh no!\n");
        return;
    }

    size_t read_bytes = 0;
    if (!core3_map_deserialize(MapLTFT_Flash, &MapLTFT, &read_bytes))
    {
        dprintf("[ECU] LTFT generating\n");
        MapLTFT = core3_map_create(AxisX_LFTF, sizeof(AxisX_LFTF) / sizeof(*AxisX_LFTF), AxisY_LFTF, sizeof(AxisY_LFTF) / sizeof(*AxisY_LFTF));

        for (size_t y = 0; y < MapLTFT->y.len; y++)
        {
            for (size_t x = 0; x < MapLTFT->x.len; x++)
            {
                core3_map_set_raw(MapLTFT, x, y, correction_to_byte(1.0f));
            }
        }

        eraseCal = true;
    }
    else
    {
        dprintf("[ECU] LTFT (%d bytes) ... OK\n", read_bytes);
    }

    if (eraseCal && isReInit)
        eraseCal = false;

    if (eraseCal)
    {
        core3_ecu_cal_write_all();
    }

    if (!isReInit)
        xTaskCreate(core3_ecu_update_task, "c3_ecu_update", 1024 * 15, NULL, CORE3_ECU_UPDATE_PRIORITY, NULL);
    /**dprintf("Indexing map\n");
    uint8_t map_val = core3_map_index(&MapLTFT, 5, 878, NULL, NULL, NULL, NULL);
    dprintf("MAP_VAL = 0x%02X, %d, %f\n", map_val, (int)map_val, byte_to_correction(map_val));

    dprintf("LTFT X = %d, Y = %d\n", MapLTFT.x.len, MapLTFT.y.len);*/
}

bool ecu_dirty = false;
uint32_t dirty_time = 0;
uint32_t clear_time = 0;

void core3_ecu_mark_dirty()
{
    dirty_time = core3_time_ms();
    ecu_dirty = true;
}

void core3_ecu_mark_clear_time()
{
    clear_time = core3_time_ms();
}

void core3_ecu_update_task(void *arg)
{
    uint32_t ms;

    while (true)
    {
        if (ecu_dirty && dirty_time != 0)
        {
            ms = core3_time_ms();
            if (ms - dirty_time > 1000)
            {
                ecu_dirty = false;
                dirty_time = 0;
                clear_time = 0;
                core3_ecu_init(true);
            }
        }

        if (clear_time != 0 && !ecu_dirty)
        {
            ms = core3_time_ms();
            if (ms - clear_time > 1000 * 2)
            {
                clear_time = 0;
                core3_ecu_cal_write_all();
            }
        }

        core3_ecu_tick();

        /*bt_stream_counter++;
        if (bt_stream_counter > 3)
        {
            bt_stream_counter = 0;
        }*/

        vTaskDelay(pdMS_TO_TICKS(10));
    }
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
