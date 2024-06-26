#include <SPI.h>
#include <MFRC522.h>
#include <Keypad.h>
#include "Adafruit_Thermal.h"
#include "SoftwareSerial.h"
#include "Arduino.h"
#include <HTTPClient.h>
#include <WiFi.h>
#include <ArduinoJson.h>
#include <WebSocketsClient.h>

#define RST_PIN 15  // Configurable, see typical pin layout above
#define SS_PIN 5    // Configurable, see typical pin layout above

#define TX_PIN 16  // Arduino transmit  YELLOW WIRE  labeled RX on printer
#define RX_PIN 17  // Arduino receive   GREEN WIRE   labeled TX on printer

#define motorPin1 21
#define motorPin2 22

SoftwareSerial mySerial(RX_PIN, TX_PIN);  // Declare SoftwareSerial obj first
Adafruit_Thermal printer(&mySerial);

MFRC522 mfrc522(SS_PIN, RST_PIN);  // Create MFRC522 instance

MFRC522::MIFARE_Key key;
MFRC522::StatusCode status;

int block = 4;
int block2 = 5;
char readblockIban1[18] = { 0 };    // Initialize with zeros and allow space for null termination
char readblockIban2[18] = { 0 };    // Initialize with zeros and allow space for null termination
char maskedblockIban2[18] = { 0 };  // Initialize with zeros and allow space for null termination

byte buffer = 18;

int transactionTimes = 0;

String noobToken = "f022be6b-e8ba-4470-83d0-3269534f3b8b";

WebSocketsClient webSocket;

#define ROW_NUM 4     // four rows
#define COLUMN_NUM 4  // four columns

char keys[ROW_NUM][COLUMN_NUM] = {
  { '1', '2', '3', 'A' },
  { '4', '5', '6', 'B' },
  { '7', '8', '9', 'C' },
  { '*', '0', '#', 'D' }
};

byte pin_rows[ROW_NUM] = { 13, 12, 14, 27 };  // GPIO19, GPIO18, GPIO5, GPIO17 connect to the row pins
byte pin_column[COLUMN_NUM] = { 26, 25, 33, 32 };

Keypad keypad = Keypad(makeKeymap(keys), pin_rows, pin_column, ROW_NUM, COLUMN_NUM);

String pincode = "";  // Initialize code
String content = "";
String maskedIban = "";
String iban = "";

static String input = "";  // Houdt de cumulatieve invoer bij

float amount;

int ready = 0;

struct AccountInfo {
  String firstname;
  String lastname;
  float balance;
};

struct Withdraw {
  float amount;
  int httpResponseCode;
};

enum State {
  WAITING_FOR_CARD,
  READING_PINCODE,
  CONFIRM_WITHDRAWAL,
  DISPENSE_MONEY,
  PRINT_RECEIPT
};

State currentState = WAITING_FOR_CARD;

bool withdrawalRequested = false;  // Flag to track if withdrawal has been requested
bool bonRequested = false;

void bonprinter(String content, float amount, String iban, int transactionTime, String time) {
  printer.setTimes(100, 10);
  printer.justify('C');
  printer.setSize('L');
  printer.println("    BANK OF    ");
  printer.println("   VENEZUELA   ");
  printer.setSize('S');
  printer.feed(1);

  printer.println("Wijnhaven 107");
  printer.println("Zimbabwe");
  printer.feed(2);

  printer.justify('R');
  printer.println("Automaat 6");
  printer.justify('L');
  printer.setSize('L');
  printer.println("----------------");
  printer.setSize('S');
  printer.feed(1);

  printer.println("Opgenomen bedrag");
  printer.justify('R');
  printer.println(amount);
  printer.justify('L');
  printer.setSize('L');
  printer.println("----------------");
  printer.setSize('S');
  printer.feed(1);

  printer.println("IBAN");
  printer.justify('R');
  printer.println(iban);
  printer.feed(1);

  printer.justify('L');
  printer.println("Transactienummer");
  printer.justify('R');
  printer.println(transactionTime);
  printer.feed(1);

  printer.justify('L');
  printer.println("Pasnummer");
  printer.justify('R');
  printer.println(content);
  printer.justify('L');
  printer.setSize('L');
  printer.println("----------------");
  printer.setSize('S');
  printer.feed(2);

  printer.justify('C');
  printer.println(time);
  printer.feed(5);

  printer.sleep();  // Tell printer to sleep
  delay(3000L);     // Sleep for 3 seconds
  printer.wake();   // MUST wake() before printing again, even if reset
  printer.setDefault();
}

