#include <DIYables_Keypad.h> // DIYables_Keypad library
#include <LiquidCrystal.h>   // LiquidCrystal library
#include <SPI.h>
#include <Ethernet.h>
#include <MFRC522.h> // RFID library

// Constants
const int ROW_NUM = 4;
const int COLUMN_NUM = 4;
const String PASSWORD = "1234A"; // Change your password here
const int BACKLIGHT_BRIGHTNESS = 95;
const unsigned long time_cool_down = 7000; // Cooldown en ms
const int LED_PIN_DETECTED = 12;
const bool SON_ALARME = false;

// Configuration Telegram via Proxy
const char* PROXY_HOST = "192.168.1.104";
const int PROXY_PORT = 8080;

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
unsigned long lastLCDUpdate = 0;
unsigned long pir_last_active = 0;
unsigned long rfid_last_active = 0;

// Constantes de timing
const unsigned long BLINK_INTERVAL = 300;
const unsigned long BLINK_ALARM_INTERVAL = 300;
const unsigned long LCD_UPDATE_INTERVAL = 100;
const unsigned long KEYPAD_DEBOUNCE = 10;
const unsigned long RFID_CHECK_INTERVAL = 20;

// Pins
const int PIR_PIN = 22;
const int LED_PIN = 13;
const int BACKLIGHT_PIN = 8;
const int BUZZER_PIN = 5;
const int RS = 29, EN = 30, D4 = 26, D5 = 27, D6 = 24, D7 = 25;
const int SS_PIN = 53, RST_PIN = 49; // RFID pins

// Keypad setup
char keys[ROW_NUM][COLUMN_NUM] = {
    {'1', '2', '3', 'A'},
    {'4', '5', '6', 'B'},
    {'7', '8', '9', 'C'},
    {'*', '0', '#', 'D'}};
byte pin_rows[ROW_NUM] = {14, 15, 16, 17};
byte pin_columns[COLUMN_NUM] = {18, 19, 20, 21}; // Correction de "pin_column"
DIYables_Keypad keypad = DIYables_Keypad(makeKeymap(keys), pin_rows, pin_columns, ROW_NUM, COLUMN_NUM);

// Ethernet setup
byte mac[] = {0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED};
byte ip[] = {192, 168, 1, 202};  // IP dans la plage du partage de connexion Windows
byte gateway[] = {192, 168, 1, 1};  // Passerelle = IP de Windows en partage de connexion
byte subnet[] = {255, 255, 255, 0};  // Masque de sous-réseau
byte dns1[] = {8, 8, 8, 8};  // DNS Google primaire
byte dns2[] = {8, 8, 4, 4};  // DNS Google secondaire
EthernetServer server(80);

// LCD setup
LiquidCrystal lcd(RS, EN, D4, D5, D6, D7);

// RFID setup
MFRC522 mfrc522(SS_PIN, RST_PIN);

// Variables
String authorizedUIDs[] = {"67C3A060"}; // Liste des badges autorisés

// Variables pour le clignotement des LEDs
bool ledState = false;

// Function declarations
void initializeLCD();
void handlePIR();
void updateBacklight();
void processKey(char key);
void handleEthernetClient();
void setAlarmState(bool state, const String &method = "");
void displayMessage(const String &message);
void handleRFID();
void handleAlarme();
void sendTelegramMessage(const String& message);
void checkHardware();

void setup()
{
  Serial.begin(9600);
  Serial.println("Initialisation du système...");
  
  // Vérification du matériel après l'initialisation de la communication série
  //checkHardware();
  
  SPI.begin();
  Serial.println("SPI initialisé");
  
  Serial.println("Tentative de connexion Ethernet...");
  Ethernet.begin(mac, ip, dns1, gateway, subnet);
  
  // Vérifier si l'IP a été assignée
  if (Ethernet.localIP()[0] != 0) {
    Serial.println("Connexion Ethernet réussie");
    Serial.print("Adresse IP: ");
    for (byte thisByte = 0; thisByte < 4; thisByte++) {
      Serial.print(Ethernet.localIP()[thisByte]);
      if (thisByte < 3) Serial.print(".");
    }
    Serial.println();
    
    // Test de connexion au proxy
    Serial.println("Test de connexion au proxy...");
    EthernetClient client;
    if (client.connect(PROXY_HOST, PROXY_PORT)) {
      Serial.println("Test de connexion au proxy réussi");
      client.stop();
    } else {
      Serial.println("Échec de la connexion au proxy");
    }
  } else {
    Serial.println("Échec de la connexion Ethernet - Pas d'IP assignée");
  }
  
  server.begin();
  Serial.println("Serveur web démarré");
  
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
  handleEthernetClient();
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
      if (currentMillis - pir_last_active > 100) { // Anti-rebond pour le PIR
        move_detected = true;
        timeDetected = currentMillis;
        pir_last_active = currentMillis;
        sendTelegramMessage("⚠️ Mouvement détecté ! L'alarme se déclenchera dans 7 secondes.");
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
  // Envoyer dans la console la touche pressée
  Serial.print("Touche pressée: ");
  Serial.println(key);
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
        sendTelegramMessage("⚠️ Tentative d'accès refusée - Code incorrect");
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
          break;
        }
      }

      if (!badgeValid) {
        sendTelegramMessage("⚠️ Tentative d'accès refusée - Badge RFID non autorisé");
      }

      mfrc522.PICC_HaltA();
      mfrc522.PCD_StopCrypto1();  // Ajout: arrête la communication crypto
    }
  }
}

