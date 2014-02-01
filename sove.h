#define TEMP_INTERVAL 60000
#undef  DS18_PIN 8     // comment out if room sensor doesn't have DS18 sensor
#define DHT_PIN 4      // comment out if room sensor doesn't have DHT sensor
#define DHT_TYPE DHT22 // DHT11, DHT22, DHT21
#define LED_PIN 9
#define ROOM_SENSOR_ID '4'
#define VCC_PIN A1
#define VCC_MAX 620 // (4.0V / 2) / 3.3V * 1024   for LiPo
#define VCC_MIN 480 // (3.1V / 2) / 3.3V * 1024   for LiPo
