#include <ESP8266WiFi.h>

#define WIFI_TIMEOUT_MS 10000

WiFiClientSecure *client;
BearSSL::CertStore certStore;

void WifiInit();

void CertStoreInit();