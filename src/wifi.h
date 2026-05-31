#pragma once
#include "esp_wifi.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_sntp.h"
//wifi password and ssid
#define WIFI_SSID "toe"
#define WIFI_PASS "javito123"
//functions
esp_err_t wifi_sta_init();
bool have_wifi();