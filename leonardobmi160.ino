#include <DFRobot_BMI160.h>

// Create the sensor object
DFRobot_BMI160 bmi160;

// DFRobot uses 0x69 as the default I2C address
const uint8_t i2c_addr = 0x69;

void setup() {
  // Initialize native USB serial for Leonardo
  Serial.begin(115200);
  
  // Wait for the Serial Monitor to be opened (vital for Leonardo boards)
  while (!Serial); 

  Serial.println("Initialising DFRobot BMI160...");

  // Reset the sensor hardware
  if (bmi160.softReset() != BMI160_OK) {
    Serial.println("Reset failed!");
    while(1);
  }

  // FIXED: I2cInit only takes the I2C address parameter
  if (bmi160.I2cInit(i2c_addr) != BMI160_OK) {
    Serial.println("I2C initialisation failed! Check your wiring.");
    while (1);
  }

  Serial.println("DFRobot BMI160 ready!");
}

void loop() {
  // Array to hold the 6-axis data: 
  // onlyData[0..2] = Gyro X, Y, Z
  // onlyData[3..5] = Accel X, Y, Z
  int16_t onlyData[6] = {0};

  // Fetch all 6-axis data via the DFRobot library
  if (bmi160.getAccelGyroData(onlyData) == BMI160_OK) {
    
    // Print Gyroscope Readings (Raw values scaled to rad/s)
    // The DFRobot library outputs raw data here, so we multiply by 3.14 / 180 to scale
    Serial.print("Gyro X: "); Serial.print(onlyData[0] * 3.14 / 180.0); Serial.print(" rad/s\t");
    Serial.print("Y: ");      Serial.print(onlyData[1] * 3.14 / 180.0); Serial.print(" rad/s\t");
    Serial.print("Z: ");      Serial.print(onlyData[2] * 3.14 / 180.0); Serial.print(" rad/s");
    Serial.print("  ||  ");

    // Print Accelerometer Readings (Raw values divided by full-scale factor 16384 to get g-force)
    Serial.print("Accel X: "); Serial.print(onlyData[3] / 16384.0); Serial.print(" g\t");
    Serial.print("Y: ");       Serial.print(onlyData[4] / 16384.0); Serial.print(" g\t");
    Serial.print("Z: ");       Serial.print(onlyData[5] / 16384.0); Serial.print(" g");
    Serial.println();
  } else {
    Serial.println("Error reading sensor data.");
  }

  delay(100); // Poll data every 100ms
}
