#include "Wifi.h"
#include "Secrets.h"
#include "control.h"
#include "FileSys.h"

void WifiInit()
{
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    unsigned long startAttemptTime = millis();

    Serial.println("Conectando ao WiFi");

    while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < WIFI_TIMEOUT_MS)
    {
        delay(500);
        Serial.println(".");
    }

    if (WiFi.status() == WL_CONNECTED)
    {
        Serial.println("\nWiFi conectado");
        Serial.print("Endereço IP: ");
        Serial.println(WiFi.localIP());
    }
    else
    {
        Serial.println("\nFalha ao conectar o WiFi");
    }
}

void setCertStore()
{
    int numCerts = certStore.initCertStore(LittleFS, PSTR("/certs.idx"), PSTR("/certs.ar"));
    if (DEBUG_MODE)
        Serial.printf("Certificados CA encontrados: %d \n", numCerts);
    if (numCerts == 0)
    {
        Serial.println("Nenhum certificado encontrado");
        return;
    }

    client = new WiFiClientSecure();
    client->setCertStore(&certStore);
}