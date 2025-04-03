#include "control.h"
#include "secrets.h"
#include "Tags.h"

#include <LittleFS.h>

bool readRFID(uint8_t *uidBuffer) {
  if (!mfrc522.PICC_IsNewCardPresent() || !mfrc522.PICC_ReadCardSerial()) {
    return false;
  }

  if (mfrc522.uid.size != RFID_TAG_SIZE) {
    if (DEBUG_MODE) Serial.println("UID inválido");
    return false;
  }

  memcpy(uidBuffer, mfrc522.uid.uidByte, RFID_TAG_SIZE);  // Salva a tag atual no buffer

  if (DEBUG_MODE) {
    Serial.print("UID lido: ");
    for (int i = 0; i < RFID_TAG_SIZE; i++) {
      Serial.printf("%02X ", uidBuffer[i]);
    }
    Serial.print("Tipo de PICC: ");
    Serial.println(mfrc522.PICC_GetTypeName(mfrc522.PICC_GetType(mfrc522.uid.sak)));
  }

  mfrc522.PICC_HaltA();

  return true;
}

bool checkRFID(uint8_t *uidToCheck) {
  File file = LittleFS.open(RFID_TAGS_FILE, "r");
  if (!file) {
    if (DEBUG_MODE) Serial.println("Erro ao abrir o arquivo de tags");
    return false;
  }

  uint8_t uid[RFID_TAG_SIZE];
  while (file.read(uid, RFID_TAG_SIZE) == RFID_TAG_SIZE) {
    if (memcmp(uid, uidToCheck, RFID_TAG_SIZE) == 0) {
      file.close();
      return true;  // tag encontrada
    }
  }

  file.close();
  return false;  //tag não encontrada
}

// Guarda a tag lida no armazenamento interno do ESP
bool storeRFID(uint8_t *uid) {
  File file = LittleFS.open(RFID_TAGS_FILE, "a");
  if (!file) {
    if (DEBUG_MODE) Serial.println("Erro ao abrir o arquivo de tags para escrita");
    return false;
  }

  file.write(uid, RFID_TAG_SIZE);
  file.close();
  return true;
}

// Remove a tag lida do armazenamento interno do ESP
bool removeRFID(uint8_t *uidToRemove) {
  File file = LittleFS.open(RFID_TAGS_FILE, "r");
  if (!file) {
    if (DEBUG_MODE) Serial.println("Erro ao abrir o arquivo de tags");
    return false;
  }

  File tempFile = LittleFS.open("/temp.bin", "w");
  if (!tempFile) {
    if (DEBUG_MODE) Serial.println("Erro ao criar arquivo temporario");
    file.close();
    return false;
  }

  uint8_t uid[RFID_TAG_SIZE];
  bool removed = false;
  while (file.read(uid, RFID_TAG_SIZE) == RFID_TAG_SIZE) {  // itera a lista de tags de 4 em 4 bytes (tag em tag)
    if (memcmp(uid, uidToRemove, RFID_TAG_SIZE) != 0) {
      tempFile.write(uid, RFID_TAG_SIZE);
    } else {
      removed = true;  // tag removida
    }
  }

  file.close();
  tempFile.close();
  // Se uma tag foi removida, substituímos o arquivo original
  if (removed) {
    LittleFS.remove(RFID_TAGS_FILE);
    LittleFS.rename("/temp.bin", RFID_TAGS_FILE);
  } else {
    LittleFS.remove("/temp.bin");  // Nenhuma tag foi removida, deletamos o temporário
  }

  return removed;
}

TagType checkMasterTag(uint8_t *uid) {
  File file = LittleFS.open(MASTER_TAGS_FILE, "r");
  if (!file) {
    if (DEBUG_MODE) Serial.println("Erro ao abrir arquivo de tags mestres");
    return NORMAL;
  }

  uint8_t masterCadastro[RFID_TAG_SIZE], masterRemocao[RFID_TAG_SIZE];
  if (file.read(masterCadastro, RFID_TAG_SIZE) != RFID_TAG_SIZE || file.read(masterRemocao, RFID_TAG_SIZE) != RFID_TAG_SIZE) {
    if (DEBUG_MODE) Serial.println("Erro ao ler tags mestres!");
    file.close();
    return NORMAL;
  }

  file.close();

  if (memcmp(uid, masterCadastro, RFID_TAG_SIZE) == 0) {
    return MASTER_CAD;
  }
  if (memcmp(uid, masterRemocao, RFID_TAG_SIZE) == 0) {
    return MASTER_DESCAD;
  }

  return NORMAL;
}