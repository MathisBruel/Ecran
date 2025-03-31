#include <DIYables_Keypad.h>
#include <LiquidCrystal.h>
#include <SPI.h>
#include <MFRC522.h>
#include <Ethernet.h>

// Constants
const int ROW_NUM = 4;
const int COLUMN_NUM = 4;
const String PASSWORD = "1234A"; // Change your password here
const int BACKLIGHT_BRIGHTNESS = 95;
const unsigned long time_cool_down = 7000; // Cooldown en ms
const int LED_PIN_DETECTED = 9;
const bool SON_ALARME = false;

const IPAddress TELEGRAM_IP(149, 154, 167, 220);  // IP de api.telegram.org
const String TELEGRAM_BOT_TOKEN = "7839952728:AAGSe8eMUBPqnWYdkqsomjvzywTnrfVgagc";
const String TELEGRAM_CHAT_ID = "-4625122029";

// LED Pins
const int LED_PIN_ALARME_ON = 6;
const int LED_PIN_ALARME_OFF = 7;

// Variables pour la sirène
unsigned long lastToneChange = 0;
const unsigned long toneInterval = 5;
int currentFreq = 2000;

int duree = 500;
int freqmin = 1500;
int freqmax = 5000;

// Variables d'état
unsigned long timeDetected = 0;
bool move_detected = false;
bool alarm = false;
bool keypad_active = true;
String input_password = "";

// Variables de timing
unsigned long lastBlinkTime = 0;
unsigned long lastBlinkAlarmTime = 0;
unsigned long lastMessageCheck = 0;
unsigned long lastCommandCheck = 0;
unsigned long lastLCDUpdate = 0;
unsigned long pir_last_active = 0;
unsigned long rfid_last_active = 0;

// Constantes de timing
const unsigned long BLINK_INTERVAL = 300;
const unsigned long BLINK_ALARM_INTERVAL = 300;
const unsigned long MESSAGE_CHECK_INTERVAL = 5000;
const unsigned long COMMAND_CHECK_INTERVAL = 2000;
const unsigned long LCD_UPDATE_INTERVAL = 100;
const unsigned long KEYPAD_DEBOUNCE = 10;
const unsigned long RFID_CHECK_INTERVAL = 20;

// Pins
const int PIR_PIN = 22;
const int LED_PIN = 13;
const int BACKLIGHT_PIN = 8;
const int BUZZER_PIN = 12;
const int RS = 29, EN = 30, D4 = 26, D5 = 27, D6 = 24, D7 = 25;
const int SS_PIN = 53, RST_PIN = 49; // RFID pins

// Keypad setup
char keys[ROW_NUM][COLUMN_NUM] = {
    {'1', '2', '3', 'A'},
    {'4', '5', '6', 'B'},
    {'7', '8', '9', 'C'},
    {'*', '0', '#', 'D'}};
byte pin_rows[ROW_NUM] = {14, 15, 16, 17};
byte pin_columns[COLUMN_NUM] = {18, 19, 20, 21};
DIYables_Keypad keypad = DIYables_Keypad(makeKeymap(keys), pin_rows, pin_columns, ROW_NUM, COLUMN_NUM);

// Ethernet setup
byte mac[] = {0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED};
byte ip[] = {192, 168, 1, 202};
byte dns[] = {8, 8, 8, 8};
byte gateway[] = {192, 168, 1, 1};
byte subnet[] = {255, 255, 255, 0};

EthernetClient client;

// LCD setup
LiquidCrystal lcd(RS, EN, D4, D5, D6, D7);

// RFID setup
MFRC522 mfrc522(SS_PIN, RST_PIN);

// Variables
String authorizedUIDs[] = {"67C3A060"}; // Liste des badges autorisés

// Variables pour le clignotement des LEDs
bool ledState = false;

// Variables pour la vérification des commandes
long lastMessageId = 0;

// Function declarations
void initializeLCD();
void handlePIR();
void updateBacklight();
void processKey(char key);
void setAlarmState(bool state, const String &method);
void displayMessage(const String &message);
void handleRFID();
void handleAlarme();
void sendTelegramMessage(const String &message);

