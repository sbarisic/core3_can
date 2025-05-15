#include <core3.h>

#include <nvs_flash.h>
#include <esp_log.h>

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

void core3_init()
{
    esp_log_level_set("*", ESP_LOG_NONE);

    /* // Initialize NVS
     esp_err_t ret = nvs_flash_init();
     if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
     {
         dprintf("Doing nvs_flash_erase()\n");
         ESP_ERROR_CHECK(nvs_flash_erase());
         ret = nvs_flash_init();
     }*/

    // core3_flash_init();
}
