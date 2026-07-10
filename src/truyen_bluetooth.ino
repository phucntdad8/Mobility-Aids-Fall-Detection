#include <Wire.h>
#include <MPU6050.h>
#include <TinyGPSPlus.h>
#include <BluetoothSerial.h>
#include <math.h>

// Chân kết nối SIM module
#define SIM_TX_PIN              27
#define SIM_RX_PIN              26
#define MCU_SIM_BAUDRATE        115200

// Chân kết nối GPS module
#define GPS_TX_PIN              17
#define GPS_RX_PIN              16
#define MCU_GPS_BAUDRATE        115200

// Số điện thoại nhận cảnh báo
#define PHONE_NUMBER            "+84913029503"

// Cảm biến MPU6050
MPU6050 mpu;

// GPS
TinyGPSPlus gps;

// Cảnh báo té ngã
const float fallThreshold = 30.0; // Ngưỡng jerk để phát hiện té ngã
const int sampleInterval = 100;   // Khoảng thời gian lấy mẫu (ms)

// Biến tính toán jerk
float prevAccX = 0, prevAccY = 0, prevAccZ = 0;
unsigned long lastTime = 0;

// Serial cho GPS và SIM
#define gpsSerial               Serial1
#define simSerial               Serial2

// Nút nhấn
#define BUTTON_PIN 15 // Chân kết nối nút nhấn
bool isButtonPressed = false;

// Bluetooth Serial
BluetoothSerial BTSerial;

void setup() {
    // Khởi tạo Bluetooth
    BTSerial.begin("ESP32_Fall_Detection"); // Tên thiết bị Bluetooth
    BTSerial.println("Bluetooth Serial initialized.");
    BTSerial.println("\nSystem initializing...");

    // Cài đặt nút nhấn
    pinMode(BUTTON_PIN, INPUT_PULLDOWN);

    // Khởi tạo SIM
    simSerial.begin(MCU_SIM_BAUDRATE, SERIAL_8N1, SIM_RX_PIN, SIM_TX_PIN);
    initSIM();

    // Khởi tạo GPS
    gpsSerial.begin(MCU_GPS_BAUDRATE, SERIAL_8N1, GPS_RX_PIN, GPS_TX_PIN);

    // Khởi tạo MPU6050
    Wire.begin();
    mpu.initialize();
    if (mpu.testConnection()) {
        BTSerial.println("MPU6050 connected successfully.");
    } else {
        BTSerial.println("MPU6050 connection failed!");
        while (1); // Dừng nếu không kết nối được
    }
}

void loop() {
    static unsigned long buttonPressTime = 0;

    // Kiểm tra trạng thái nút nhấn
    if (digitalRead(BUTTON_PIN) == HIGH && !isButtonPressed) {
        isButtonPressed = true;
        buttonPressTime = millis();
        BTSerial.println("Button pressed! Initiating emergency actions...");
        handleFallEvent();
    }

    // Reset trạng thái nút sau khi nhả
    if (isButtonPressed && (millis() - buttonPressTime > 1000) && digitalRead(BUTTON_PIN) == LOW) {
        isButtonPressed = false;
    }

    // Đọc dữ liệu từ MPU6050
    if (millis() - lastTime >= sampleInterval) {
        lastTime = millis();

        int16_t ax, ay, az;
        mpu.getAcceleration(&ax, &ay, &az);

        // Chuyển đổi gia tốc từ giá trị raw sang G
        float accX = ax / 16384.0;
        float accY = ay / 16384.0;
        float accZ = az / 16384.0;

        // Tính toán jerk
        float jerkX = (accX - prevAccX) / (sampleInterval / 1000.0);
        float jerkY = (accY - prevAccY) / (sampleInterval / 1000.0);
        float jerkZ = (accZ - prevAccZ) / (sampleInterval / 1000.0);

        prevAccX = accX;
        prevAccY = accY;
        prevAccZ = accZ;

        // Tính độ lớn jerk
        float jerkMagnitude = sqrt(jerkX * jerkX + jerkY * jerkY + jerkZ * jerkZ);
        BTSerial.print("Jerk Magnitude: ");
        BTSerial.println(jerkMagnitude);

        // Nếu phát hiện té ngã
        if (jerkMagnitude > fallThreshold) {
            BTSerial.println("Fall detected! Initiating emergency actions...");
            handleFallEvent();
            delay(30000); // Tránh gọi và gửi SMS liên tục
        }
    }

    // Đọc dữ liệu từ GPS
    while (gpsSerial.available() > 0) {
        char c = gpsSerial.read();
        gps.encode(c);
    }
}

void handleFallEvent() {
    // Đảm bảo tín hiệu GPS ổn định
    waitForGPSFix();

    // Thực hiện cuộc gọi cảnh báo
    makeCall();

    // Nếu có dữ liệu GPS, gửi SMS với vị trí
    if (gps.location.isUpdated()) {
        String gpsMessage = createGPSMessage();
        sendSMS(gpsMessage);
        BTSerial.println("SMS with GPS location sent.");
    } else {
        sendSMS("Fall detected but GPS location not available.");
        BTSerial.println("SMS sent: Fall detected without GPS location.");
    }
}

void initSIM() {
    simSerial.println("AT");
    delay(100);
    simSerial.println("AT+CMGF=1"); // Chuyển sang chế độ Text
    delay(100);
    BTSerial.println("SIM module initialized.");
}

void makeCall() {
    String callCmd = "ATD";
    callCmd += PHONE_NUMBER;
    callCmd += ";";
    simSerial.println(callCmd);
    BTSerial.println("Calling emergency contact...");
    delay(10000); // Gọi trong 10 giây
    simSerial.println("ATH"); // Ngắt cuộc gọi
    BTSerial.println("Call ended.");
}

String createGPSMessage() {
    String message = "Fall detected! Current location:\n";
    message += "Latitude: ";
    message += String(gps.location.lat(), 6);
    message += "\nLongitude: ";
    message += String(gps.location.lng(), 6);
    message += "\nGoogle Maps: https://www.google.com/maps?q=";
    message += String(gps.location.lat(), 6);
    message += ",";
    message += String(gps.location.lng(), 6);
    return message;
}

void sendSMS(String message) {
    simSerial.println("AT+CMGS=\"" + String(PHONE_NUMBER) + "\"");
    delay(100);
    simSerial.print(message);
    simSerial.write(26); // Ký tự kết thúc tin nhắn (Ctrl+Z)
    BTSerial.println("Sending SMS...");
    unsigned long timeout = millis();
    while (millis() - timeout < 10000) {
        if (simSerial.available()) {
            String response = simSerial.readString();
            if (response.indexOf("OK") != -1) {
                BTSerial.println("SMS sent successfully.");
                return;
            } else if (response.indexOf("ERROR") != -1) {
                BTSerial.println("Failed to send SMS.");
                return;
            }
        }
    }
    BTSerial.println("SMS send timeout.");
}

void waitForGPSFix() {
    BTSerial.println("Waiting for GPS fix...");
    unsigned long start = millis();
    while (!gps.location.isUpdated() && millis() - start < 30000) {
        while (gpsSerial.available() > 0) {
            char c = gpsSerial.read();
            gps.encode(c);
        }
        delay(100);
    }

    if (gps.location.isUpdated()) {
        BTSerial.println("GPS fix acquired.");
    } else {
        BTSerial.println("Failed to acquire GPS fix within timeout.");
    }
}