void initWifi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin("tesla iot", "fsL6HgjN");
  Serial.println("\nConnecting");

  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(100);
  }

  Serial.println("\nConnected to the WiFi network");

  Serial.print("IP Adress: ");
  Serial.println(WiFi.localIP());
}


void setup() {
  Serial.begin(9600);  // Initialize serial communications with the PC

  SPI.begin();  // Init SPI bus

  initWifi();  //Initialize wifi function

  // WebSocket Client instellen
  webSocket.begin("145.24.223.83", 8080, "/");  // Vervang door je serveradres en poort
  webSocket.onEvent(webSocketEvent);

  mfrc522.PCD_Init();  // Init MFRC522
  //mfrc522.PCD_DumpVersionToSerial();	// Show details of PCD - MFRC522 Card Reader details

  pinMode(4, OUTPUT);
  digitalWrite(4, LOW);
  mySerial.begin(9600);
  printer.begin();

  pinMode(motorPin1, OUTPUT);
  pinMode(motorPin2, OUTPUT);

  Serial.println("Houdt uw pas tegen de lezer.");
}

void loop() {
  webSocket.loop();  // Handle WebSocket events
  for (byte i = 0; i < 6; i++) key.keyByte[i] = 0xFF;

  switch (currentState) {
    case WAITING_FOR_CARD:
      pincode = "";  // Initialize code
      content = "";
      maskedIban = "";
      iban = "";
      withdrawalRequested = false;
      bonRequested = false;
      checkRFIDCard();
      break;
    case READING_PINCODE:
      readPincode();
      break;
    case DISPENSE_MONEY:
      dispenseMoney();
      break;
    case PRINT_RECEIPT:
      printReceipt();
      break;
  }
}

void checkRFIDCard() {
  if (mfrc522.PICC_IsNewCardPresent() && mfrc522.PICC_ReadCardSerial()) {
    Serial.setTimeout(20000L);

    Serial.print("UID info: ");
    for (byte i = 0; i < mfrc522.uid.size; i++) {
      Serial.print(mfrc522.uid.uidByte[i] < 0x10 ? " 0" : " ");
      Serial.print(mfrc522.uid.uidByte[i], HEX);
      content.concat(String(mfrc522.uid.uidByte[i] < 0x10 ? " 0" : " "));
      content.concat(String(mfrc522.uid.uidByte[i], HEX));
    }

    String stringWithSpaces = content;
    String contentWithoutSpaces = "";

    for (int i = 0; i < stringWithSpaces.length(); i++)
      if (stringWithSpaces.charAt(i) != ' ') contentWithoutSpaces += stringWithSpaces.charAt(i);
    content = contentWithoutSpaces;
    String result = "";
    for (int i = 0; i < content.length(); i++) {
      char c = content[i];
      if (isAlpha(c)) {
        if (c >= 'a' && c <= 'z') {
          c -= 32;  // Convert lowercase to uppercase by subtracting 32 (ASCII difference)
        }
        result += c;
      } else {
        result += c;
      }
    }
    content = result;

    Serial.println();

    readDataFromBlock1(block, readblockIban1);
    maskedDataFromBlock2(block2, maskedblockIban2);
    readDataFromBlock2(block2, readblockIban2);

    iban = String(readblockIban1) + String(readblockIban2);
    maskedIban = String(readblockIban1) + String(maskedblockIban2);

    webSocket.sendTXT("Pas gescand");

    pincode = "";
    currentState = READING_PINCODE;
  }
}

void readPincode() {
  char key = keypad.getKey();
  if (key) {
    if (key == '#') {
      if (input.length() == 4) {
        String pincode = input;
        Serial.println("Pincode is: " + pincode);
        AccountInfo accountInfo = sendInfoRequestToAPI(iban, pincode, content, noobToken);  // Stuur IBAN, pincode en pasnummer naar de API
        input = "";
      } else {
        Serial.println("Ongeldige invoer. Voer een geldige pincode in.");
        webSocket.sendTXT("{\"message\":\"Ongeldige pincode. Voer opnieuw in.\"}");
      }
    } else if (key == '*') {
      input = "";  // Reset de invoer
      Serial.println("Invoer geannuleerd.");
      webSocket.sendTXT("{\"message\":\"Invoer geannuleerd.\"}");
    } else {
      if (input.length() < 4) {
        input += key;
        Serial.print(key);

        // JSON-bericht maken voor elke ingevoerde cijfer
        StaticJsonDocument<200> doc;
        doc["pincode"] = input;
        String jsonMessage;
        serializeJson(doc, jsonMessage);

        // Verstuur JSON-bericht via WebSocket
        webSocket.sendTXT(jsonMessage);
      } else {
        Serial.println("Maximaal 4 cijfers. Druk op '#' om te bevestigen.");
      }
    }
  }
}

