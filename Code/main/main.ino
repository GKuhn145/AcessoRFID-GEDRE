#include <ESP8266WiFi.h>
#include <MFRC522.h>
#include <SPI.h>
#include <LittleFS.h>
#include <Blinkenlight.h>
#include <base64.h>
#include "Secrets.h"

// Bibliotecas relacionadas ao MQTT
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <CertStoreBearSSL.h>
#include <TZ.h>

// Parâmetros do Wifi
#define WIFI_TIMEOUT_MS 10000

PubSubClient *client;
BearSSL::CertStore certStore;

// Parâmetros do MQTT
#define MQTT_PORT 8883
#define MQTT_TOPIC "esp/tags/update"

// Tag related parameters 
#define RFID_TAG_SIZE 4
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
bool DEBUG_MODE = true;
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

  wifi_init();

  FS_init();

  setDateTime();

  setCertStore();

  client->setServer(MQTT_SERVER, MQTT_PORT);

  pinMode(PIN_BUZZER, OUTPUT);
  digitalWrite(PIN_BUZZER, LOW);

  pinMode(PIN_LED_G, OUTPUT);
  pinMode(PIN_LED_R, OUTPUT);

  digitalWrite(PIN_LED_G, LOW);
  digitalWrite(PIN_LED_R, LOW);
}

void loop() {
  // updates relacionados ao MQTT
  if (!client->connected()) {
    reconnect();
  }
  client->loop();

  // updates dos leds e buzzers
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
            if (DEBUG_MODE) Serial.println("Modo de CADASTRAMENTO ativado. Aguardando nova Tag...");
            mode = 1;
            waitStart = currentMillis;
            break;

          case MASTER_DESCAD:
            if (DEBUG_MODE) Serial.println("Modo de DESCADASTRAMENTO ativado. Aguardando Tag para remover...");
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
            if (DEBUG_MODE) Serial.println("Nova tag cadastrada");
            ledG.pattern(1, false);
            buzzer.pattern(1, false);
            sendTagsToMQTT();
            mode = 0;
          } else {
            if (DEBUG_MODE) Serial.println("Tag já cadastrada");
            ledR.pattern(3, SPEED_RAPID, false);
            buzzer.pattern(3, SPEED_RAPID, false);
          }
        }
      } else {
        if (DEBUG_MODE) Serial.println("Tempo expirado para cadastramento");
        ledG.off();
        mode = 0;
      }
      break;

    case 2:  // modo de descadastramento, lê continuamente um cartão para cadastrar
      if (currentMillis - waitStart < TAG_DELAY_MS) {
        ledR.blink(SPEED_RAPID);
        if (readRFID(uid)) {
          if (removeRFID(uid)) {
            if (DEBUG_MODE) Serial.println("Tag removida");
            ledR.pattern(1, false);
            buzzer.pattern(1, false);
            sendTagsToMQTT();
            mode = 0;
          } else {
            if (DEBUG_MODE) Serial.println("Tag não encontrada");
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

//---Funções de inicialização---//

void FS_init() {
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
}

void wifi_init() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  unsigned long startAttemptTime = millis();

  Serial.println("Conectando ao WiFi");

  while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < WIFI_TIMEOUT_MS) {
    delay(500);
    Serial.println(".");
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi conectado");
    Serial.print("Endereço IP: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("\nFalha ao conectar o WiFi");
  }
}

void setDateTime() {
  configTime(PSTR("<-03>3"), "pool.ntp.org", "time.nist.gov");

  if (DEBUG_MODE) Serial.print("Waiting for NTP time sync: ");
  time_t now = time(nullptr);
  while (now < 8 * 3600 * 2) {
    delay(100);
    if (DEBUG_MODE) Serial.print(".");
    now = time(nullptr);
  }

  struct tm timeinfo;
  gmtime_r(&now, &timeinfo);
  if (DEBUG_MODE) Serial.printf("%s %s", tzname[0], asctime(&timeinfo));
}

void setCertStore() {
  int numCerts = certStore.initCertStore(LittleFS, PSTR("/certs.idx"), PSTR("/certs.ar"));
  if (DEBUG_MODE) Serial.printf("Certificados CA encontrados: %d \n", numCerts);
  if (numCerts == 0) {
    Serial.println("Nenhum certificado encontrado");
    return;
  }

  if (DEBUG_MODE) Serial.println("Criando cliente MQTT");
  BearSSL::WiFiClientSecure *bear = new BearSSL::WiFiClientSecure();
  bear->setCertStore(&certStore);
  client = new PubSubClient(*bear);
  if (DEBUG_MODE) Serial.println("Cliente MQTT criado");
}

//---Funções de envio MQTT---//

void reconnect() {
  static unsigned long lastAttempt = 0;
  static int attempt = 0;
  const int maxAttempts = 5;
  const unsigned long retryInterval = 2000;
  const unsigned long longWait = 10000;

  if (client->connected()) return;

  if (DEBUG_MODE) Serial.println("Conectando ao broker MQTT");

  unsigned long currMillis = millis();

  if (currMillis - lastAttempt >= retryInterval) {
    lastAttempt = currMillis;

    if (DEBUG_MODE) {
      Serial.print("Tentativa ");
      Serial.println(attempt + 1);
    }

    if (client->connect("ESP8266_Client", MQTT_USER, MQTT_PASS)) {
      if (DEBUG_MODE) {
        Serial.println("Conectado ao MQTT!");
        client->subscribe("esp/tags");
        client->subscribe("esp/config");
      }
      attempt = 0;
    } else {
      if (DEBUG_MODE) {
        Serial.print("Falha na conexão, código: ");
        Serial.println(client->state());
      }
      attempt++;

      if (attempt >= maxAttempts) {
        if (DEBUG_MODE) Serial.println("Falha ao conectar ao MQTT, aguardando 10s antes de tentar novamente...");
        lastAttempt = millis() + longWait - retryInterval;
        attempt = 0;
      }
    }
  }
}

void sendTagsToMQTT() {
  if (!LittleFS.exists(RFID_TAGS_FILE)) {
    if (DEBUG_MODE) Serial.println("Arquivo de tags não encontrado");
    return;
  }

  File file = LittleFS.open(RFID_TAGS_FILE, "r");
  if (!file) {
    if (DEBUG_MODE) Serial.println("Falha ao abrir o arquivo de tags");
    return;
  }

  size_t fileSize = file.size();
  uint8_t buffer[fileSize];
  file.read(buffer, fileSize);
  file.close();

  String encodedData = base64::encode(buffer, fileSize);

  StaticJsonDocument<1024> doc;
  doc["device"] = "ESP8266 Porta";
  doc["filename"] = "tags.bin";
  doc["data"] = encodedData;

  char jsonBuffer[1024];
  serializeJson(doc, jsonBuffer);
  client->publish(MQTT_TOPIC, jsonBuffer);
  if (DEBUG_MODE) Serial.println("Arquivo enviado via MQTT");
}


//---Funções de utilidades---//

// Realiza a leitura de um RFID para um buffer
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

// Checa se a tag existe
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

// Checa se a tag é mestre
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

// --- Funções de processo ---------//
void aceitarEntrada() {
  ledG.pattern(1, false);
  buzzer.pattern(1, false);
}

void negarEntrada() {
  ledR.pattern(3, SPEED_RAPID, false);
  buzzer.pattern(3, SPEED_RAPID, false);
}
