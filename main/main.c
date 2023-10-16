#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_mac.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "driver/gpio.h"
#include "lwip/err.h"
#include "lwip/sys.h"
#include <stdlib.h>
#include <unistd.h>
#include <sys/param.h>
#include "esp_netif.h"
#include <esp_http_server.h>
#include "esp_tls.h"
#if !CONFIG_IDF_TARGET_LINUX
#include <esp_system.h>

#endif  // !CONFIG_IDF_TARGET_LINUX

static const char *TAG = "webserver";


#define EXAMPLE_ESP_WIFI_SSID      CONFIG_ESP_WIFI_SSID
#define EXAMPLE_ESP_WIFI_PASS      CONFIG_ESP_WIFI_PASSWORD
#define EXAMPLE_ESP_WIFI_CHANNEL   CONFIG_ESP_WIFI_CHANNEL
#define EXAMPLE_MAX_STA_CONN       CONFIG_ESP_MAX_STA_CONN
#define GPIO_RED    GPIO_NUM_1 

static esp_err_t ledon_handler(httpd_req_t *req)
{
    esp_err_t error;
    gpio_set_level(GPIO_RED, 0);
    ESP_LOGI(TAG, "LED Turned ON");
    const char *response = (const char *) req->user_ctx;
    error= httpd_resp_send(req, response, strlen(response));
    if(error != ESP_OK)
    {
        ESP_LOGI(TAG, "ERROR %d while sending Response", error);
    }
    else ESP_LOGI(TAG, "Response sent successfully");
    return error;
}

static const httpd_uri_t ledon = {
    .uri       = "/ledon",
    .method    = HTTP_GET,
    .handler   = ledon_handler,
    .user_ctx  = "<!DOCTYPE html>"\
                 "<html>"\
                 "<head>"\
                 "<style>"\
                 ".button {"\
                 "  border: none;"\
                 "  color: white;"\
                 "  padding: 15px 32px;"\
                 "  text-align: center;"\
                 "  text-decoration: none;"\
                 "  display: inline-block;"\
                 "  font-size: 16px;"\
                 "  margin: 4px 2px;"\
                 "  cursor: pointer;"\
                 "}"\
                 ".button1 {background-color: #000000;}"\
                 "</style>"\
                 "</head>"\
                 "<body>"\
                 "<h1>ESP32-S3 Webserver</h1>"\
                 "<p>Change LED State</p>"\
                 "<h3>LED STATE: ON<h3>"\
                 "<button class=\"button button1\" onclick = \"window.location.href='/ledoff'\">TURN LED OFF</button>"\
                 "</body>"\
                 "</html>"
};


/* An HTTP GET handler */
static esp_err_t ledoff_handler(httpd_req_t *req)
{
    esp_err_t error;
    gpio_set_level(GPIO_RED, 1);
    ESP_LOGI(TAG, "LED Turned Off");
    const char *response = (const char *) req->user_ctx;
    error= httpd_resp_send(req, response, strlen(response));
    if(error != ESP_OK)
    {
        ESP_LOGI(TAG, "ERROR %d while sending Response", error);
    }
    else ESP_LOGI(TAG, "Response sent successfully");
    return error;
}

static const httpd_uri_t ledoff = {
    .uri       = "/ledoff",
    .method    = HTTP_GET,
    .handler   = ledoff_handler,
    .user_ctx  = "<!DOCTYPE html>"\
                 "<html>"\
                 "<head>"\
                 "<style>"\
                 ".button {"\
                 "  border: none;"\
                 "  color: white;"\
                 "  padding: 15px 32px;"\
                 "  text-align: center;"\
                 "  text-decoration: none;"\
                 "  display: inline-block;"\
                 "  font-size: 16px;"\
                 "  margin: 4px 2px;"\
                 "  cursor: pointer;"\
                 "}"\
                 ".button1 {background-color: #4CAF50;}"\
                 "</style>"\
                 "</head>"\
                 "<body>"\
                 "<h1>ESP32-S3 Webserver</h1>"\
                 "<p>Change LED State</p>"\
                 "<h3>LED STATE: OFF<h3>"\
                 "<button class=\"button button1\" onclick = \"window.location.href='/ledon'\">TURN LED ON</button>"\
                 "</body>"\
                 "</html>"
};



esp_err_t http_404_error_handler(httpd_req_t *req, httpd_err_code_t err)
{

    /* For any other URI send 404 and close socket */
    httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "Some 404 error message");
    return ESP_FAIL;
}