void dispenseMoney() {
  if (withdrawalRequested) {
    webSocket.sendTXT("Wacht op geld.");
    moneyDispenser(amount);
    if (ready == 1) {
      webSocket.sendTXT("Geld is uitgegeven.");
      currentState = PRINT_RECEIPT;
    }
  }
}

void printReceipt() {
  if (bonRequested) {
    bonprinter(content, amount, maskedIban, transactionTimes, getCurrentTimeFromServer());
    mfrc522.PICC_HaltA();
    mfrc522.PCD_StopCrypto1();
    currentState = WAITING_FOR_CARD;
  }
}

void readDataFromBlock1(int block, char readBlockData[]) {
  /* Authenticating the desired data block for Read access using Key A */
  status = mfrc522.PCD_Authenticate(MFRC522::PICC_CMD_MF_AUTH_KEY_A, block, &key, &(mfrc522.uid));
  if (status != MFRC522::STATUS_OK) {
    Serial.print("Authentication failed for Read: ");
    Serial.println(mfrc522.GetStatusCodeName(status));
    return;
  } else {
    Serial.println("Authentication success");
  }

  /* Reading data from the Block */
  status = mfrc522.MIFARE_Read(block, (byte*)readBlockData, &buffer);
  if (status != MFRC522::STATUS_OK) {
    Serial.print("Reading from Block failed: ");
    Serial.println(mfrc522.GetStatusCodeName(status));
    return;
  } else {
    Serial.println("Block was read successfully");
  }

  // Ensure null termination
  readBlockData[8] = '\0';
}

void readDataFromBlock2(int block, char readBlockData[]) {
  /* Authenticating the desired data block for Read access using Key A */
  status = mfrc522.PCD_Authenticate(MFRC522::PICC_CMD_MF_AUTH_KEY_A, block, &key, &(mfrc522.uid));
  if (status != MFRC522::STATUS_OK) {
    Serial.print("Authentication failed for Read: ");
    Serial.println(mfrc522.GetStatusCodeName(status));
    return;
  } else {
    Serial.println("Authentication success");
  }

  /* Reading data from the Block */
  status = mfrc522.MIFARE_Read(block, (byte*)readBlockData, &buffer);
  if (status != MFRC522::STATUS_OK) {
    Serial.print("Reading from Block failed: ");
    Serial.println(mfrc522.GetStatusCodeName(status));
    return;
  } else {
    Serial.println("Block was read successfully");
  }

  // Ensure null termination
  readBlockData[10] = '\0';
}

void maskedDataFromBlock2(int block, char readBlockData[]) {
  // Authenticating the desired data block for Read access using Key A
  status = mfrc522.PCD_Authenticate(MFRC522::PICC_CMD_MF_AUTH_KEY_A, block, &key, &(mfrc522.uid));
  if (status != MFRC522::STATUS_OK) {
    Serial.print("Authentication failed for Read: ");
    Serial.println(mfrc522.GetStatusCodeName(status));
    return;
  } else {
    Serial.println("Authentication success");
  }

  // Reading data from the Block
  byte buffer[18];  // Buffer to hold block data, 16 bytes data + 2 bytes CRC
  byte size = sizeof(buffer);
  status = mfrc522.MIFARE_Read(block, buffer, &size);
  if (status != MFRC522::STATUS_OK) {
    Serial.print("Reading from Block failed: ");
    Serial.println(mfrc522.GetStatusCodeName(status));
    return;
  } else {
    Serial.println("Block was read successfully");
  }

  // Ensure null termination
  memcpy(readBlockData, buffer, 10);
  readBlockData[10] = '\0';

  // Mask all but the last two characters with asterisks
  for (int i = 0; i < 8; i++) {
    readBlockData[i] = '*';
  }
}

