#include <core3_wifi.h>

#include "esp_event.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"
#include <string.h>
// #include "nvs_flash.h"
#include <esp_now.h>

#include "lwip/err.h"
#include "lwip/sys.h"

#define dprintf printf

#define EXAMPLE_ESP_WIFI_SSID "Serengeti"
#define EXAMPLE_ESP_WIFI_PASS "srgt#2018"
#define EXAMPLE_ESP_MAXIMUM_RETRY 3

#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT BIT1

static EventGroupHandle_t s_wifi_event_group;
static int s_retry_num = 0;

static void event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START)
    {
        esp_wifi_connect();
    }
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED)
    {
        if (s_retry_num < EXAMPLE_ESP_MAXIMUM_RETRY)
        {
            esp_wifi_connect();
            s_retry_num++;
            // dprintf("Retry to connect to the AP\n");
        }
        else
        {
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
        }

        // dprintf("Connect to the AP fail\n");
    }
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
    {
        // ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;

        // dprintf("Got ip: " IPSTR "\n", IP2STR(&event->ip_info.ip));

        s_retry_num = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

bool core3_wifi_delay_until_connected()
{
    EventBits_t bits =
        xEventGroupWaitBits(s_wifi_event_group, WIFI_CONNECTED_BIT | WIFI_FAIL_BIT, pdFALSE, pdFALSE, portMAX_DELAY);

    if (bits & WIFI_CONNECTED_BIT)
        return true;

    return false;
}

esp_err_t core3_wifi_init()
{
    s_wifi_event_group = xEventGroupCreate();

    // Init TCP/IP stack
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_err_t err = esp_wifi_init(&cfg);

    if (err != ESP_OK)
    {
        dprintf("esp_wifi_init - FAIL\n");
        return err;
    }

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;

    ESP_ERROR_CHECK(
        esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL, &instance_any_id));

    ESP_ERROR_CHECK(
        esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, NULL, &instance_got_ip));

    wifi_config_t wifi_config = {};
    wifi_sta_config_t sta = {};

    strcpy((char *)sta.ssid, EXAMPLE_ESP_WIFI_SSID);
    strcpy((char *)sta.password, EXAMPLE_ESP_WIFI_PASS);
    sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
    sta.sae_pwe_h2e = WPA3_SAE_PWE_UNSPECIFIED;
    strcpy((char *)sta.sae_h2e_identifier, "H2E Ident");
    wifi_config.sta = sta;

    err = esp_wifi_set_mode(WIFI_MODE_STA);
    if (err != ESP_OK)
    {
        dprintf("esp_wifi_set_mode failed\n");
        return err;
    }

    err = esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
    if (err != ESP_OK)
    {
        dprintf("esp_wifi_set_config failed\n");
        return err;
    }

    err = esp_wifi_start();
    if (err != ESP_OK)
    {
        dprintf("esp_wifi_start failed\n");
        return err;
    }

    dprintf("esp_wifi_init - OK\n");
    return ESP_OK;
}