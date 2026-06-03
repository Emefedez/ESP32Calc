#include "system/wifi_manager.h"

#include <cstdio>
#include <cstring>

#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "nvs_flash.h"

namespace esp32calc_alt {
namespace {

constexpr const char* TAG = "wifi";
constexpr EventBits_t kConnectedBit = BIT0;

}  // namespace

esp_err_t WifiManager::start(const WifiSettings& settings) {
  if (!settings.loaded || settings.ssid[0] == '\0') {
    return ESP_ERR_INVALID_ARG;
  }
  if (started_) {
    return ESP_OK;
  }

  esp_err_t err = nvs_flash_init();
  if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    ESP_ERROR_CHECK(nvs_flash_erase());
    err = nvs_flash_init();
  }
  if (err != ESP_OK) {
    return err;
  }

  err = esp_netif_init();
  if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
    return err;
  }
  err = esp_event_loop_create_default();
  if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
    return err;
  }
  esp_netif_create_default_wifi_sta();

  events_ = xEventGroupCreate();
  if (events_ == nullptr) {
    return ESP_ERR_NO_MEM;
  }

  wifi_init_config_t init_config = WIFI_INIT_CONFIG_DEFAULT();
  err = esp_wifi_init(&init_config);
  if (err != ESP_OK) {
    return err;
  }

  ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                      ESP_EVENT_ANY_ID,
                                                      &WifiManager::event_handler,
                                                      this,
                                                      nullptr));
  ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                      IP_EVENT_STA_GOT_IP,
                                                      &WifiManager::event_handler,
                                                      this,
                                                      nullptr));

  wifi_config_t wifi_config {};
  strncpy(reinterpret_cast<char*>(wifi_config.sta.ssid),
          settings.ssid,
          sizeof(wifi_config.sta.ssid) - 1);
  strncpy(reinterpret_cast<char*>(wifi_config.sta.password),
          settings.password,
          sizeof(wifi_config.sta.password) - 1);
  wifi_config.sta.threshold.authmode = settings.password[0] == '\0'
                                           ? WIFI_AUTH_OPEN
                                           : WIFI_AUTH_WPA2_PSK;
  wifi_config.sta.sae_pwe_h2e = WPA3_SAE_PWE_BOTH;

  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
  ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
  err = esp_wifi_start();
  if (err == ESP_OK) {
    started_ = true;
    ESP_LOGI(TAG, "connecting to %s", settings.ssid);
  }
  return err;
}

bool WifiManager::connected() const {
  return events_ != nullptr && (xEventGroupGetBits(events_) & kConnectedBit) != 0;
}

bool WifiManager::wait_connected(uint32_t timeout_ms) const {
  if (events_ == nullptr) {
    return false;
  }
  const EventBits_t bits = xEventGroupWaitBits(events_,
                                               kConnectedBit,
                                               pdFALSE,
                                               pdTRUE,
                                               pdMS_TO_TICKS(timeout_ms));
  return (bits & kConnectedBit) != 0;
}

void WifiManager::event_handler(void* arg,
                                esp_event_base_t event_base,
                                int32_t event_id,
                                void*) {
  auto* self = static_cast<WifiManager*>(arg);
  if (self == nullptr || self->events_ == nullptr) {
    return;
  }

  if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
    esp_wifi_connect();
  } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
    xEventGroupClearBits(self->events_, kConnectedBit);
    esp_wifi_connect();
    ESP_LOGW(TAG, "wifi disconnected, retrying");
  } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
    xEventGroupSetBits(self->events_, kConnectedBit);
    ESP_LOGI(TAG, "wifi connected");
  }
}

}  // namespace esp32calc_alt