void webSocketEvent(WStype_t type, uint8_t* payload, size_t length) {
  // Declare variables before the switch statement
  StaticJsonDocument<200> doc;
  DeserializationError error;
  String output;
  String message;

  switch (type) {
    case WStype_DISCONNECTED:
      Serial.printf("[WSc] Disconnected!\n");
      break;
    case WStype_CONNECTED:
      Serial.printf("[WSc] Connected to url: %s\n", payload);
      webSocket.sendTXT("ESP_CONNECTED");
      break;
    case WStype_TEXT:
      Serial.printf("Received text: %s\n", payload);

      // Convert payload to String
      message = String((char*)payload);

      // Print received message
      Serial.println("Received message: " + message);

      // Check if message is JSON
      if (message.startsWith("{")) {
        // Parse JSON message
        error = deserializeJson(doc, payload, length);
        if (error) {
          Serial.print("deserializeJson() failed: ");
          Serial.println(error.c_str());
          return;
        }

        // Check if message is "Withdraw"
        if (doc.containsKey("message") && doc["message"] == "Withdraw") {
          // Extract amount from JSON
          amount = doc["amount"];

          Withdraw withdraw = withdrawFromAccount(iban, pincode, content, amount, noobToken);
          if (withdraw.httpResponseCode == 200) {
            withdrawalRequested = true;
          }
        }
      } else {
        if (message == "Return") {  // If the message is "Return", restart the ESP
          Serial.println("Return ontvangen. Herstart ESP.");
          mfrc522.PICC_HaltA();
          mfrc522.PCD_StopCrypto1();
          currentState = WAITING_FOR_CARD;
        }
        if (message == "Bon") {
          Serial.println("Bon commando ontvangen.");
          transactionTimes++;
          bonRequested = true;
        }
        if (message == "Geen bon") {
          Serial.println("Geen bon commando ontvangen.");
          transactionTimes++;
          mfrc522.PICC_HaltA();
          mfrc522.PCD_StopCrypto1();
          currentState = WAITING_FOR_CARD;
        }
      }
      break;
    case WStype_BIN:
      Serial.printf("[WSc] get binary length: %u\n", length);
      break;
  }
}

void handleWebSocketMessages() {
  // Check for incoming WebSocket messages
  webSocket.loop();
}

String getCurrentTimeFromServer() {
  HTTPClient http;
  String currentTime = "";

  // De URL van de API waar je het verzoek naartoe stuurt
  String apiUrl = "http://145.24.223.83:8080/api/time";

  // Begin met het configureren van de HTTP-client
  http.begin(apiUrl);
  http.addHeader("NOOB-TOKEN", noobToken);

  // Voer het GET-verzoek uit
  int httpResponseCode = http.GET();

  // Controleer of het verzoek succesvol was
  if (httpResponseCode == 200) {
    // Parse de ontvangen JSON-response
    DynamicJsonDocument doc(1024);
    deserializeJson(doc, http.getString());

    currentTime = doc["currentTime"].as<String>();

    currentTime = adjustTime(currentTime, 7200);
  } else {
    Serial.print("HTTP Request failed with error code: ");
    Serial.println(httpResponseCode);
  }

  // Beëindig het HTTP-verzoek
  http.end();
  return currentTime;
}

String adjustTime(String currentTime, long adjustment) {
  // Parse de ontvangen tijd
  int year = currentTime.substring(0, 4).toInt();
  int month = currentTime.substring(5, 7).toInt();
  int day = currentTime.substring(8, 10).toInt();
  int hour = currentTime.substring(11, 13).toInt();
  int minute = currentTime.substring(14, 16).toInt();
  int second = currentTime.substring(17, 19).toInt();

  // Voeg de aanpassing toe aan de tijd
  // Houd er rekening mee dat dit eenvoudig is en niet omgaat met dagelijkse, maandelijkse of jaarlijkse overloop
  // Dit is alleen een algemene aanpassing voor uren
  hour += (adjustment / 3600);
  if (hour >= 24) {
    hour -= 24;
    // Pas de datum ook aan indien nodig
    // Dit moet echter afhangen van je specifieke behoeften en situatie
  }

  // Formatteer de tijd terug naar een string
  String adjustedTime = String(year) + "-" + formatTimeElement(month) + "-" + formatTimeElement(day) + " " + formatTimeElement(hour) + ":" + formatTimeElement(minute) + ":" + formatTimeElement(second);
  return adjustedTime;
}

String formatTimeElement(int element) {
  // Voeg een voorloopnul toe als het element kleiner is dan 10
  return (element < 10 ? "0" + String(element) : String(element));
}

