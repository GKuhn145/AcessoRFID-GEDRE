#include <EEPROM.h>
#include <ESP8266WiFi.h>
#include <MFRC522.h>
#include <SPI.h>
#include <LittleFS.h>

// User-define paramanter
#define DEBUG_MODE                true

// Tag related parameters
#define MASTER_ADD                {0x45, 0xFF, 0x55, 0xBE}
byte masterAdd[] = MASTER_ADD;
#define MASTER_REMOVE             {0xF5, 0x07, 0x60, 0xBE}
byte masterRemove[] = MASTER_REMOVE;
#define RFID_TAG_SIZE             4
#define RFID_TAGS_FILE            "/rfid_tags.bin"

// GPIO pins - Outputs
#define PIN_LED_G                 9   // S3
#define PIN_LED_R                 10  // SK
#define PIN_RELAY_DOOR            4   // D2
#define PIN_RELAY_DOORBELL        5   // D1
#define PIN_BUZZER                0   // D3

// GPIO pins - SPI bus
#define PIN_SDA                   15  // D8
#define PIN_SCK                   14  // D5
#define PIN_MISO                  12  // D6
#define PIN_MOSI                  13  // D7


MFRC522 mfrc522(PIN_SDA, UINT8_MAX);
bool card_equal;

// Array that stores the code of the current card being read
byte size;
byte *currTag;

void setup()
{
  Serial.begin(9600);
  SPI.begin();       
  mfrc522.PCD_Init();

  if (!LittleFS.begin()) {
    Serial.println("Falha ao inicializar o arquivo (LittleFS)");
    return;
  }

  pinMode(PIN_BUZZER, OUTPUT);
  digitalWrite(PIN_BUZZER, LOW);

  pinMode(PIN_LED_G, OUTPUT);
  pinMode(PIN_LED_R, OUTPUT);

  digitalWrite(PIN_LED_G, LOW);
  digitalWrite(PIN_LED_R, LOW);
}

void loop() {

  uint8_t uid[RFID_TAG_SIZE];

  if (readRFID(uid)) {
    if (checkUID(uid)){
      confirmTone()
    }
    else {
      denyTone()
    }
  }

}






//---Utility Funtions----------------------------------------------------------------------------//

// Realiza a leitura de um RFID para um buffer
bool readRFID(uint8_t *uidBuffer) {
  if (!mfrc522.PICC_IsNewCardPresent() || !mfrc522.PICC_ReadCardSerial()) {
    return false;
  }

  if(mfrc522.uid.size != RFID_TAG_SIZE) {
    Serial.println("UID inválido");
    return false;
  }

  memcpy(uidBuffer, mfrc522.uid.uidByte, RFID_TAG_SIZE); // Salva a tag atual no buffer

  if (DEBUG_MODE == true) {
    Serial.print("UID lido: ");
    for (int i = 0; i < RFID_TAG_SIZE; i++) {
        Serial.printf("%02X ", uidBuffer[i]);
    }
    Serial.print("Tipo de PICC: ");
    Serial.println(mfrc522.PICC_GetTypeName(mfrc522.PICC_GetType(mfrc522.uid.sak)));
    Serial.println();
  }

  mfrc522.PICC_HaltA();

  return true;
}

bool storeRFID(uint8_t *uid) {
  File file = LittleFS.open(RFID_TAGS_FILE, "a");
  if (!file) {
    Serial.println("Erro ao abrir o arquivo de tags para escrita");
    return false;
  }

  file.write(uid, RFID_TAG_SIZE);
  file.close();
  return true;
}

bool removeRFID(uint8_t *uidToRemove) {
  File file = LittleFS.open(RFID_TAGS_FILE, "r");
  if (!file) {
    Serial.println("Erro ao abrir o arquivo de tags");
    return false;
  }

  File tempFile = LittleFS.open("/temp.bin", "w");
  if (!tempFile) {
    Serial.println("Erro ao criar arquivo temporario");
    file.close();
    return false;
  }

  uint8_t uid[RFID_TAG_SIZE];
  bool remove = false;
  while (file.read(uid, RFID_TAG_SIZE) == RFID_TAG_SIZE) { // itera a lista de tags de 4 em 4 bytes (tag em tag)
    if (memcmp(uid, uidToRemove, RFID_TAG_SIZE) != 0) {
      tempFile.write(uid, RFID_TAG_SIZE);
    }
    else {
      removed = true; // tag removida
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

bool checkUID(uint8_t *uidToCheck) {
  File file = LittleFS.open(RFID_TAGS_FILE, "r");
  if (!file) {
    Serial.println("Erro ao abrir o arquivo de tags");
    return false;
  }

  uint8_t uid[RFID_TAG_SIZE];
  while(file.read(uid, RFID_TAG_SIZE) == RFID_TAG_SIZE) {
    if (memcmp(uid, uidToCheck, RFID_TAG_SIZE) == 0) {
      file.close();
      return true; // tag encontrada
    }
  }

  file.close();
  return false; //tag não encontrada
}

// Sinal de confirmação ao abrir a porta
void confirmTone() {
  digitalWrite(PIN_LED_G, HIGH);
  digitalWrite(PIN_BUZZER, HIGH);
  delay(200);
  digitalWrite(PIN_BUZZER, LOW);
  digitalWrite(PIN_LED_G, LOW);
}

// Sinal de negação da porta
void denyTone() {
  digitalWrite(PIN_LED_R, HIGH);
  for (int i = 0; i < 3; i++) {
    digitalWrite(PIN_BUZZER, HIGH);
    delay(100);
    digitalWrite(PIN_BUZZER, LOW);
    delay(100);
  }
  digitalWrite(PIN_BUZZER, LOW);
  digitalWrite(PIN_LED_R, LOW);
}



