#include <EEPROM.h>
#include <ESP8266WiFi.h>
#include <MFRC522.h>
#include <SPI.h>
#include <LittleFS.h>
#include <Blinkenlight.h>

// User-define paramanter
#define DEBUG_MODE true

// Tag related parameters
#define MASTER_ADD \
  { 0x45, 0xFF, 0x55, 0xBE }
#define MASTER_REMOVE \
  { 0xF5, 0x07, 0x60, 0xBE }
#define RFID_TAG_SIZE 4
#define RFID_TAGS_FILE "/rfid_tags.bin"
#define MASTER_TAGS_FILE "/master_tags.bin"
#define TAG_DELAY_MS 3000

// Feedback parameters
#define BLINK_INTERVAL_MS 100
#define CONFIRM_TIME_MS 100
#define MAX_BLINKS 6

// GPIO pins - Outputs
#define PIN_LED_G 9           // S3
#define PIN_LED_R 10          // SK
#define PIN_RELAY_DOOR 4      // D2
#define PIN_RELAY_DOORBELL 5  // D1
#define PIN_BUZZER 0          // D3

// GPIO pins - SPI bus
#define PIN_SDA 15   // D8
#define PIN_SCK 14   // D5
#define PIN_MISO 12  // D6
#define PIN_MOSI 13  // D7

// Declarações de tipos e classes
MFRC522 mfrc522(PIN_SDA, UINT8_MAX);
enum TagType {
  NORMAL,
  MASTER_CAD,
  MASTER_DESCAD
};

Blinkenlight ledR(PIN_LED_R);
Blinkenlight ledG(PIN_LED_G);
Blinkenlight buzzer(PIN_BUZZER);

void setup() {
  Serial.begin(115200);
  SPI.begin();
  mfrc522.PCD_Init();

  if (!LittleFS.begin()) {
    Serial.println("Falha ao inicializar o arquivo (LittleFS)");
    return;
  }

  // Cria o arquivo de tags normais se não existir
  if (!LittleFS.exists(RFID_TAGS_FILE)) {
    File file = LittleFS.open(RFID_TAGS_FILE, "w");
    if (file) {
      Serial.println("Arquivo de tags criado");
      file.close();
    } else {
      Serial.println("Erro ao criar o arquivo de tags");
    }
  }

  // Cria o arquivo de tags mestres se não existir e insere as tags hardcoded
  if (!LittleFS.exists(MASTER_TAGS_FILE)) {
    File file = LittleFS.open(MASTER_TAGS_FILE, "w");
    if (file) {
      uint8_t masterCad[RFID_TAG_SIZE] = MASTER_ADD;
      uint8_t masterDescad[RFID_TAG_SIZE] = MASTER_REMOVE;

      file.write(masterCad, RFID_TAG_SIZE);
      file.write(masterDescad, RFID_TAG_SIZE);

      Serial.println("Arquivo de tags mestres criado e populado");
      file.close();
    } else {
      Serial.println("Falha ao criar arquivo de tags mestres");
    }
  } else {
    Serial.println("Arquivo de tags mestre já existe");
  }

  pinMode(PIN_BUZZER, OUTPUT);
  digitalWrite(PIN_BUZZER, LOW);

  pinMode(PIN_LED_G, OUTPUT);
  pinMode(PIN_LED_R, OUTPUT);

  digitalWrite(PIN_LED_G, LOW);
  digitalWrite(PIN_LED_R, LOW);
}

