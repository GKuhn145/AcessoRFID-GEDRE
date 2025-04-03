#include "pins.h"

#include <MFRC522.h>
#include <SPI.h>

MFRC522 mfrc522(PIN_SDA, UINT8_MAX);

enum TagType {
    NORMAL,
    MASTER_CAD,
    MASTER_DESCAD
  };

// Lê uma tag para o buffer principal do loop
bool readRFID(uint8_t *uidBuffer);

// Armazena a tag internamente no LittleFS do ESP
bool storeRFID(uint8_t *uid);

// Remove uma tag internamente no LittleFS do ESP
bool removeRFID(uint8_t *uidToRemove);

// Checa se uma tag existe no armazenamento interno
bool checkRFID(uint8_t *uidToCheck);

// Checa se uma tag lida é uma tag mestre
TagType checkMasterTag(uint8_t *uid);
