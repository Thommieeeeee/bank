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

#define RST_PIN 15          // Configurable, see typical pin layout above
#define SS_PIN 5         // Configurable, see typical pin layout above

const int printerBaudrate = 9600;  // or 19200 usually
const byte rxPin = 2;   // check datasheet of your board
const byte txPin = 0;   // check datasheet of your board
const byte dtrPin = 4;  // optional

MFRC522 mfrc522(SS_PIN, RST_PIN);  // Create MFRC522 instance

MFRC522::MIFARE_Key key;
MFRC522::StatusCode status;

int block = 4;
int block2 = 5;
char readblockIban1[18] = {0}; // Initialize with zeros and allow space for null termination
char readblockIban2[18] = {0}; // Initialize with zeros and allow space for null termination

byte buffer = 18;

HardwareSerial mySerial(1);
Adafruit_Thermal printer(&mySerial);

#define ROW_NUM     4 // four rows
#define COLUMN_NUM  4 // four columns

char keys[ROW_NUM][COLUMN_NUM] = {
  {'1', '2', '3', 'A'},
  {'4', '5', '6', 'B'},
  {'7', '8', '9', 'C'},
  {'*', '0', '#', 'D'}
};

byte pin_rows[ROW_NUM]      = {13, 12, 14, 27}; // GPIO19, GPIO18, GPIO5, GPIO17 connect to the row pins
byte pin_column[COLUMN_NUM] = {26, 25, 33, 32};

Keypad keypad = Keypad( makeKeymap(keys), pin_rows, pin_column, ROW_NUM, COLUMN_NUM );

String code = ""; // Initialize code

void bonprinter(String content){
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
  delay(3000L);         // Sleep for 3 seconds
  // printer.wake();       // MUST wake() before printing again, even if reset
  // printer.setDefault();
}


void setup() {
	Serial.begin(9600);		// Initialize serial communications with the PC

	SPI.begin();			// Init SPI bus

	mfrc522.PCD_Init();		// Init MFRC522
	//mfrc522.PCD_DumpVersionToSerial();	// Show details of PCD - MFRC522 Card Reader details

  micros();
  mySerial.begin(printerBaudrate, SERIAL_8N1, rxPin, txPin);  // must be 8N1 mode

  //  printer.enableDtr(dtrPin, LOW); // optional
  //printer.begin();
}

void loop() {

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

  ReadDataFromBlock(block, readblockIban1);
  ReadDataFromBlock(block2, readblockIban2);
  

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
      bonprinter(content);
      sendRequestToAPI(readblockIban1, code, content); // Stuur IBAN, pincode en pasnummer naar de API
      code = "";
    } else {
      Serial.println("\nFoute invoer.");
      code = "";
    }
  } else {
    Serial.println("\nAccess denied");
  }

  delay(3000);
}

void ReadDataFromBlock(int block, char readBlockData[]) {
  /* Authenticating the desired data block for Read access using Key A */
  status = mfrc522.PCD_Authenticate(MFRC522::PICC_CMD_MF_AUTH_KEY_A, block, &key, &(mfrc522.uid));
  if (status != MFRC522::STATUS_OK)
  {
    Serial.print("Authentication failed for Read: ");
    Serial.println(mfrc522.GetStatusCodeName(status));
    return;
  }
  else
  {
    Serial.println("Authentication success");
  }

  /* Reading data from the Block */
  status = mfrc522.MIFARE_Read(block, (byte *)readBlockData, &buffer);
  if (status != MFRC522::STATUS_OK)
  {
    Serial.print("Reading from Block failed: ");
    Serial.println(mfrc522.GetStatusCodeName(status));
    return;
  }
  else
  {
    Serial.println("Block was read successfully");
  }

  // Ensure null termination
  readBlockData[16] = '\0';
}

void sendRequestToAPI(char* iban, String pincode, String pasnummer) {
    HTTPClient http;

    // De URL van de API waar je het verzoek naartoe stuurt
    String apiUrl = "http://145.24.223.83:8080/api/withdraw"; // Vervang <IP_ADRES> door het werkelijke IP-adres van je API-server

    // Maak een JSON-payload met de IBAN, pincode en pasnummer
    String payload = "{\"iban\":\"" + String(iban) + "\",\"pincode\":\"" + pincode + "\",\"pasnummer\":\"" + pasnummer + "\"}";

    // Begin met het configureren van de HTTP-client
    http.begin(apiUrl);
    http.addHeader("Content-Type", "application/json");

    // Voer het POST-verzoek uit met de JSON-payload
    int httpResponseCode = http.POST(payload);

    // Controleer of het verzoek succesvol was
    if (httpResponseCode > 0) {
        Serial.print("HTTP Response code: ");
        Serial.println(httpResponseCode);
        String response = http.getString();
        Serial.println(response);
    } else {
        Serial.print("HTTP Request failed with error code: ");
        Serial.println(httpResponseCode);
    }

    // Beëindig het HTTP-verzoek
    http.end();
}