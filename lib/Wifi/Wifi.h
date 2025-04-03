#ifndef WIFI_H
#define WIFI_H
#include <ESP8266WiFi.h>

#define WIFI_TIMEOUT_MS 10000

extern WiFiClientSecure *client;
extern BearSSL::CertStore certStore;

void WifiInit();

void CertStoreInit();
#endif