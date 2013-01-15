#include <avr/eeprom.h>
#include <avr/pgmspace.h>
#include <avr/interrupt.h>
#include <avr/wdt.h>
#include <avr/sleep.h>

#include <Arduino.h>
#include <JeeLib.h>
#include <OneWire.h>
#include <DallasTemperature.h>

ISR(WDT_vect) { Sleepy::watchdogEvent(); }

#define PIN_LED 9
#define PIN_TEMP 8
#define TEMP_INTERVAL 60000
#define DEBUG

const long InternalReferenceVoltage = 1095L;  // Change this to the reading from your internal voltage reference

unsigned char payload[] = "R3       ";
//                           ^^------- recipient, always spaces for broadcast
//                             ^------ 1 = temperature
//                              ^^---- temperature, signed 16-bit int
//                                ^^-- battery, centivolts

OneWire oneWire(PIN_TEMP);
DallasTemperature sensors(&oneWire);
DeviceAddress tempAddress;

void startADC() {
	// REFS1 REFS0          --> 0 0 AREF, Internal Vref turned off
	// MUX3 MUX2 MUX1 MUX0  --> 1110 1.1V (VBG)
    ADMUX = (0<<REFS1) | (0<<REFS0) | (0<<ADLAR) | (1<<MUX3) | (1<<MUX2) | (1<<MUX1) | (0<<MUX0);

    // Start a conversion
    ADCSRA |= _BV( ADSC );
}

int endADC() {
    // Wait for it to complete
    while( ( (ADCSRA & (1<<ADSC)) != 0 ) );

    // Scale the value
    return (((InternalReferenceVoltage * 1023L) / ADC) + 5L) / 10L;
}

void printAddress(DeviceAddress deviceAddress)
{
  for (uint8_t i = 0; i < 8; i++)
  {
    // zero pad the address if necessary
    if (deviceAddress[i] < 16) Serial.print("0");
    Serial.print(deviceAddress[i], HEX);
  }
}

void setup () {
    Serial.begin(57600);
    Serial.print("\n[RoomSensor]");
    rf12_initialize(3, RF12_868MHZ, 5);

    sensors.begin();
    Serial.print("Found ");
    Serial.print(sensors.getDeviceCount());
    Serial.println(" temp sensor.");

    Serial.print("Parasite power is: ");
    if (sensors.isParasitePowerMode()) Serial.println("ON"); else Serial.println("OFF");

    if (!sensors.getAddress(tempAddress, 0)) Serial.println("Unable to find temp sensor");

    Serial.print("Device 0 Address: ");
    printAddress(tempAddress);
    Serial.println();

    sensors.setResolution(tempAddress, 12);
    sensors.setWaitForConversion(false);

    pinMode(PIN_LED, OUTPUT);
}

int getSupplyVoltage() {
    startADC();
    endADC();
    startADC();
    endADC();
    int voltage = 0;
    for (byte b = 0; b < 16; b++) {
        startADC();
        voltage += endADC();
    }
    return voltage >> 4;
}

void sendTempPacket() {
    digitalWrite(PIN_LED, HIGH);

#ifdef DEBUG
    Serial.print("[");
#endif
    sensors.requestTemperatures();
#ifdef DEBUG
    Serial.print("*");
    Serial.flush();
#endif
    digitalWrite(PIN_LED, LOW);
    Sleepy::loseSomeTime(1000);
    digitalWrite(PIN_LED, HIGH);

#ifdef DEBUG
    Serial.print("*");
    Serial.flush();
#endif
    int voltage = getSupplyVoltage();
#ifdef DEBUG
    Serial.print(voltage);
    Serial.flush();
#endif
    *((int*)(payload + 7)) = voltage;

#ifdef DEBUG
    Serial.print("*");
    Serial.flush();
#endif

	rf12_sleep(RF12_WAKEUP);

    float tempC = sensors.getTempC(tempAddress);
#ifdef DEBUG
    Serial.print(tempC);
#endif

    payload[4] = 1;
    int t = (int) (tempC * 100);
    *((int*)(payload + 5)) = (int) t;

#ifdef DEBUG
    Serial.print("*");
#endif
    // send our packet, once possible
    while (!rf12_canSend()) rf12_recvDone();
#ifdef DEBUG
    Serial.print("*");
#endif

    rf12_sendStart(0, payload, sizeof payload);
    // set the sync mode to 2 if the fuses are still the Arduino default
    // mode 3 (full powerdown) can only be used with 258 CK startup fuses
    rf12_sendWait(2);
	rf12_sleep(RF12_SLEEP);

#ifdef DEBUG
    Serial.println("]");
	Serial.flush();
#endif
    digitalWrite(PIN_LED, LOW);
}

void loop () {
	sendTempPacket();
	ADCSRA &= ~(1<<ADEN);
    Sleepy::loseSomeTime(TEMP_INTERVAL);
    ADCSRA |=  (1<<ADEN);
}

int main(void) {

  init();
  setup();

  while(true) {
    loop();
  }
}
