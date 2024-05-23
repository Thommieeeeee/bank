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
#include <HardwareSerial.h>
#include "Arduino.h"
#include <HTTPClient.h>
#include <WiFi.h>
#include <ArduinoJson.h>
#include <Stepper.h>

#define RST_PIN 15  // Configurable, see typical pin layout above
#define SS_PIN 5    // Configurable, see typical pin layout above

const int printerBaudrate = 9600;  // or 19200 usually
const byte rxPin = 2;              // check datasheet of your board
const byte txPin = 0;              // check datasheet of your board
const byte dtrPin = 4;             // optional

MFRC522 mfrc522(SS_PIN, RST_PIN);  // Create MFRC522 instance

MFRC522::MIFARE_Key key;
MFRC522::StatusCode status;

int block = 4;
int block2 = 5;
char readblockIban1[18] = { 0 };  // Initialize with zeros and allow space for null termination
char readblockIban2[18] = { 0 };  // Initialize with zeros and allow space for null termination

byte buffer = 18;

String noobToken = "f022be6b-e8ba-4470-83d0-3269534f3b8b";

HardwareSerial mySerial(1);
Adafruit_Thermal printer(&mySerial);

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

String code = "";  // Initialize code

void bonprinter(String content) {
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
  printer.boldOn();
  printer.setSize('L');
  printer.println("----------------");
  printer.setSize('S');
  printer.boldOff();
  printer.feed(1);

  printer.println("Opgenomen bedrag");
  printer.justify('R');
  printer.println("15.00");
  printer.justify('L');
  printer.boldOn();
  printer.setSize('L');
  printer.println("----------------");
  printer.setSize('S');
  printer.boldOff();
  printer.feed(1);

  printer.println("IBAN");
  printer.justify('R');
  printer.println("NL06 INGB ********14");
  printer.feed(1);

  printer.justify('L');
  printer.println("Transactienummer");
  printer.justify('R');
  printer.println("1");
  printer.feed(1);

  printer.justify('L');
  printer.println("Pasnummer");
  printer.justify('R');
  printer.println(content);
  printer.justify('L');
  printer.boldOn();
  printer.setSize('L');
  printer.println("----------------");
  printer.setSize('S');
  printer.boldOff();
  printer.feed(2);

  printer.justify('C');
  printer.println("tijd");
  printer.feed(5);

  //printer.sleep();      // Tell printer to sleep
  delay(3000L);  // Sleep for 3 seconds
  // printer.wake();       // MUST wake() before printing again, even if reset
  // printer.setDefault();
}

void initWifi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin("tesla iot", "fsL6HgjN");
  //WiFi.begin("FRITZ!Box gast", "martine20101977");
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

  mfrc522.PCD_Init();  // Init MFRC522
  //mfrc522.PCD_DumpVersionToSerial();	// Show details of PCD - MFRC522 Card Reader details

  micros();
  mySerial.begin(printerBaudrate, SERIAL_8N1, rxPin, txPin);  // must be 8N1 mode

  Serial.println("Houdt uw pas tegen de lezer.");

  //  printer.enableDtr(dtrPin, LOW); // optional
  //printer.begin();
}

void loop() {
  while(1){
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

    Serial.println();

    ReadDataFromBlock(block, readblockIban1);
    ReadDataFromBlock(block2, readblockIban2);

    String iban = String(readblockIban1) + String(readblockIban2);

    if (content.equalsIgnoreCase(" AE 0D 07 02")) {
      Serial.println();
      Serial.println("Voer pincode in: ");

      char key;

      //Blijf wachten totdat er een viercijferige code is ingevoerd
      while (code.length() < 4) {
        key = keypad.getKey();
        if (key != NO_KEY && key >= '0' && key <= '9') {
          code += key;
          Serial.print(key);
        }
      }

      // Controleer of de ingevoerde code overeenkomt met de verwachte code
      if (code == "1234") {
        Serial.println("\nWelkom team 6");
        //bonprinter(content);
        sendInfoRequestToAPI(iban, code, content, noobToken);  // Stuur IBAN, pincode en pasnummer naar de API
        code = "";
        Serial.println("Wil je geld opnemen? Druk op 1 voor ja en op 2 voor nee");
        while (code.length() < 1) {
          key = keypad.getKey();
          if (key != NO_KEY && key >= '0' && key <= '9') {
            code += key;
            Serial.print(key);
          }
        }

        if (code == "1") {
          int amount = 10;
          Serial.println();
          withdrawFromAccount(iban, code, content, amount, noobToken);
        } else if (code == "2") {
          Serial.println("\nOke");
        } else {
          Serial.println("\nVerkeerde input");
        }
        code = "";
      } else {
        Serial.println("\nFoute invoer.");
        code = "";
      }
    } else {
      Serial.println("\nAccess denied");
    }

    delay(3000);
    Serial.println("Houdt uw pas tegen de lezer.");

    mfrc522.PICC_HaltA();
    mfrc522.PCD_StopCrypto1();
  }
}