static httpd_handle_t start_webserver(void)
{
    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
#if CONFIG_IDF_TARGET_LINUX
    // Setting port as 8001 when building for Linux. Port 80 can be used only by a priviliged user in linux.
    // So when a unpriviliged user tries to run the application, it throws bind error and the server is not started.
    // Port 8001 can be used by an unpriviliged user as well. So the application will not throw bind error and the
    // server will be started.
    config.server_port = 8001;
    // server will be started.
    config.server_port = 8001;
#endif // !CONFIG_IDF_TARGET_LINUX
    config.lru_purge_enable = true;

    // Start the httpd server
    ESP_LOGI(TAG, "Starting server on port: '%d'", config.server_port);
    
    if (httpd_start(&server, &config) == ESP_OK) {
        // Set URI handlers
        ESP_LOGI(TAG, "Registering URI handlers");
        httpd_register_uri_handler(server, &ledoff);
        httpd_register_uri_handler(server, &ledon);
        return server;
    }

    ESP_LOGI(TAG, "Error starting server!");
    return NULL;
}

#if !CONFIG_IDF_TARGET_LINUX
static esp_err_t stop_webserver(httpd_handle_t server)
{
    // Stop the httpd server
    return httpd_stop(server);
}

static void disconnect_handler(void* arg, esp_event_base_t event_base,
                               int32_t event_id, void* event_data)
{
    httpd_handle_t* server = (httpd_handle_t*) arg;
    if (*server) {
        ESP_LOGI(TAG, "Stopping webserver");
        if (stop_webserver(*server) == ESP_OK) {
            *server = NULL;
        } else {
            ESP_LOGE(TAG, "Failed to stop http server");
        }
    }
}

static void connect_handler(void* arg, esp_event_base_t event_base,
                            int32_t event_id, void* event_data)
{
    httpd_handle_t* server = (httpd_handle_t*) arg;
    if (*server == NULL) {
        ESP_LOGI(TAG, "Starting webserver");
        *server = start_webserver();
    }
}
#endif // !CONFIG_IDF_TARGET_LINUX

static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                                    int32_t event_id, void* event_data)
{
    if (event_id == WIFI_EVENT_AP_STACONNECTED) {
        wifi_event_ap_staconnected_t* event = (wifi_event_ap_staconnected_t*) event_data;
        ESP_LOGI(TAG, "station "MACSTR" join, AID=%d",
                 MAC2STR(event->mac), event->aid);
    } else if (event_id == WIFI_EVENT_AP_STADISCONNECTED) {
        wifi_event_ap_stadisconnected_t* event = (wifi_event_ap_stadisconnected_t*) event_data;
        ESP_LOGI(TAG, "station "MACSTR" leave, AID=%d",
                 MAC2STR(event->mac), event->aid);
    }
}

void wifi_init_softap(void)
{
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_ap();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        NULL));

    wifi_config_t wifi_config = {
        .ap = {
            .ssid = EXAMPLE_ESP_WIFI_SSID,
            .ssid_len = strlen(EXAMPLE_ESP_WIFI_SSID),
            .channel = EXAMPLE_ESP_WIFI_CHANNEL,
            .password = EXAMPLE_ESP_WIFI_PASS,
            .max_connection = EXAMPLE_MAX_STA_CONN,
#ifdef CONFIG_ESP_WIFI_SOFTAP_SAE_SUPPORT
            .authmode = WIFI_AUTH_WPA3_PSK,
            .sae_pwe_h2e = WPA3_SAE_PWE_BOTH,
#else /* CONFIG_ESP_WIFI_SOFTAP_SAE_SUPPORT */
            .authmode = WIFI_AUTH_WPA2_PSK,
#endif
            .pmf_cfg = {
                    .required = true,
            },
        },
    };
    if (strlen(EXAMPLE_ESP_WIFI_PASS) == 0) {
        wifi_config.ap.authmode = WIFI_AUTH_OPEN;
    }

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "wifi_init_softap finished. SSID:%s password:%s channel:%d",
             EXAMPLE_ESP_WIFI_SSID, EXAMPLE_ESP_WIFI_PASS, EXAMPLE_ESP_WIFI_CHANNEL);
}

static void configure_led(void)
{
    gpio_reset_pin(GPIO_RED);
    gpio_set_direction(GPIO_RED, GPIO_MODE_OUTPUT);
    gpio_set_level(GPIO_RED, 1);
}

void app_main(void)
{
    configure_led();
    static httpd_handle_t server = NULL;
    //Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_LOGI(TAG, "ESP_WIFI_MODE_AP");
    wifi_init_softap();

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_AP_STAIPASSIGNED, &connect_handler, &server));
}
