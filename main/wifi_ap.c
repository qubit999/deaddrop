#include "wifi_ap.h"
#include "constants.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "lwip/dns.h"
#include "lwip/err.h"
#include "lwip/sys.h"
#include <string.h>

static const char *TAG = "wifi_ap";
static bool ap_running = false;
static esp_netif_t *ap_netif = NULL;

static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                               int32_t event_id, void* event_data)
{
    if (event_id == WIFI_EVENT_AP_STACONNECTED) {
        wifi_event_ap_staconnected_t* event = (wifi_event_ap_staconnected_t*) event_data;
        ESP_LOGI(TAG, "Station %02x:%02x:%02x:%02x:%02x:%02x connected, AID=%d",
                 event->mac[0], event->mac[1], event->mac[2],
                 event->mac[3], event->mac[4], event->mac[5], event->aid);
    } else if (event_id == WIFI_EVENT_AP_STADISCONNECTED) {
        wifi_event_ap_stadisconnected_t* event = (wifi_event_ap_stadisconnected_t*) event_data;
        ESP_LOGI(TAG, "Station %02x:%02x:%02x:%02x:%02x:%02x disconnected, AID=%d",
                 event->mac[0], event->mac[1], event->mac[2],
                 event->mac[3], event->mac[4], event->mac[5], event->aid);
    }
}

esp_err_t wifi_ap_init(void)
{
    // Initialize network interface
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    // Create AP network interface
    ap_netif = esp_netif_create_default_wifi_ap();

    // Configure static IP
    esp_netif_dhcps_stop(ap_netif);
    
    esp_netif_ip_info_t ip_info;
    memset(&ip_info, 0, sizeof(ip_info));
    ip_info.ip.addr = ipaddr_addr(WIFI_AP_IP);
    ip_info.gw.addr = ipaddr_addr(WIFI_AP_GATEWAY);
    ip_info.netmask.addr = ipaddr_addr(WIFI_AP_NETMASK);
    
    ESP_ERROR_CHECK(esp_netif_set_ip_info(ap_netif, &ip_info));
    ESP_ERROR_CHECK(esp_netif_dhcps_start(ap_netif));

    // Initialize WiFi
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        NULL));

    // Configure AP
    wifi_config_t wifi_config = {
        .ap = {
            .ssid_len = strlen(WIFI_AP_SSID),
            .channel = WIFI_AP_CHANNEL,
            .max_connection = WIFI_AP_MAX_CONNECTIONS,
            .authmode = WIFI_AUTH_OPEN,
            .pmf_cfg = {
                .required = false,
            },
        },
    };
    
    strncpy((char *)wifi_config.ap.ssid, WIFI_AP_SSID, sizeof(wifi_config.ap.ssid));
    
    // Set password if configured
    if (strlen(WIFI_AP_PASSWORD) > 0) {
        strncpy((char *)wifi_config.ap.password, WIFI_AP_PASSWORD, sizeof(wifi_config.ap.password));
        wifi_config.ap.authmode = WIFI_AUTH_WPA2_PSK;
    }

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));

    ESP_LOGI(TAG, "WiFi AP initialized: SSID=%s, Channel=%d, Auth=%s",
             WIFI_AP_SSID, WIFI_AP_CHANNEL,
             wifi_config.ap.authmode == WIFI_AUTH_OPEN ? "Open" : "WPA2");

    return ESP_OK;
}

esp_err_t wifi_ap_start(void)
{
    if (ap_running) {
        ESP_LOGW(TAG, "WiFi AP already running");
        return ESP_OK;
    }

    ESP_ERROR_CHECK(esp_wifi_start());
    ap_running = true;

    // Configure DNS server for captive portal
    esp_netif_dns_info_t dns_info;
    dns_info.ip.u_addr.ip4.addr = ipaddr_addr(WIFI_AP_IP);
    dns_info.ip.type = IPADDR_TYPE_V4;
    ESP_ERROR_CHECK(esp_netif_set_dns_info(ap_netif, ESP_NETIF_DNS_MAIN, &dns_info));

    ESP_LOGI(TAG, "WiFi AP started: %s", WIFI_AP_SSID);
    return ESP_OK;
}

esp_err_t wifi_ap_stop(void)
{
    if (!ap_running) {
        ESP_LOGW(TAG, "WiFi AP not running");
        return ESP_OK;
    }

    ESP_ERROR_CHECK(esp_wifi_stop());
    ap_running = false;

    ESP_LOGI(TAG, "WiFi AP stopped");
    return ESP_OK;
}

bool wifi_ap_is_running(void)
{
    return ap_running;
}