void ReadDataFromBlock(int block, char readBlockData[]) {
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
  readBlockData[16] = '\0';
}

void sendInfoRequestToAPI(String iban, String pincode, String pasnummer, String token) {
  HTTPClient http;

  // De URL van de API waar je het verzoek naartoe stuurt
  String apiUrl = "http://145.24.223.83:8080/api/accountinfo?target=" + iban;

  // Maak een JSON-payload met de IBAN, pincode en pasnummer
  String payload = "{\"pincode\":\"" + pincode + "\",\"uid\":\"" + pasnummer + "\"}";

  // Begin met het configureren van de HTTP-client
  http.begin(apiUrl);
  http.addHeader("Content-Type", "application/json");
  http.addHeader("NOOB-TOKEN", token);

  // Voer het POST-verzoek uit met de JSON-payload
  int httpResponseCode = http.POST(payload);

  // Controleer of het verzoek succesvol was
  if (httpResponseCode == 200) {
    // Parse de ontvangen JSON-response
    DynamicJsonDocument doc(1024);
    deserializeJson(doc, http.getString());

    // Haal de gegevens uit de JSON-response
    String name = doc["name"].as<String>();
    float balance = doc["balance"].as<float>();

    Serial.print("HTTP Response code: ");
    Serial.println(httpResponseCode);

    // Toon de ontvangen gegevens
    Serial.print("Naam: ");
    Serial.println(name);
    Serial.print("Saldo: ");
    Serial.println(balance);
  } else {
    Serial.print("HTTP Request failed with error code: ");
    Serial.println(httpResponseCode);
  }

  // Beëindig het HTTP-verzoek
  http.end();
}

void withdrawFromAccount(String iban, String pincode, String pasnummer, int amount, String token) {
  HTTPClient http;

  // De URL van de API waar je het verzoek naartoe stuurt
  String apiUrl = "http://145.24.223.83:8080/api/accountinfo?target=" + iban;

  // Maak een JSON-payload met de IBAN, pincode en pasnummer
  String payload = "{\"pincode\":\"" + pincode + "\",\"uid\":\"" + pasnummer + "\",\"amount\":\"" + String(amount) + "\"}";

  // Begin met het configureren van de HTTP-client
  http.begin(apiUrl);
  http.addHeader("Content-Type", "application/json");
  http.addHeader("NOOB-TOKEN", token);

  // Voer het POST-verzoek uit met de JSON-payload
  int httpResponseCode = http.POST(payload);

  // Controleer of het verzoek succesvol was
  if (httpResponseCode == 200) {
    // Parse de ontvangen JSON-response
    DynamicJsonDocument doc(1024);
    deserializeJson(doc, http.getString());

    float amount = doc["amount"].as<float>();  

    Serial.print("HTTP Response code: ");
    Serial.println(httpResponseCode);

    Serial.print("Bedrag: ");
    Serial.println(amount);
    int printAmount = 10;
    //dispenseMoney(printAmount);
  } else {
    Serial.print("HTTP Request failed with error code: ");
    Serial.println(httpResponseCode);
  }

  // Beëindig het HTTP-verzoek
  http.end();
}

// void dispenseMoney(int amount) {
//   int bills[2] = {20, 5};
//   int num_bills = sizeof(bills) / sizeof(int);

//   for (int i = 0; i < num_bills; i++) {
//     int bill = bills[i];
//     int num_steps = amount / bill;
//     amount %= bill;

//     for (int j = 0; j < num_steps; j++) {
//       if (bill == 5) {
//         stepper5.step(stepsPerRevolution);
//         delay(100);
//       } else if (bill == 20) {
//         stepper20.step(stepsPerRevolution);
//         delay(100);
//       }
//     }
//   }
// }