AccountInfo sendInfoRequestToAPI(String iban, String pincode, String pasnummer, String token) {
  HTTPClient http;
  AccountInfo accountInfo;

  // De URL van de API waar je het verzoek naartoe stuurt
  String apiUrl = "http://145.24.223.83:8080/api/accountinfo?target=" + iban;

  // Maak een JSON-object en voeg de velden toe
  DynamicJsonDocument payload(200);
  payload["pincode"] = pincode;
  payload["uid"] = pasnummer;

  // Maak een string van het JSON-object
  String jsonString;
  serializeJson(payload, jsonString);

  // Begin met het configureren van de HTTP-client
  http.begin(apiUrl);
  http.addHeader("Content-Type", "application/json");
  http.addHeader("NOOB-TOKEN", token);

  // Voer het POST-verzoek uit met de JSON-payload
  int httpResponseCode = http.POST(jsonString);

  // Controleer of het verzoek succesvol was
  if (httpResponseCode == 200) {
    // Parse de ontvangen JSON-response
    DynamicJsonDocument doc(1024);
    deserializeJson(doc, http.getString());

    // Haal de gegevens uit de JSON-response
    accountInfo.firstname = doc["firstname"].as<String>();
    accountInfo.lastname = doc["lastname"].as<String>();
    accountInfo.balance = doc["balance"].as<float>();

    DynamicJsonDocument jsonDoc(1024);  // JSON-document maken

    // Gegevens toevoegen aan het JSON-document
    jsonDoc["message"] = "Inloggen success";
    jsonDoc["firstname"] = accountInfo.firstname;
    jsonDoc["balance"] = accountInfo.balance;

    // JSON-bericht omzetten naar een string
    String jsonString;
    serializeJson(jsonDoc, jsonString);

    // Verstuur het JSON-bericht via WebSocket
    webSocket.sendTXT(jsonString);

    Serial.println("HTTP Response code: ");
    Serial.println(httpResponseCode);

    // Toon de ontvangen gegevens
    Serial.print("Voornaam: ");
    Serial.println(accountInfo.firstname);
    Serial.print("Achternaam: ");
    Serial.println(accountInfo.lastname);
    Serial.print("Saldo: ");
    Serial.println(accountInfo.balance);

    currentState = DISPENSE_MONEY;
  } else {
    Serial.print("HTTP Request failed with error code: ");
    Serial.println(httpResponseCode);
  }

  // Beëindig het HTTP-verzoek
  http.end();
  return accountInfo;
}

Withdraw withdrawFromAccount(String iban, String pincode, String pasnummer, int amount, String token) {
  HTTPClient http;
  Withdraw withdraw;

  // De URL van de API waar je het verzoek naartoe stuurt
  String apiUrl = "http://145.24.223.83:8080/api/withdraw?target=" + iban;

  // Maak een JSON-object en voeg de velden toe
  DynamicJsonDocument payload(200);
  payload["amount"] = amount;
  payload["pincode"] = pincode;
  payload["uid"] = pasnummer;

  // Maak een string van het JSON-object
  String jsonString;
  serializeJson(payload, jsonString);

  // Begin met het configureren van de HTTP-client
  http.begin(apiUrl);
  http.addHeader("Content-Type", "application/json");
  http.addHeader("NOOB-TOKEN", token);

  // Voer het POST-verzoek uit met de JSON-payload
  withdraw.httpResponseCode = http.POST(jsonString);

  // Controleer of het verzoek succesvol was
  if (withdraw.httpResponseCode == 200) {
    // Parse de ontvangen JSON-response
    DynamicJsonDocument doc(1024);
    deserializeJson(doc, http.getString());
  } else {
    Serial.print("HTTP Request failed with error code: ");
    Serial.println(withdraw.httpResponseCode);
  }

  // Beëindig het HTTP-verzoek
  http.end();

  return withdraw;
}

void moneyDispenser(int amount) {
  int XX = 0;
  int V = 0;
  ready = 0;
  Serial.println(amount);
  while (amount >= 0) {
    if (amount >= 20) {
      digitalWrite(motorPin1, HIGH);
      digitalWrite(motorPin2, LOW);
      amount -= 20;
      XX += 1;
      Serial.println(amount);
      delay(1000);
    } else if (amount < 20 && amount >= 5) {
      digitalWrite(motorPin1, LOW);
      digitalWrite(motorPin2, HIGH);
      amount -= 5;
      V += 1;
      Serial.println(amount);
      delay(1000);
    } else {
      // Geen geld meer uit te geven, stop de motoren
      digitalWrite(motorPin1, LOW);
      digitalWrite(motorPin2, LOW);
      break;  // Breek de loop als het bedrag kleiner is dan 5
    }
  }
  // Zet de motoren uit na het beëindigen van de loop
  digitalWrite(motorPin1, LOW);
  digitalWrite(motorPin2, LOW);
  ready = 1;  // Geef aan dat het proces klaar is
}