void setup()
{
  Serial.begin(9600);
  Serial.println("Initialisation du système...");

  SPI.begin();
  Serial.println("SPI initialisé");

  Serial.println("Tentative de connexion Ethernet...");
  Ethernet.begin(mac, ip, dns, gateway, subnet);
  delay(1000);

  if (Ethernet.hardwareStatus() == EthernetNoHardware) {
    Serial.println("Ethernet shield was not found.");
    while (true) {
      delay(1);
    }
  }
  if (Ethernet.linkStatus() == LinkOFF) {
    Serial.println("Ethernet cable is not connected.");
  }

  Serial.print("IP Address: ");
  Serial.println(Ethernet.localIP());

  mfrc522.PCD_Init();
  Serial.println("RFID initialisé");

  pinMode(PIR_PIN, INPUT);
  pinMode(LED_PIN, OUTPUT);
  pinMode(BACKLIGHT_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  Serial.println("Pins configurés");

  initializeLCD();
  Serial.println("LCD initialisé");

  Serial.println("Initialisation terminée");

  sendTelegramMessage("L'alarme est mise sous tension");
}

void loop()
{
  unsigned long currentMillis = millis();

  // Lecture du clavier en priorité et sans délai
  char key = keypad.getKey();
  if (key) {
    processKey(key);
  }

  // Lecture RFID en priorité et avec intervalle minimal
  handleRFID();

  // Gestion des autres fonctionnalités
  handlePIR();
  handleAlarme();
}

// Initialize the LCD and display initial state
void initializeLCD()
{
  lcd.begin(16, 2);
  lcd.setCursor(0, 1);
  lcd.print(alarm ? "Alarme ON " : "Alarme OFF");

  if(alarm) {
    analogWrite(LED_PIN_ALARME_ON, 255);
    analogWrite(LED_PIN_ALARME_OFF, 0);
  } else {
    analogWrite(LED_PIN_ALARME_ON, 0);
    analogWrite(LED_PIN_ALARME_OFF, 255);
    analogWrite(LED_PIN_DETECTED, 0);
    digitalWrite(LED_PIN, LOW);
  }
}

// Handle PIR sensor activity
void handlePIR()
{
  unsigned long currentMillis = millis();

  if (alarm) {
    if (!move_detected && digitalRead(PIR_PIN) == HIGH) {
      if (currentMillis - pir_last_active > 100) {
        move_detected = true;
        timeDetected = currentMillis;
        pir_last_active = currentMillis;
      }
    }
  } else {
    move_detected = false;
    analogWrite(LED_PIN_DETECTED, 0);
  }
}

// Update backlight brightness
void updateBacklight()
{
  analogWrite(BACKLIGHT_PIN, BACKLIGHT_BRIGHTNESS);
}

// Process keypad input
void processKey(char key)
{
  // Suppression du délai de rafraîchissement LCD initial
  if (input_password.length() == 0) {
    lcd.clear();
    lcd.setCursor(0, 1);
    lcd.print(alarm ? "Alarme ON " : "Alarme OFF");
  }

  // Traitement immédiat de la touche
  switch(key) {
    case '*':
      input_password = "";
      keypad_active = true;
      lcd.clear();
      lcd.setCursor(0, 1);
      lcd.print(alarm ? "Alarme ON " : "Alarme OFF");
      break;

    case '#':
      if (input_password == PASSWORD) {
        alarm = !alarm;
        setAlarmState(alarm, "clavier");
        sendTelegramMessage("Alarme " + String(alarm ? "activée" : "désactivée") + " via le clavier");
        input_password = "";
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("Correct");
      } else {
        lcd.clear();
        lcd.setCursor(0, 1);
        lcd.print(alarm ? "Alarme ON " : "Alarme OFF");
        lcd.setCursor(0, 0);
        lcd.print("Incorrect");
        input_password = "";
      }
      keypad_active = true;
      break;

    default:
      if (keypad_active && input_password.length() < 5) {  // Limite la longueur du mot de passe
        input_password += key;
        lcd.setCursor(input_password.length() - 1, 0);
        lcd.print(key);
      }
      break;
  }
}

// Handle RFID badge detection
void handleRFID()
{
  unsigned long currentMillis = millis();

  // Réduit l'intervalle anti-rebond à 20ms
  if (currentMillis - rfid_last_active < RFID_CHECK_INTERVAL) return;

  if (mfrc522.PICC_IsNewCardPresent()) {  // Premier check sans bloquer
    if (mfrc522.PICC_ReadCardSerial()) {  // Second check uniquement si nécessaire
      String uid = "";
      for (byte i = 0; i < mfrc522.uid.size; i++) {
        uid += String(mfrc522.uid.uidByte[i] < 0x10 ? "0" : "") + String(mfrc522.uid.uidByte[i], HEX);
      }
      uid.toUpperCase();
      rfid_last_active = currentMillis;

      // Optimisation de la recherche de badge
      bool badgeValid = false;
      for (String authorizedUID : authorizedUIDs) {
        if (uid == authorizedUID) {
          badgeValid = true;
          alarm = !alarm;
          setAlarmState(alarm, "badge RFID");
          sendTelegramMessage("Alarme " + String(alarm ? "activée" : "désactivée") + " via badge RFID");
          break;
        }
      }

      if (!badgeValid) {
        // Handle invalid badge if needed
      }

      mfrc522.PICC_HaltA();
      mfrc522.PCD_StopCrypto1();  // Ajout: arrête la communication crypto
    }
  }
}

// Gérer l'alarme et le mouvement PIR
void handleAlarme()
{
  unsigned long currentMillis = millis();

  if (alarm && move_detected) {
    if (currentMillis - timeDetected > time_cool_down) {
      // Gestion optimisée du clignotement LED
      if (currentMillis - lastBlinkTime >= BLINK_INTERVAL) {
        lastBlinkTime = currentMillis;
        digitalWrite(LED_PIN, !digitalRead(LED_PIN));
      }

      // Gestion optimisée de la sirène
      if (SON_ALARME) {
        // Ajustement de la fréquence pour un son plus fluide
        if (currentMillis - lastToneChange >= toneInterval) {
          lastToneChange = currentMillis;
          currentFreq = (currentFreq >= freqmax) ? freqmin : currentFreq + 50; // Changement de 100 à 50 pour un son plus fluide
          tone(BUZZER_PIN, currentFreq, duree);
        }
      }
    } else {
      // Gestion du cooldown
      if (currentMillis - lastBlinkTime >= BLINK_INTERVAL) {
        lastBlinkTime = currentMillis;
        analogWrite(LED_PIN_DETECTED, digitalRead(LED_PIN_DETECTED) ? 0 : 255);
      }
    }
  } else {
    digitalWrite(LED_PIN, LOW);
    noTone(BUZZER_PIN);
    if (!alarm) {
      move_detected = false;
      analogWrite(LED_PIN_DETECTED, 0);
    }
  }
}

// Set the alarm state and update the LCD
void setAlarmState(bool state, const String &method)
{
    lcd.clear();
    lcd.setCursor(0, 1);
    lcd.print(state ? "Alarme ON " : "Alarme OFF");

    if(state) {
        analogWrite(LED_PIN_ALARME_ON, 255);
        analogWrite(LED_PIN_ALARME_OFF, 0);
    } else {
        analogWrite(LED_PIN_ALARME_ON, 0);
        analogWrite(LED_PIN_ALARME_OFF, 255);
        analogWrite(LED_PIN_DETECTED, 0);
        digitalWrite(LED_PIN, LOW);
    }
}

// Display a message on the LCD
void displayMessage(const String &message)
{
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(message);
}

void displayMessageNoClear(const String &message)
{
  lcd.setCursor(0, 0);
  lcd.print(message);
}

// Send a message to Telegram
void sendTelegramMessage(const String &message) {
    EthernetClient client;
    
    if (client.connect(TELEGRAM_IP, 80)) {
        Serial.println("Connecté à Telegram");
        
        // Construire l'URL avec le message
        String url = String("/bot") + TELEGRAM_BOT_TOKEN + 
                    "/sendMessage?chat_id=" + TELEGRAM_CHAT_ID + 
                    "&text=" + message;
        
        // Envoyer la requête GET
        client.println("GET " + url + " HTTP/1.1");
        client.println("Host: api.telegram.org"); // Garder le nom d'hôte original
        client.println("Accept: */*");
        client.println("Connection: close");
        client.println();
        
        // Attendre et afficher la réponse
        unsigned long timeout = millis();
        while (client.connected() && millis() - timeout < 10000) {
            if (client.available()) {
                String line = client.readStringUntil('\n');
                Serial.println(line);
            }
        }
        
        client.stop();
        Serial.println("Message envoyé");
    } else {
        Serial.println("Échec de connexion à Telegram");
    }
}
