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

#define RST_PIN 9          // Configurable, see typical pin layout above
#define SS_PIN 10         // Configurable, see typical pin layout above
#define TX_PIN 6 // labeled RX on printer
#define RX_PIN 5 // labeled TX on printer

MFRC522 mfrc522(SS_PIN, RST_PIN);  // Create MFRC522 instance

SoftwareSerial mySerial(RX_PIN, TX_PIN); // Declare SoftwareSerial obj first
Adafruit_Thermal printer(&mySerial);

const byte ROWS = 4;
const byte COLS = 4;

char keys[ROWS][COLS] = {
  {'1', '2', '3', 'A'},
  {'4', '5', '6', 'B'},
  {'7', '8', '9', 'C'},
  {'*', '0', '#', 'D'}
};

byte rowPins[ROWS] = {8, 7, 6, 5};
byte colPins[COLS] = {4, 3, 2, 1};

Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

String code = ""; // Initialize code

void setup() {
	Serial.begin(9600);		// Initialize serial communications with the PC
	while (!Serial);		// Do nothing if no serial port is opened (added for Arduinos based on ATMEGA32U4)
	SPI.begin();			// Init SPI bus
	mfrc522.PCD_Init();		// Init MFRC522
	delay(4);				// Optional delay. Some board do need more time after init to be ready, see Readme
	mfrc522.PCD_DumpVersionToSerial();	// Show details of PCD - MFRC522 Card Reader details
	Serial.println(F("Scan een pinpas."));

  pinMode(7, OUTPUT); digitalWrite(7, LOW);

  // NOTE: SOME PRINTERS NEED 9600 BAUD instead of 19200, check test page.
  mySerial.begin(9600);  // Initialize SoftwareSerial
  //Serial1.begin(19200); // Use this instead if using hardware serial
  //printer.begin();
}

void loop() {
	// Reset the loop if no new card present on the sensor/reader. This saves the entire process when idle.
	if ( ! mfrc522.PICC_IsNewCardPresent()) {
		return;
	}

	// Select one of the cards
	if ( ! mfrc522.PICC_ReadCardSerial()) {
		return;
	}

  String content = "";
  String letter;
  Serial.print("UID info: ");
	for (byte i = 0; i < mfrc522.uid.size; i++) 
  {
    Serial.print(mfrc522.uid.uidByte[i] < 0x10 ? " 0" : " ");
    Serial.print(mfrc522.uid.uidByte[i], HEX);
    content.concat(String(mfrc522.uid.uidByte[i] < 0x10 ? " 0" : " "));
    content.concat(String(mfrc522.uid.uidByte[i], HEX));
  }

  if (content.equalsIgnoreCase(" AE 0D 07 02")) {
    Serial.println();
    bonprinter(content);
    Serial.println("Voer pincode in: ");

    char key;

    // Blijf wachten totdat er een viercijferige code is ingevoerd
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

void bonprinter(String content){
  printer.inverseOn();
  printer.justify('C');
  printer.setSize('L');
  printer.println("    BANK OF    ");
  printer.println("   VENEZUELA   ");
  printer.inverseOff();
  printer.setSize('S');
  printer.feed(1);

  printer.println("Wijnhaven 107");
  printer.println("Zimbabwe");
  printer.feed(2);

  printer.justify('R');
  printer.println("Automaat 6");
  printer.justify('L');
  printer.println("--------------------------------");
  printer.feed(1);
  
  printer.println("Opgenomen bedrag");
  printer.justify('R');
  printer.println("15.00");
  printer.justify('L');
  printer.println("--------------------------------");
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
  printer.println("--------------------------------");
  printer.feed(2);

  printer.justify('C');
  printer.println("tijd");
  printer.feed(5);

  printer.sleep();      // Tell printer to sleep
  delay(3000L);         // Sleep for 3 seconds
  printer.wake();       // MUST wake() before printing again, even if reset
  printer.setDefault();
}
