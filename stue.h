#define TEMP_INTERVAL 60000
#define DS18_PIN 8     // comment out if room sensor doesn't have DS18 sensor
#define DHT_PIN 5      // comment out if room sensor doesn't have DHT sensor
#define DHT_TYPE DHT11 // DHT11, DHT22, DHT21
#define LED_PIN 9
#define ROOM_SENSOR_ID '2'
#define VCC_PIN A3
#define VCC_MAX 620 // (4.0V / 2) / 3.3V * 1024   for NiMh
#define VCC_MIN 465 // (3.0V / 2) / 3.3V * 1024   for NiMh
