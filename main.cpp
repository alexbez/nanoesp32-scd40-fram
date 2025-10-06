

// Include Required Libraries
#include <Wire.h>
#include <Adafruit_FRAM_I2C.h>
#include <Adafruit_EEPROM_I2C.h>
#include <SensirionI2cScd4x.h>


#define MONITOR_SPEED 115200
#define NO_ERROR      0
#define DATA_ADDRESS  0

//const int I2C_SDA_PIN = 8;
//const int I2C_SCL_PIN = 9;

const int FRAM_I2C_ADDR = 0x50;
const int SCD40_I2C_ADDR = 0x62;

Adafruit_FRAM_I2C fram = Adafruit_FRAM_I2C();
SensirionI2cScd4x sensor;

static int16_t error;
static char error_message[80];

struct sensorData {
  uint16_t co2;
  float temp;
  float humid;
};

sensorData sensor_data;

void setup()
{
  uint16_t  manufacturer_id;
  uint16_t product_id;

  Serial.begin(MONITOR_SPEED);
  while (!Serial) {
    delay(10);
  }
  delay(1000);
  Serial.println("\nESP32S3-SDC40-FRAM test\n");

  // Initialize the I2C bus with our defined pins.
  Wire.begin();
  Serial.println("I2C bus initialized");

  // Initialize FRAM
  if (!fram.begin()) {
    Serial.println("Could not find a valid FRAM chip. Check wiring!");
    while (true) {}
  }
  Serial.println("FRAM initialized");

  sensor.begin(Wire, SCD40_I2C_ADDR);
  
  error = sensor.wakeUp();
  if(error != NO_ERROR) {
    errorToString(error, error_message, sizeof(error_message));
    Serial.print("Error executing wakeUp(): ");
    Serial.println(error_message);
    while(true) {}
  }

  error = sensor.stopPeriodicMeasurement();
    if (error != NO_ERROR) {
        Serial.print("Error trying to execute stopPeriodicMeasurement(): ");
        errorToString(error, error_message, sizeof error_message);
        Serial.println(error_message);
    }
    error = sensor.reinit();
    if (error != NO_ERROR) {
        Serial.print("Error trying to execute reinit(): ");
        errorToString(error, error_message, sizeof error_message);
        Serial.println(error_message);
    }

  Serial.println("SCD40 sensor initialized");

  if(!fram.begin(FRAM_I2C_ADDR)) {
    Serial.println("FRAM not found");
    //while(true) {}
  }

  Serial.println("FRAM memory found");
  fram.getDeviceID(&manufacturer_id, &product_id);
  Serial.print("FRAM Manufacturer ID: ");
  Serial.print(manufacturer_id);
  Serial.print(", Product ID: ");
  Serial.println(product_id);

  // --- POWER-ON RECOVERY ---
  
  int lastCO2 = 0;
  
  fram.read(DATA_ADDRESS, (uint8_t*)&sensor_data, sizeof(sensorData));
  lastCO2 = sensor_data.co2;

  // isnan() checks for "Not a Number". Uninitialized FRAM often has this
  if (!isnan(lastCO2) && lastCO2 > 0 && lastCO2 < 10000) {
    Serial.println("-------------------------------------------");
    Serial.println(">> Recovered last known values: ");
    Serial.print(lastCO2);
    Serial.println(" ppm");
    Serial.print(sensor_data.temp);
    Serial.println(" C");
    Serial.print(sensor_data.humid);
    Serial.println(" %");
    Serial.println("-------------------------------------------");
  } else {
    Serial.println("No valid previous data found in FRAM.");
  }

  // Slight Delay
  delay(1500);

  error = sensor.startPeriodicMeasurement();
  if (error != NO_ERROR) {
    Serial.print("Error trying to execute startPeriodicMeasurement(): ");
    errorToString(error, error_message, sizeof error_message);
    Serial.println(error_message);
    return;
  }

  Serial.println("Starting real-time CO2 measurement and logging...");
}

void loop() {
  bool data_ready = false;
  uint16_t co2 = 0;
  float temperature;
  float humidity;

  delay(3000);
  
  error = sensor.getDataReadyStatus(data_ready);
  if (error != NO_ERROR) {
    Serial.print("Error trying to execute getDataReadyStatus(): ");
    errorToString(error, error_message, sizeof error_message);
    Serial.println(error_message);
    return;
  }
  
  while (!data_ready) {
    delay(100);
    error = sensor.getDataReadyStatus(data_ready);
    if (error != NO_ERROR) {
      Serial.print("Error trying to execute getDataReadyStatus(): ");
      errorToString(error, error_message, sizeof error_message);
      Serial.println(error_message);
      return;
    }
  }

  error = sensor.readMeasurement(co2, temperature, humidity);
  if (error != NO_ERROR) {
      Serial.print("Error trying to execute readMeasurement(): ");
      errorToString(error, error_message, sizeof error_message);
      Serial.println(error_message);
      return;
  }
  

  // Check if the read was successful
  if (isnan(co2)) {
    Serial.println("Failed to read CO2 from SDC40 sensor!");
    return;
  }

  // Print the current temperature to the serial monitor
  Serial.print("Logging CO2, temperature, humidity: ");
  Serial.print(co2);
  Serial.print(" ppm, ");
  Serial.print(temperature);
  Serial.print(" C, ");
  Serial.print(humidity);
  Serial.print(" %   ");

  // Write the new temperature value to FRAM, overwriting the old one.
  // This happens on every loop, demonstrating high write endurance.
  sensor_data.co2 = co2;
  sensor_data.temp = temperature;
  sensor_data.humid = humidity;
  fram.write(DATA_ADDRESS, (uint8_t *)&sensor_data, sizeof(sensorData));
  Serial.println("Data written to FRAM");

  // Wait a short amount of time before the next reading.
  delay(2000);
}