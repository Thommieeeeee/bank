/* Typical pin layout used:
 * -----------------------------------------------------------------------------------------
 *             MFRC522      Arduino       Arduino   Arduino    Arduino          Arduino
 *             Reader/PCD   Uno/101       Mega      Nano v3    Leonardo/Micro   Pro Micro
 * Signal      Pin          Pin           Pin       Pin        Pin              Pin
 * -----------------------------------------------------------------------------------------
 * RST/Reset   RST          9             5         D9         RESET/ICSP-5     RST
 * SPI SS      SDA(SS)      10            53        D10        10               10
 * SPI MOSI    MOSI         11 / ICSP-4   51        D11        ICSP-4           16
 * SPI MISO    MISO         12 / ICSP-1   50        D12        ICSP-1           14
 * SPI SCK     SCK          13 / ICSP-3   52        D13        ICSP-3           15
 *
 * More pin layouts for other boards can be found here: https://github.com/miguelbalboa/rfid#pin-layout
 */

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

int XX = 0;
int V = 0;
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
  webSocket.loop();

  while (1) {
    for (byte i = 0; i < 6; i++) key.keyByte[i] = 0xFF;

    // Reset the loop if no new card present on the sensor/reader. This saves the entire process when idle.
    if (!mfrc522.PICC_IsNewCardPresent()) {
      return;
    }

    // Select one of the cards
    if (!mfrc522.PICC_ReadCardSerial()) {
      return;
    }

    Serial.setTimeout(20000L);

    String content = "";

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

    String iban = String(readblockIban1) + String(readblockIban2);
    String maskedIban = String(readblockIban1) + String(maskedblockIban2);

    webSocket.sendTXT("Pas gescand");

    Serial.println();
    Serial.println("Voer pincode in: ");

    static String input = "";  // Houdt de cumulatieve invoer bij

    while (true) {
      char key = keypad.getKey();
      if (key) {
        if (key == '#') {
          if (input.length() == 4) {
            pincode = input;
            Serial.println("Pincode is: " + pincode);
            webSocket.sendTXT("Pincode ingevoerd: " + pincode);  // Verstuur pincode via WebSocket
            input = "";
            break;
          } else {
            Serial.println("Ongeldige invoer. Voer een geldige pincode in.");
            webSocket.sendTXT("Ongeldige pincode. Voer opnieuw in.");
          }
        } else if (key == '*') {
          input = "";  // Reset de invoer
          Serial.println("Invoer geannuleerd.");
          webSocket.sendTXT("Invoer geannuleerd.");
        } else {
          if (input.length() < 4) {
            input += key;
            Serial.print(key);
            webSocket.sendTXT(input);  // Verstuur de ingevoerde cijfers via WebSocket
          } else {
            Serial.println("Maximaal 4 cijfers. Druk op '#' om te bevestigen.");
          }
        }
      }
    }

    Serial.println(input);

    int amount;
    int flag = 0;

    //loginToServer(iban, code, noobToken);

    Serial.println("Wil je geld opnemen? Druk op 1 voor ja en op 2 voor nee");
    while (true) {
      char key = keypad.getKey();
      if (key) {
        if (key == '1') {
          Serial.println("Hoeveel wilt u opnemen?");
          while (true) {
            key = keypad.getKey();
            if (key) {
              if (key == '#') {          // '#' wordt gebruikt om de invoer te voltooien
                amount = input.toInt();  // Converteer de invoer naar een geheel getal
                if (amount > 0) {        // Controleer of het bedrag geldig is
                  Serial.println();
                  Withdraw withdraw = withdrawFromAccount(iban, pincode, content, amount, noobToken);
                  if (withdraw.httpResponseCode == 200) {
                    moneyDispenser(amount);
                  }
                } else {
                  Serial.println("Ongeldige invoer. Voer een geldig bedrag in.");
                }
                input = "";  // Reset de invoer na voltooiing
                break;
              } else if (key == '*') {  // '*' wordt gebruikt om de invoer te annuleren
                input = "";             // Reset de invoer
                Serial.println("Invoer geannuleerd.");
              } else {  // Anders, voeg de toets toe aan de invoer
                input += key;
                Serial.print(key);  // Optioneel: print de ingedrukte toets naar de seriële monitor
              }
            }
          }
          break;
        } else if (key == '2') {
          Serial.println("\nOke");
          break;
        } else {
          Serial.println("\nVerkeerde input");
        }
      }
      flag++;
    }

    if (ready == 1) {
      Serial.println("Wilt u een bon? Druk op 1 voor ja en 2 voor nee");
      while (true) {
        char key = keypad.getKey();
        if (key) {
          if (key == '1') {
            Serial.println("Bon wordt geprind.");
            bonprinter(content, amount, maskedIban, transactionTimes, getCurrentTimeFromServer());
            break;
          } else if (key == '2') {
            Serial.println("\nOke");
            break;
          } else {
            Serial.println("\nVerkeerde input");
          }
        }
      }
    }

    delay(3000);
    Serial.println("Houdt uw pas tegen de lezer.");

    mfrc522.PICC_HaltA();
    mfrc522.PCD_StopCrypto1();
  }
}

void webSocketEvent(WStype_t type, uint8_t* payload, size_t length) {
  switch (type) {
    case WStype_DISCONNECTED:
      Serial.println("WebSocket verbroken");
      break;
    case WStype_CONNECTED:
      Serial.println("WebSocket verbonden");
      break;
    case WStype_TEXT:
      Serial.printf("Bericht ontvangen: %s\n", payload);
      break;
      // Andere gevallen zoals binaire berichten, pingen etc.
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

void moneyDispenser(int amount) {
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