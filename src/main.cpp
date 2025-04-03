// Importações de constantes
#include "Secrets.h"
#include "pins.h"
#include "control.h"

// Importações das libs do projeto
#include "FileSys.h"
#include "Tags.h"
#include "Wifi.h"
#include "Feedback.h"

bool DEBUG_MODE = true;

void aceitarEntrada();
void negarEntrada();

void setup() {
  Serial.begin(9600);
  SPI.begin();
  mfrc522.PCD_Init();

  WifiInit();

  FileSysInit();

  setDateTime();

  CertStoreInit();

  pinMode(PIN_BUZZER, OUTPUT);
  digitalWrite(PIN_BUZZER, LOW);

  pinMode(PIN_LED_G, OUTPUT);
  pinMode(PIN_LED_R, OUTPUT);

  digitalWrite(PIN_LED_G, LOW);
  digitalWrite(PIN_LED_R, LOW);
}

void loop() {

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



// --- Funções de processo ---------//
void aceitarEntrada() {
  ledG.pattern(1, false);
  buzzer.pattern(1, false);
}

void negarEntrada() {
  ledR.pattern(3, SPEED_RAPID, false);
  buzzer.pattern(3, SPEED_RAPID, false);
}
