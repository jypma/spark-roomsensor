/*                 JeeNode / JeeNode USB / JeeSMD
     -------|-----------------------|----|-----------------------|----
    |       |D3  A1 [Port2]  D5     |    |D3  A0 [port1]  D4     |    |
    |-------|IRQ AIO +3V GND DIO PWR|    |IRQ AIO +3V GND DIO PWR|    |
    | D1|TXD|                                           ---- ----     |
    | A5|SCL|                                       D12|MISO|+3v |    |
    | A4|SDA|   Atmel Atmega 328                    D13|SCK |MOSI|D11 |
    |   |PWR|   JeeNode / JeeNode USB / JeeSMD         |RST |GND |    |
    |   |GND|                                       D8 |BO  |B1  |D9  |
    | D0|RXD|                                           ---- ----     |
    |-------|PWR DIO GND +3V AIO IRQ|    |PWR DIO GND +3V AIO IRQ|    |
    |       |    D6 [Port3]  A2  D3 |    |    D7 [Port4]  A3  D3 |    |
     -------|-----------------------|----|-----------------------|----
*/

#include <avr/eeprom.h>
#include <avr/pgmspace.h>
#include <avr/interrupt.h>
#include <avr/wdt.h>
#include <avr/sleep.h>

#include <Arduino.h>
#include <JeeLib.h>

#include "stue.h"

#define DEBUG

#ifdef DS18_PIN
#include <OneWire.h>
#include <DallasTemperature.h>
#endif

#ifdef DHT_PIN
#include <DHT.h>
#endif

ISR(WDT_vect) { Sleepy::watchdogEvent(); }

const long InternalReferenceVoltage = 1095L;  // Change this to the reading from your internal voltage reference

#ifdef DHT_PIN
unsigned char payload[] = "R1         ";
//                           ^^--------- recipient, always spaces for broadcast
//                             ^-------- 1 = temperature
//                              ^^------ temperature, 1/100 deg celcius, signed 16-bit int
//                                ^^---- battery, centivolts
//                                  ^^-- humidity, %
#else
unsigned char payload[] = "R1       ";
//                           ^^------- recipient, always spaces for broadcast
//                             ^------ 1 = temperature
//                              ^^---- temperature, signed 16-bit int
//                                ^^-- battery, centivolts
#endif

#ifdef DS18_PIN
OneWire oneWire(DS18_PIN);
DallasTemperature sensors(&oneWire);
DeviceAddress tempAddress;

void printAddress(DeviceAddress deviceAddress)
{
  for (uint8_t i = 0; i < 8; i++)
  {
    // zero pad the address if necessary
    if (deviceAddress[i] < 16) Serial.print("0");
    Serial.print(deviceAddress[i], HEX);
  }
}
#endif

#ifdef DHT_PIN
DHT dht(DHT_PIN, DHT_TYPE);
#endif

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

void setup () {
    Serial.begin(57600);
    Serial.print("\n[RoomSensor]");
    rf12_initialize(3, RF12_868MHZ, 5);
    payload[1] = ROOM_SENSOR_ID;

#ifdef DS18_PIN
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
#endif

#ifdef DHT_PIN
    dht.begin();
    Serial.println("DHT initialized");
#endif

    pinMode(LED_PIN, OUTPUT);
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
    digitalWrite(LED_PIN, HIGH);

#ifdef DEBUG
    Serial.print("[");
#endif

#ifdef DS18_PIN
    sensors.requestTemperatures();
#ifdef DEBUG
    Serial.print("*");
    Serial.flush();
#endif
    digitalWrite(LED_PIN, LOW);
    Sleepy::loseSomeTime(1000);
    digitalWrite(LED_PIN, HIGH);
#ifdef DEBUG
    Serial.print("*");
    Serial.flush();
#endif
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

#ifdef DS18_PIN
    float tempC = sensors.getTempC(tempAddress);
#elif DHT_PIN
    float tempC = dht.readTemperature();
#endif

#ifdef DEBUG
    Serial.print(tempC);
#endif

    payload[4] = 1;
    int t = (int) (tempC * 100);
    *((int*)(payload + 5)) = (int) t;

#ifdef DEBUG
    Serial.print("*");
#endif

#ifdef DHT_PIN
    float hum = dht.readHumidity();
#ifdef DEBUG
    Serial.print(hum);
#endif
    int h = (int) (hum * 100);
    *((int*)(payload + 9)) = (int) h;
#ifdef DEBUG
    Serial.print("*");
#endif
#endif

    // send our packet, once possible
    while (!rf12_canSend()) rf12_recvDone();
#ifdef DEBUG
    Serial.print("*");
    Serial.flush();
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
    digitalWrite(LED_PIN, LOW);
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
