/*******************************************************************************
 * Program name:    software.ino
 * Program purpose: Automatic moisture logging on Arduino
 * Author:          Alexander Nekhaev
 ******************************************************************************/

/*******************************************************************************
 * Includes
 ******************************************************************************/
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>

/*******************************************************************************
 * Definig constants
 ******************************************************************************/

// Pins
#define SOIL_MOISTURE A2 // Soil moisture analog pin: A2
#define RELAY_PIN 5      // Relay signal digital pin: D5
#define SENSOR_POWER 4   // Soil moisture sensor power
                         // digital pin.
                         // If 3.3V then set to -1
#define PIN_3_3V -1      // 3.3V pin value for MoistureSensor power pin

// Operational parameters
#define BAUD_RATE 115200                                    // Serial port baud
                                                            // rate
#define ADC_RESOLUTION 12                                   // ADC resolution
#define VERBOSE 1                                           // Verbose mode
#define DEVICE_NAME "DracaenaMoisture"
#define SERVICE_UUID "a255a717-1559-4ded-9a62-0b285f9a5c8a" // Service UUID
#define CHAR_UUID "6fe14a79-6e62-491b-b2e0-174762f9ac9b"    // Characteristic
                                                            // UUID

// Timings
#define TEST_INTERVAL 5000     // Moisture reading delay, in milliseconds
#define WATERING_DURATION 1000 // Watering duration

/*******************************************************************************
 * Local classes
 ******************************************************************************/

/**
 * @brief Moisture sensor controller class
 *
 * Constructor arguments:
 * @param pin: analog data pin for reading sensor meter
 * @param power: digital power pin. Set to -1 if connected to 3.3V
 *
 * Methods:
 *  - change_resolution(uint8_t bits): sensor ADC bit (possible values: 8/10/12)
 *  - value(): returns current moisture value in interval [0:1]
 *  - is_enabled(): returns true if sensor is currently powered
 */
class MoistureSensor
{
private:
  uint8_t sensor_pin;       // Sensor pin value
  uint8_t sensor_power_pin; // Sensor power pin
  uint8_t adc_bits;         // ADC bits
  bool always_on;           // true if 3.3V pin
  bool current_state;       // true if sensor is powered
  int moisture_max;         // maximal possible moisture value from ADC

  /**
   * @brief Normalizes provided moisture value
   * Arguments:
   * @param value: provided value
   */
  double moisture(uint16_t value)
  {
    return (double)(moisture_max - value) / moisture_max;
  }

  // Powers sensor on
  void enable()
  {
    if (!always_on)                         // checks if power pin is not 3.3V
    {                                       // assuming not powered
      digitalWrite(sensor_power_pin, HIGH); // set high voltage on specified pin
      current_state = true;                 // saves current state as true
    }
  }

  // Powers sensor off
  void disable()
  {
    if (!always_on)                        // checks if power pin is not 3.3V
    {                                      // assuming not powered
      digitalWrite(sensor_power_pin, LOW); // set low voltage on specified pin
      current_state = false;               // saves current state as false
    }
  }

  // Calculates max possible analog moisture level
  int calculate_max_moisture(uint8_t bits)
  {
    switch (bits)  // Hardcoded LUT of function
    {              // 2**(bits) - 1
    case 8:        // bits:
      return 255;  // 8 => 255
    case 10:       //
      return 1023; // 10 => 1023
    case 12:       //
      return 4095; // 12 => 4095
    default:       //
      return 255;  // _ => 255
    }
  }

public:
  /**
   * @param pin: analog data pin for reading sensor meter
   * @param power: digital power pin. Set to -1 if connected to 3.3V
   */
  MoistureSensor(uint8_t pin, uint8_t power)
  {
    sensor_pin = pin;                  // save sensor data pin
    change_resolution(ADC_RESOLUTION); // sets default resolution from macros
    if (VERBOSE)                       // verbose macro flag logging
    {
      Serial.printf("Moisture sensor setup complete at %x with resolution\
 %d\n",
                    sensor_pin, ADC_RESOLUTION);
    }
    if (power == PIN_3_3V) // checks if power is 3.3V
    {
      always_on = true; // if yes, saves in always_on flag
    }
    else
    {
      always_on = false;                   // if no, sets as always_on = false
      sensor_power_pin = power;            // saves power pin
      pinMode(sensor_power_pin, OUTPUT);   // sets sensor power pin to OUTPUT
                                           // mode
      current_state = false;               // sets power state off
      digitalWrite(sensor_power_pin, LOW); // disabling sensor
    }
  }