void loop() {

  ledR.update();
  ledG.update();
  buzzer.update();

  uint8_t uid[RFID_TAG_SIZE];
  unsigned long currentMillis = millis();

  // Variáveis de controle
  static uint32_t waitStart = 0;
  static uint8_t mode = 0;  // Modo de tag; 0 = normal, 1 = cadastramento, 2 = descadastramento

  switch (mode) {

    case 0:  // Modo normal, que aguarda a leitura de uma tag


      if (readRFID(uid)) {
        TagType tagType = checkMasterTag(uid);

        // switch para detectar o tipo de tag
        switch (tagType) {

          case MASTER_CAD:
            Serial.println("Modo de CADASTRAMENTO ativado. Aguardando nova Tag...");
            mode = 1;
            waitStart = currentMillis;
            break;

          case MASTER_DESCAD:
            Serial.println("Modo de DESCADASTRAMENTO ativado. Aguardando Tag para remover...");
            waitStart = currentMillis;
            mode = 2;
            break;

          case NORMAL:
            if (checkRFID(uid)) {
              if (DEBUG_MODE) { Serial.println("Entrada confirmada"); }
              aceitarEntrada();
            } else {
              if (DEBUG_MODE) { Serial.println("Entrada negada"); }
              negarEntrada();
            }
            break;
        }
      }
      break;

    case 1:  // modo de cadastramento
      if (currentMillis - waitStart < TAG_DELAY_MS) {
        ledG.blink(SPEED_RAPID);
        if (readRFID(uid)) {
          if (!checkRFID(uid)) {
            storeRFID(uid);
            Serial.println("Nova tag cadastrada");
            ledG.pattern(1, false);
            buzzer.pattern(1, false);
            mode = 0;
          } else {
            Serial.println("Tag já cadastrada");
            ledR.pattern(3, SPEED_RAPID, false);
            buzzer.pattern(3, SPEED_RAPID, false);
          }
        }
      } else {
        Serial.println("Tempo expirado para cadastramento");
        ledG.off();
        mode = 0;
      }
      break;

    case 2:  // modo de descadastramento, lê continuamente um cartão para cadastrar
      if (currentMillis - waitStart < TAG_DELAY_MS) {
        ledR.blink(SPEED_RAPID);
        if (readRFID(uid)) {
          if (removeRFID(uid)) {
            Serial.println("Tag removida");
            ledR.pattern(1, false);
            buzzer.pattern(1, false);
            mode = 0;
          } else {
            Serial.println("Tag não encontrada");
            ledR.pattern(3, SPEED_RAPID, false);
            buzzer.pattern(3, SPEED_RAPID, false);
          }
        }
      } else {
        Serial.println("Tempo expirado para descadastramento");
        ledR.off();
        mode = 0;
      }
      break;
  }
}

//---Utility Funtions----------------------------------------------------------------------------//

// Realiza a leitura de um RFID para um buffer
bool readRFID(uint8_t *uidBuffer) {
  if (!mfrc522.PICC_IsNewCardPresent() || !mfrc522.PICC_ReadCardSerial()) {
    return false;
  }

  if (mfrc522.uid.size != RFID_TAG_SIZE) {
    Serial.println("UID inválido");
    return false;
  }

  memcpy(uidBuffer, mfrc522.uid.uidByte, RFID_TAG_SIZE);  // Salva a tag atual no buffer

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

// Guarda a tag lida no armazenamento interno do ESP
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

// Remove a tag lida do armazenamento interno do ESP
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

// Checa se a tag existe
bool checkRFID(uint8_t *uidToCheck) {
  File file = LittleFS.open(RFID_TAGS_FILE, "r");
  if (!file) {
    Serial.println("Erro ao abrir o arquivo de tags");
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

// Checa se a tag é mestre
TagType checkMasterTag(uint8_t *uid) {
  File file = LittleFS.open(MASTER_TAGS_FILE, "r");
  if (!file) {
    Serial.println("Erro ao abrir arquivo de tags mestres");
    return NORMAL;
  }

  uint8_t masterCadastro[RFID_TAG_SIZE], masterRemocao[RFID_TAG_SIZE];
  if (file.read(masterCadastro, RFID_TAG_SIZE) != RFID_TAG_SIZE || file.read(masterRemocao, RFID_TAG_SIZE) != RFID_TAG_SIZE) {
    Serial.println("Erro ao ler tags mestres!");
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

// --- Funções de processo ---------//
void aceitarEntrada() {
  ledG.pattern(1, false);
  buzzer.pattern(1, false);
}

void negarEntrada() {
  ledR.pattern(3, SPEED_RAPID, false);
  buzzer.pattern(3, SPEED_RAPID, false);
}




// // Sinal de confirmação ao abrir a porta
// void confirmTone() {
//   digitalWrite(PIN_LED_G, HIGH);
//   digitalWrite(PIN_BUZZER, HIGH);
//   delay(200);
//   digitalWrite(PIN_BUZZER, LOW);
//   digitalWrite(PIN_LED_G, LOW);
// }

// // Sinal de negação da porta
// void denyTone() {
//   digitalWrite(PIN_LED_R, HIGH);
//   for (int i = 0; i < 3; i++) {
//     digitalWrite(PIN_BUZZER, HIGH);
//     delay(100);
//     digitalWrite(PIN_BUZZER, LOW);
//     delay(100);
//   }
//   digitalWrite(PIN_BUZZER, LOW);
//   digitalWrite(PIN_LED_R, LOW);
// }
