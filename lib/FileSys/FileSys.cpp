#include "control.h"
#include "Secrets.h"
#include "FileSys.h"



void FileSysInit()
{
    if (!LittleFS.begin())
    {
        Serial.println("Falha ao inicializar o arquivo (LittleFS)");
        return;
    }

    // Cria o arquivo de tags normais se não existir
    if (!LittleFS.exists(RFID_TAGS_FILE))
    {
        File file = LittleFS.open(RFID_TAGS_FILE, "w");
        if (file)
        {
            Serial.println("Arquivo de tags criado");
            file.close();
        }
        else
        {
            Serial.println("Erro ao criar o arquivo de tags");
        }
    }

    // Cria o arquivo de tags mestres se não existir e insere as tags hardcoded
    if (!LittleFS.exists(MASTER_TAGS_FILE))
    {
        File file = LittleFS.open(MASTER_TAGS_FILE, "w");
        if (file)
        {
            uint8_t masterCad[RFID_TAG_SIZE] = MASTER_ADD;
            uint8_t masterDescad[RFID_TAG_SIZE] = MASTER_REMOVE;

            file.write(masterCad, RFID_TAG_SIZE);
            file.write(masterDescad, RFID_TAG_SIZE);

            Serial.println("Arquivo de tags mestres criado e populado");
            file.close();
        }
        else
        {
            Serial.println("Falha ao criar arquivo de tags mestres");
        }
    }
    else
    {
        Serial.println("Arquivo de tags mestre já existe");
    }
}

void setDateTime()
{
    configTime(PSTR("<-03>3"), "pool.ntp.org", "time.nist.gov");

    if (DEBUG_MODE)
        Serial.print("Waiting for NTP time sync: ");
    time_t now = time(nullptr);
    while (now < 8 * 3600 * 2)
    {
        delay(100);
        if (DEBUG_MODE)
            Serial.print(".");
        now = time(nullptr);
    }

    struct tm timeinfo;
    gmtime_r(&now, &timeinfo);
    if (DEBUG_MODE)
        Serial.printf("%s %s", tzname[0], asctime(&timeinfo));
}