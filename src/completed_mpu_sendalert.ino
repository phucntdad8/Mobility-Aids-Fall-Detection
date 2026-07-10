#include <Wire.h>
#include <MPU6050.h>
#include <HardwareSerial.h>
#include <math.h>

#define simSerial               Serial2
#define MCU_SIM_BAUDRATE        115200
#define MCU_SIM_TX_PIN          27
#define MCU_SIM_RX_PIN          26

// Phone number to send alert
#define PHONE_NUMBER            "+84913029503"

// MPU6050 sensor instance
MPU6050 mpu;

// Fall detection parameters
const float fallThreshold = 15.0; // Jerk threshold to detect a fall
const int sampleInterval = 100;   // Sample interval in milliseconds

// Variables for jerk calculation
float prevAccX = 0, prevAccY = 0, prevAccZ = 0;
unsigned long lastTime = 0;

void sim_at_wait() {
    delay(500);
    while (simSerial.available()) {
        Serial.write(simSerial.read());
    }
}

bool sim_at_cmd(String cmd) {
    simSerial.println(cmd);
    sim_at_wait();
    return true;
}

void setup() {
    Serial.begin(115200);
    Serial.println("\nSystem initializing...");

    // Initialize SIM communication
    simSerial.begin(MCU_SIM_BAUDRATE, SERIAL_8N1, MCU_SIM_RX_PIN, MCU_SIM_TX_PIN);

    // Check SIM connection
    sim_at_cmd("AT");
    sim_at_cmd("ATI");
    sim_at_cmd("AT+CPIN?");
    sim_at_cmd("AT+CSQ");

    // Initialize MPU6050
    Wire.begin();
    mpu.initialize();

    if (mpu.testConnection()) {
        Serial.println("MPU6050 connected successfully.");
    } else {
        Serial.println("MPU6050 connection failed.");
        while (1); // Halt if MPU6050 fails
    }
}

void loop() {
    // Read accelerometer data
    if (millis() - lastTime >= sampleInterval) {
        lastTime = millis();

        int16_t ax, ay, az;
        mpu.getAcceleration(&ax, &ay, &az);

        // Convert raw accelerometer data to G units (1 G = 16384)
        float acceleration_mg_x = ax / 16384.0;
        float acceleration_mg_y = ay / 16384.0;
        float acceleration_mg_z = az / 16384.0;

        // Calculate jerk by taking the derivative of acceleration with respect to time
        float jerkX = (acceleration_mg_x - prevAccX) / (sampleInterval / 1000.0);
        float jerkY = (acceleration_mg_y - prevAccY) / (sampleInterval / 1000.0);
        float jerkZ = (acceleration_mg_z - prevAccZ) / (sampleInterval / 1000.0);

        // Update previous acceleration values for the next iteration
        prevAccX = acceleration_mg_x;
        prevAccY = acceleration_mg_y;
        prevAccZ = acceleration_mg_z;

        // Calculate the magnitude of jerk
        float jerkMagnitude = sqrt(jerkX * jerkX + jerkY * jerkY + jerkZ * jerkZ);
        Serial.print("Jerk Magnitude: ");
        Serial.println(jerkMagnitude);

        // Check for a fall
        if (jerkMagnitude > fallThreshold) {
            Serial.println("Fall detected, calling emergency number...");
            makeCall();
            delay(30000); // Wait 30 seconds to prevent repeated calls
        }
    }
}

void makeCall() {
    String callCmd = "ATD";
    callCmd += PHONE_NUMBER;
    callCmd += ";";
    sim_at_cmd(callCmd);

    // Wait for a short duration to simulate call
    delay(10000); // Adjust delay as needed for call duration

    // End call
    sim_at_cmd("ATH");
    Serial.println("Call ended.");
}