  /**
   * @brief Changes ADC resolution.
   * Possible values: 8, 10, 12. If other, sets to 8.
   * Arguments:
   * @param bits: ADC resolution (bits)
   */
  void change_resolution(uint8_t bits)
  {
    if (bits == 8 || bits == 10 || bits == 12) // checking if bits value is
    {                                          // applicable
      adc_bits = bits;
    }
    else
    {
      adc_bits = 8; // otherwise set to 8
    }
    analogReadResolution(adc_bits);
    moisture_max = calculate_max_moisture(adc_bits); // calculate max
    if (VERBOSE)
    {
      Serial.printf("Moisture sensor: resolution changed to %d\n",
                    sensor_pin, bits);
    }
  }

  // Returns normalized moisture value in range [0, 1]
  double value()
  {
    if (!is_enabled())
    {
      enable();
    }
    double moisture_value = moisture(analogRead(sensor_pin));
    if (VERBOSE)
    {
      Serial.printf("Moisture value: %lf\n", moisture_value);
    }
    if (is_enabled())
    {
      disable();
    }
    return moisture_value;
  }

  // Returns true if sensor is powered
  bool is_enabled()
  {
    if (!always_on)
    {
      return current_state;
    }
    return true;
  }
};

/**
 * @brief Overriden BLEServerCallbacks for moisture sensor.
 * Automaticly starts advertising on disconnect.
 */
class MoistureServerCallbacks : public BLEServerCallbacks
{
  void onConnect(BLEServer *pServer)
  {
    Serial.println("Connected");
  }

  void onDisconnect(BLEServer *pServer)
  {
    Serial.println("Disconnected");
    pServer->getAdvertising()->start(); // Restart advertising
  }
};

/**
 * @brief Overriden BLECharacteristicCallbacks for moisture sensor.
 * Sends double value of moisture on read command.
 */
class MoistureSensorCallback : public BLECharacteristicCallbacks
{
private:
  MoistureSensor *controller;

public:
  /**
   * @param c: MoistureSensor controller instance
   */
  MoistureSensorCallback(MoistureSensor *c)
  {
    controller = c;
  }

  void onRead(BLECharacteristic *pCharacteristic)
  {
    double read_value = controller->value();
    pCharacteristic->setValue(read_value);
  }
};

/**
 * @brief Moisture sensor BLE controller.
 *
 * Initialized BLE device as "DracaenaMoisture".
 * Creates MoistureSensorCallback &  MoistureServerCallbacks callback instances,
 * Starts advertising.
 */
class BLEMoistureSensor
{
private:
  MoistureSensor *sensor;

public:
  /**
   * @param s: MoistureSensor controller instance
   */
  BLEMoistureSensor(MoistureSensor *s)
  {
    sensor = s;
    BLEDevice::init(DEVICE_NAME); // Sets up BLE module as
                                         // DracaenaMoisture

    BLEServer *pServer = BLEDevice::createServer(); // Creates server
                                                    // instance.

    pServer->setCallbacks(new MoistureServerCallbacks()); // Assigns callback
    BLEService *pService = pServer->createService(SERVICE_UUID);
    BLECharacteristic *pCharacteristic = pService->createCharacteristic(
        CHAR_UUID,
        BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE);
    pCharacteristic->setCallbacks(new MoistureSensorCallback(sensor));
    pService->start();

    // Initial advertising setup
    BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(SERVICE_UUID);
    pAdvertising->setScanResponse(true);
    pAdvertising->setMinPreferred(0x06);
    pAdvertising->setMinPreferred(0x12);
    BLEDevice::startAdvertising();
    Serial.println("Ready for connection");
  }
};
/*******************************************************************************
 * Global variables
 ******************************************************************************/

/*******************************************************************************
 * Arduino specific sketch functions
 ******************************************************************************/

// Initialization
void setup()
{
  Serial.begin(BAUD_RATE); // Enabling serial port with specified baud rate

  MoistureSensor *sensor = new MoistureSensor(SOIL_MOISTURE, SENSOR_POWER);
  BLEMoistureSensor *ble = new BLEMoistureSensor(sensor);
}

// Main loop
void loop()
{
}
