#include "Adafruit_Thermal.h"
#include "SoftwareSerial.h"

#define TX_PIN 6 // labeled RX on printer
#define RX_PIN 5 // labeled TX on printer

SoftwareSerial mySerial(RX_PIN, TX_PIN); // Declare SoftwareSerial obj first
Adafruit_Thermal printer(&mySerial);

void bonprinter(){
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
  printer.println("'pasid'");
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

void setup() {
  pinMode(7, OUTPUT); digitalWrite(7, LOW);

  // NOTE: SOME PRINTERS NEED 9600 BAUD instead of 19200, check test page.
  mySerial.begin(9600);  // Initialize SoftwareSerial
  //Serial1.begin(19200); // Use this instead if using hardware serial
  //printer.begin();

  bonprinter();

}

void loop() {
  // put your main code here, to run repeatedly:

}
