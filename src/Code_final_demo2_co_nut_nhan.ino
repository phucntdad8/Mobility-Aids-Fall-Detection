#include <Wire.h>
#include <MPU6050.h>
#include <TinyGPSPlus.h>
#include <HardwareSerial.h>
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
#define PHONE_NUMBER            "+84336217406"

// Cảm biến MPU6050
MPU6050 mpu;

// GPS
TinyGPSPlus gps;

// Cảnh báo té ngã
const float fallThreshold = 10.0; // Ngưỡng jerk để phát hiện té ngã
const int sampleInterval = 100;   // Khoảng thời gian lấy mẫu (ms)

// Biến tính toán jerk
float prevAccX = 0, prevAccY = 0, prevAccZ = 0;
unsigned long lastTime = 0;

// Serial cho GPS và SIM
#define gpsSerial               Serial1
#define simSerial               Serial2

// Nút nhấn
#define BUTTON_PIN 25 // Chân kết nối nút nhấn
bool isButtonPressed = false;

void setup() {
    Serial.begin(115200); // Serial chính để debug
    Serial.println("\nSystem initializing...");

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
        Serial.println("MPU6050 connected successfully.");
    } else {
        Serial.println("MPU6050 connection failed!");
        while (1); // Dừng nếu không kết nối được
    }
}

void loop() {
    // Kiểm tra trạng thái nút nhấn
    if (digitalRead(BUTTON_PIN) == HIGH && !isButtonPressed) {
        isButtonPressed = true;
        Serial.println("Button pressed! Initiating emergency actions...");
        handleFallEvent();
        delay(3000); // Chống rung (debounce)
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
        Serial.print("Jerk Magnitude: ");
        Serial.println(jerkMagnitude);

        // Nếu phát hiện té ngã
        if (jerkMagnitude > fallThreshold) {
            Serial.println("Fall detected! Initiating emergency actions...");
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
    // Thực hiện cuộc gọi cảnh báo
    makeCall();

    // Nếu có dữ liệu GPS, gửi SMS với vị trí
    if (gps.location.isUpdated()) {
        String gpsMessage = createGPSMessage();
        sendSMS(gpsMessage);
    } else {
        sendSMS("Fall detected but GPS location not available.");
    }
}

void initSIM() {
    simSerial.println("AT");
    delay(100);
    simSerial.println("AT+CMGF=1"); // Chuyển sang chế độ Text
    delay(100);
    Serial.println("SIM module initialized.");
}

void makeCall() {
    String callCmd = "ATD";
    callCmd += PHONE_NUMBER;
    callCmd += ";";
    simSerial.println(callCmd);
    Serial.println("Calling emergency contact...");
    delay(10000); // Gọi trong 10 giây
    simSerial.println("ATH"); // Ngắt cuộc gọi
    Serial.println("Call ended.");
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
    // Gửi lệnh AT để chuẩn bị gửi SMS
    simSerial.println("AT+CMGS=\"" + String(PHONE_NUMBER) + "\"");
    delay(100);
    
    // Gửi tin nhắn
    simSerial.print(message);
    delay(100);
    
    // Gửi ký tự CTRL+Z (26) để kết thúc tin nhắn
    simSerial.write(26); 
    Serial.println("Sending SMS...");

    // Đọc phản hồi từ SIM module
    String response = "";
    unsigned long timeout = millis();
    
    // Đọc dữ liệu trả về từ SIM module trong khoảng thời gian nhất định
    while (millis() - timeout < 5000) {
        if (simSerial.available()) {
            char c = simSerial.read();
            response += c;
            Serial.print(c);  // In ra dữ liệu nhận được từ SIM module
        }
    }

    // Kiểm tra xem phản hồi có chứa "OK" hoặc "+CMGS" (thành công)
    if (response.indexOf("+CMGS") != -1) {
        Serial.println("\nSMS sent successfully.");
    } else if (response.indexOf("OK") != -1) {
        Serial.println("\nSMS sent successfully.");
    } else if (response.indexOf("ERROR") != -1) {
        Serial.println("\nFailed to send SMS. Error occurred.");
    } else {
        Serial.println("\nUnexpected response: " + response);
    }
}