// Handle incoming Ethernet clients
void handleEthernetClient()
{
  EthernetClient client = server.available();
  if (client)
  {
    unsigned long currentMillis = millis();
    String request = "";
    while (client.connected() && client.available())
    {
      char c = client.read();
      request += c;
      if (c == '\n' && request.endsWith("\r\n\r\n"))
        break;
    }

    if (request.indexOf("GET /?alarm=on") != -1)
    {
      alarm = true;
      setAlarmState(true, "interface web");
    }
    else if (request.indexOf("GET /?alarm=off") != -1)
    {
      alarm = false;
      setAlarmState(false, "interface web");
    }
    else if (request.indexOf("GET /telegram_command") != -1)
    {
      // Endpoint pour recevoir les commandes Telegram
      String command = "";
      int start = request.indexOf("command=") + 8;
      int end = request.indexOf(" ", start);
      if (start > 8 && end > start) {
        command = request.substring(start, end);
        
        if (command == "ON") {
          if (!alarm) {
            alarm = true;
            setAlarmState(true, "commande Telegram");
          }
        } else if (command == "OFF") {
          if (alarm) {
            alarm = false;
            setAlarmState(false, "commande Telegram");
          }
        }
      }
      
      client.println("HTTP/1.1 200 OK");
      client.println("Content-Type: text/plain");
      client.println();
      client.println("OK");
      client.stop();
      return;
    }
    else if (request.indexOf("GET /status") != -1)
    {
      // Endpoint pour le proxy pour vérifier l'état
      client.println("HTTP/1.1 200 OK");
      client.println("Content-Type: application/json");
      client.println();
      client.print("{\"alarm\":");
      client.print(alarm ? "true" : "false");
      client.print(",\"movement\":");
      client.print(move_detected ? "true" : "false");
      client.println("}");
      client.stop();
      return;
    }
    else if (request.indexOf("POST /update") != -1)
    {
      // Endpoint pour recevoir les mises à jour du proxy
      String body = "";
      while(client.available()) {
        body += (char)client.read();
      }
      
      if(body.indexOf("\"message\":") != -1) {
        // Extraire le message entre guillemets après "message":
        int start = body.indexOf("\"message\":") + 10;
        int end = body.indexOf("\"", start);
        if(start > 0 && end > start) {
          String message = body.substring(start, end);
          displayMessage(message);
        }
      }

      client.println("HTTP/1.1 200 OK");
      client.println();
      client.stop();
      return;
    }    
    if (request.indexOf("GET /?detected=true") != -1)
    {
      alarm = true;
      if(alarm) {
        move_detected = true;
        timeDetected = currentMillis;
        pir_last_active = currentMillis;
        sendTelegramMessage("⚠️ Mouvement détecté sur un module détaché! L'alarme se déclenchera dans 7 secondes.");
      }
    }
    else if (request.indexOf("GET /?detected=false") != -1)
    {
      move_detected = false;
      analogWrite(LED_PIN_DETECTED, 0);
    }

    client.println("HTTP/1.1 200 OK");
    client.println("Content-Type: text/html");
    client.println();
    client.println("<html><body>");
    client.println("<h1>Alarme</h1>");
    client.print("<p>Etat actuel : <strong>");
    client.print(alarm ? "ON" : "OFF");
    client.println("</strong></p>");
    client.println("<form method='get'>");
    client.println("<button type='submit' name='alarm' value='on'>Activer</button>");
    client.println("<button type='submit' name='alarm' value='off'>Desactiver</button>");
    client.println("</form>");
    client.println("</body></html>");

    client.stop();
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
void setAlarmState(bool state, const String &method = "")
{
  lcd.clear();
  lcd.setCursor(0, 1);
  lcd.print(state ? "Alarme ON " : "Alarme OFF");
  
  if(state) {
    analogWrite(LED_PIN_ALARME_ON, 255);
    analogWrite(LED_PIN_ALARME_OFF, 0);
    if(method != "") {
      sendTelegramMessage("🔒 Alarme armée via " + method);
    }
  } else {
    analogWrite(LED_PIN_ALARME_ON, 0);
    analogWrite(LED_PIN_ALARME_OFF, 255);
    analogWrite(LED_PIN_DETECTED, 0);
    digitalWrite(LED_PIN, LOW);
    if(method != "") {
      sendTelegramMessage("🔓 Alarme désarmée via " + method);
    }
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

// Fonction pour envoyer un message Telegram via le proxy
void sendTelegramMessage(const String& message) {
  Serial.println("\nTentative d'envoi de message Telegram via proxy:");
  Serial.print("Message: ");
  Serial.println(message);
  
  EthernetClient client;
  Serial.println("Tentative de connexion au proxy...");
  
  if (client.connect(PROXY_HOST, PROXY_PORT)) {
    Serial.println("Connexion au proxy réussie");
    
    // Encodage URL du message pour gérer les caractères spéciaux
    String encodedMessage = message;
    // Caractères spéciaux
    encodedMessage.replace(" ", "%20");
    encodedMessage.replace("!", "%21");
    // Emojis
    encodedMessage.replace("⚠️", "%E2%9A%A0%EF%B8%8F");
    encodedMessage.replace("🔒", "%F0%9F%94%92");
    encodedMessage.replace("🔓", "%F0%9F%94%93");
    encodedMessage.replace("🔔", "%F0%9F%94%94");
    // Caractères accentués
    encodedMessage.replace("é", "%C3%A9");
    encodedMessage.replace("è", "%C3%A8");
    encodedMessage.replace("ê", "%C3%AA");
    encodedMessage.replace("ë", "%C3%AB");
    encodedMessage.replace("à", "%C3%A0");
    encodedMessage.replace("â", "%C3%A2");
    encodedMessage.replace("ä", "%C3%A4");
    encodedMessage.replace("î", "%C3%AE");
    encodedMessage.replace("ï", "%C3%AF");
    encodedMessage.replace("ô", "%C3%B4");
    encodedMessage.replace("ö", "%C3%B6");
    encodedMessage.replace("ù", "%C3%B9");
    encodedMessage.replace("û", "%C3%BB");
    encodedMessage.replace("ü", "%C3%BC");
    encodedMessage.replace("ç", "%C3%A7");
    // Majuscules accentuées
    encodedMessage.replace("É", "%C3%89");
    encodedMessage.replace("È", "%C3%88");
    encodedMessage.replace("Ê", "%C3%8A");
    encodedMessage.replace("Ë", "%C3%8B");
    encodedMessage.replace("À", "%C3%80");
    encodedMessage.replace("Â", "%C3%82");
    encodedMessage.replace("Ä", "%C3%84");
    encodedMessage.replace("Î", "%C3%8E");
    encodedMessage.replace("Ï", "%C3%8F");
    encodedMessage.replace("Ô", "%C3%94");
    encodedMessage.replace("Ö", "%C3%96");
    encodedMessage.replace("Ù", "%C3%99");
    encodedMessage.replace("Û", "%C3%9B");
    encodedMessage.replace("Ü", "%C3%9C");
    encodedMessage.replace("Ç", "%C3%87");
    
    // Envoi au format que le proxy pourra comprendre
    String url = "/send?message=" + encodedMessage;
    Serial.println("Envoi de la requête au proxy...");
    Serial.println("URL: " + url);
    
    client.print("GET ");
    client.print(url);
    client.println(" HTTP/1.1");
    client.print("Host: ");
    client.println(PROXY_HOST);
    client.println("Connection: close");
    client.println();
    
    Serial.println("Attente de la réponse...");
    unsigned long timeout = millis();
    bool responseReceived = false;
    bool headersDone = false;
    String response = "";
    
    while (client.connected() && (millis() - timeout < 5000)) {
      while (client.available()) {
        String line = client.readStringUntil('\n');
        
        if (!headersDone) {
          if (line == "\r") {
            headersDone = true;
          }
          continue;
        }
        
        response = line;
        break;
      }
      
      if (response.length() > 0) {
        break;
      }
    }
    
    if (responseReceived) {
      Serial.println("Réponse reçue du proxy:");
      Serial.println(response);
      
      if (response.indexOf("OK") != -1) {
        Serial.println("Message envoyé avec succès!");
      } else {
        Serial.println("Échec de l'envoi du message");
      }
    } else {
      Serial.println("Timeout - Pas de réponse reçue du proxy");
    }
    
    client.stop();
    Serial.println("Connexion au proxy fermée");
  } else {
    Serial.println("Échec de la connexion au proxy");
  }
  Serial.println("Fin de l'envoi du message\n");
}

// Fonction pour vérifier les pins et les modules
void checkHardware() {
  Serial.println("\n=== Vérification du matériel ===");
  
  // Vérification du clavier matriciel
  Serial.println("\nVérification du clavier matriciel...");
  for(int i = 0; i < ROW_NUM; i++) {
    pinMode(pin_rows[i], INPUT_PULLUP);
    Serial.print("  Ligne ");
    Serial.print(i);
    Serial.print(" (Pin ");
    Serial.print(pin_rows[i]);
    Serial.print("): ");
    if(digitalRead(pin_rows[i]) == HIGH) {
      Serial.println("OK");
    } else {
      Serial.println("ERREUR - Vérifiez le branchement");
    }
  }
  for(int i = 0; i < COLUMN_NUM; i++) {
    pinMode(pin_columns[i], OUTPUT);
    digitalWrite(pin_columns[i], LOW);
    Serial.print("  Colonne ");
    Serial.print(i);
    Serial.print(" (Pin ");
    Serial.print(pin_columns[i]);
    Serial.print("): ");
    if(digitalRead(pin_columns[i]) == LOW) {
      Serial.println("OK");
    } else {
      Serial.println("ERREUR - Vérifiez le branchement");
    }
  }

  // Vérification de l'écran LCD
  Serial.println("\nVérification de l'écran LCD...");
  lcd.begin(16, 2);
  lcd.clear();
  lcd.print("Test LCD");
  Serial.println("  Message affiché sur LCD - Vérifiez l'affichage");

  // Vérification des LEDs
  Serial.println("\nVérification des LEDs...");
  const int ledPins[] = {LED_PIN, LED_PIN_ALARME_ON, LED_PIN_ALARME_OFF, LED_PIN_DETECTED, BACKLIGHT_PIN};
  const String ledNames[] = {"LED principale", "LED Alarme ON", "LED Alarme OFF", "LED Détection", "Rétroéclairage"};
  
  for(int i = 0; i < 5; i++) {
    Serial.print("  ");
    Serial.print(ledNames[i]);
    Serial.print(" (Pin ");
    Serial.print(ledPins[i]);
    Serial.print("): ");
    
    pinMode(ledPins[i], OUTPUT);
    digitalWrite(ledPins[i], HIGH);
    delay(500);
    if(digitalRead(ledPins[i]) == HIGH) {
      Serial.println("OK");
    } else {
      Serial.println("ERREUR - Vérifiez le branchement");
    }
    digitalWrite(ledPins[i], LOW);
  }

  // Vérification du buzzer
  Serial.println("\nVérification du buzzer...");
  Serial.print("  Buzzer (Pin ");
  Serial.print(BUZZER_PIN);
  Serial.print("): ");
  pinMode(BUZZER_PIN, OUTPUT);
  tone(BUZZER_PIN, 1000, 500);
  Serial.println("Test sonore - Vérifiez si vous entendez le son");
  delay(500);
  noTone(BUZZER_PIN);

  // Vérification du capteur PIR
  Serial.println("\nVérification du capteur PIR...");
  Serial.print("  PIR (Pin ");
  Serial.print(PIR_PIN);
  Serial.print("): ");
  pinMode(PIR_PIN, INPUT);
  int pirValue = digitalRead(PIR_PIN);
  Serial.print("Valeur actuelle: ");
  Serial.println(pirValue);
  Serial.println("  Déplacez-vous devant le capteur pour tester");

  // Vérification du module RFID
  Serial.println("\nVérification du module RFID...");
  Serial.print("  RFID SS (Pin ");
  Serial.print(SS_PIN);
  Serial.print("): ");
  pinMode(SS_PIN, OUTPUT);
  digitalWrite(SS_PIN, HIGH);
  if(digitalRead(SS_PIN) == HIGH) {
    Serial.println("OK");
  } else {
    Serial.println("ERREUR - Vérifiez le branchement");
  }
  
  Serial.print("  RFID RST (Pin ");
  Serial.print(RST_PIN);
  Serial.print("): ");
  pinMode(RST_PIN, OUTPUT);
  digitalWrite(RST_PIN, HIGH);
  if(digitalRead(RST_PIN) == HIGH) {
    Serial.println("OK");
  } else {
    Serial.println("ERREUR - Vérifiez le branchement");
  }

  Serial.println("\n=== Fin de la vérification ===");
  Serial.println("Veuillez vérifier que tous les tests sont OK");
  Serial.println("Si un test échoue, vérifiez le branchement correspondant");
  delay(3000); // Attente pour lire les messages
}