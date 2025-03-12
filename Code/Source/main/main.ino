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
#define MASTER_SIZE               4
byte masterSize = MASTER_SIZE;
#define RFID_TAG_MAX_SIZE             10
#define RFID_FILE                 "/rfid_tags.bin"

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

// Task scheduler - Flags
struct StructTaskPending
{
  bool ButtonInside = 0;
  bool ButtonOutside = 0;
  bool LedG = 0;
  bool LedR = 0;
  bool RelayDoor = 0;
  bool RelayDoorbell = 0;
  bool Buzzer = 0;
  bool APServer = 0;
} TaskPending;

// Task scheduler - Call times (ms)
struct StructTaskCallTime
{
  unsigned long Current = 0;
  unsigned long LedG = 0;
  unsigned long LedR = 0;
  unsigned long RelayDoor = 0;
  unsigned long RelayDoorbell = 0;
  unsigned long Buzzer = 0;
  unsigned long APServer = 0;
} TaskCallTime;

// Task scheduler - Duration (ms)
struct StructTaskDuration
{
  short LedG = 0;
  short LedR = 0;
  short RelayDoor = 1000;
  short RelayDoorbell = 1000;
  short Buzzer = 0;
} TaskDuration;

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
    Serial.println("Failed to mount LittleFS");
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

  if (!mfrc522.PICC_IsNewCardPresent()) return;

  if (!mfrc522.PICC_ReadCardSerial()) return;

  currTag = mfrc522.uid.uidByte;
  size = mfrc522.uid.size;

  if (checkEqualCard(currTag, size, masterAdd, masterSize)) {
    add_tag_ordered(currTag, size);
  };
  if (checkEqualCard(currTag, size, masterRemove, masterSize)) {
    denyTone();
  };

  printID(currTag, size);

}






//---Utility Funtions----------------------------------------------------------------------------//

void printID(byte *tag, byte size) {// Function to read and print the RFID card's UID.
  Serial.print(F("Card UID:"));
  dump_byte_array(tag, size);
  Serial.println();
  Serial.print(F("PICC type: "));
  MFRC522::PICC_Type piccType = mfrc522.PICC_GetType(mfrc522.uid.sak);
  Serial.println(mfrc522.PICC_GetTypeName(piccType));
  mfrc522.PICC_HaltA();
}

bool checkEqualCard(byte* tag, byte& size1, byte* cardAgainst, byte& size2) {
  if (size1 != size2) return false;
  
  card_equal = true;
  for (byte i = 0; i < size1; i++) {
    if (tag[i] != cardAgainst[i]) {
      card_equal = false;
      break;
    }
  }

  return card_equal;
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

// imprime o rfid para o serial
void dump_byte_array(byte *buffer, byte bufferSize) {
    for (byte i = 0; i < bufferSize; i++) {
        Serial.print(buffer[i] < 0x10 ? " 0" : " ");
        Serial.print(buffer[i], HEX);
    }
}

bool add_tag_ordered(byte *tag, byte size) {
  if (size > RFID_TAG_MAX_SIZE) {
  Serial.println("Tag Inválida: formato não suportado");
  return false; 
  }

  // Abertura ou criação do arquivo que guarda as tags
  File file = LittleFS.open(RFID_FILE, "r+");
  if(!file) {
    file = LittleFS.open(RFID_FILE, "w+");
    if (!file) {
      Serial.println("Erro ao criar arquivo para guardar tag");
      return false;
    }
  }

  byte tagIter[RFID_TAG_MAX_SIZE];
  size_t position = 0;
  bool tagExists = false;

  // loop para iterar as tags do arquivo e comparar a tag a ser inserida
  while(file.readBytes((char *)tagIter, RFID_TAG_MAX_SIZE) == RFID_TAG_MAX_SIZE) {
    int comparison = memcmp(tagIter, tag, size);
    if (comparison == 0) {
      tagExists = true;
      break; // tag já existente no arquivo
    } 
    else if (comparison > 0) {
      break; // Achou a posição para inserir a tag
    } 
    else {
      position += RFID_TAG_MAX_SIZE; // Pula para a próxima tag no arquivo
    }  
  }

  // rejeita inserção de tag já existente
  if (tagExists) {
    Serial.println("Tag já existe");
    file.close();
    return false;
  }

  File tempFile = LittleFS.open("/temp.bin", "w+");
  if (!tempFile) {
    Serial.println("Erro ao criar arquivo temporário de inserção de tags");
    file.close();
    return false;
  }

  // Volta ao começo do arquivo
  file.seek(0, fs::SeekSet);

  // Copia todas as tags anteriores à posição de inserção no arquivo temporário
  while(position > 0 && file.readBytes((char *)tagIter, RFID_TAG_MAX_SIZE) == RFID_TAG_MAX_SIZE){
    tempFile.write(tagIter, RFID_TAG_MAX_SIZE);
    position -= RFID_TAG_MAX_SIZE;
  }

  // insere a tag nova
  tempFile.write(tag, RFID_TAG_MAX_SIZE);

  // copia o resto do arquivo
  while (file.readBytes((char *)tagIter, RFID_TAG_MAX_SIZE) == RFID_TAG_MAX_SIZE) {
    tempFile.write(tagIter, RFID_TAG_MAX_SIZE);
  }

  // fecha o arquivo atual
  file.close();
  tempFile.close();

  // exclui o arquivo antigo e salva o novo
  LittleFS.remove(RFID_FILE);
  LittleFS.rename("/temp.bin", RFID_FILE);

  return true;
}

int search_tag(byte *tag, byte size) {
  if (size > RFID_TAG_MAX_SIZE) {
    Serial.println("Tag Inválida: formato não suportado");
    return -1; 
  }

  File file = LittleFS.open(RFID_FILE, "r");
  if (!file) {
    Serial.println("Arquivo das tags não encontrado");
    return -1; // Arquivo não existe
  }

  byte tagIter[RFID_TAG_MAX_SIZE];
  int position = 0;

  while (file.readBytes((char *)tagIter, RFID_TAG_MAX_SIZE) == RFID_TAG_MAX_SIZE) {
    if (memcmp(tagIter, tag, size) == 0) {
      file.close();
      return position; // Retorna a posição no arquivo onde se encontra a tag
    }
    position++;
  }

  file.close();
  return -1; // Tag não encontrada no arquivo
}


