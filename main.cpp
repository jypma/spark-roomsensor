#include <Arduino.h>

#include <JeeLib.h>
#include <avr/eeprom.h>
#include <avr/pgmspace.h>
#include <OneWire.h>
#include <DallasTemperature.h>

#define PIN_LED 9
#define PIN_TEMP 5
#define TEMP_INTERVAL 10000
//#define DEBUG

unsigned char payload[] = "R2     ";
//                           ^^------- recipient, always spaces for broadcast
//                             ^------ 1 = temperature
//                              ^^---- temperature, signed 16-bit int

OneWire oneWire(PIN_TEMP);
DallasTemperature sensors(&oneWire);
DeviceAddress tempAddress;

ISR(WDT_vect) { Sleepy::watchdogEvent(); }

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
    Sleepy::loseSomeTime(1000);
#ifdef DEBUG
    Serial.print("*");
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
	Sleepy::loseSomeTime(TEMP_INTERVAL);
}

int main(void) {

  init();
  setup();

  while(true) {
    loop();
  }
}
