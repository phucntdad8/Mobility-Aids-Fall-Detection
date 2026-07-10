const int trigPin = 8; // Chân Trig của HC-SR04
const int echoPin = 7; // Chân Echo của HC-SR04

void setup() {
  Serial.begin(9600); // Khởi tạo giao tiếp Serial với baudrate 9600
  pinMode(trigPin, OUTPUT); // Đặt chân Trig là OUTPUT
  pinMode(echoPin, INPUT); // Đặt chân Echo là INPUT
}

void loop() {
  unsigned long duration; // Biến lưu thời gian
  int distance; // Biến lưu khoảng cách

  // Phát xung từ chân Trig
  digitalWrite(trigPin, LOW); // Tắt chân Trig
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH); // Phát xung
  delayMicroseconds(5); // Xung có độ dài 5 micro giây
  digitalWrite(trigPin, LOW); // Tắt chân Trig

  // Đo thời gian phản hồi ở chân Echo
  duration = pulseIn(echoPin, HIGH);

  // Tính khoảng cách (cm)
  distance = duration / 2 / 29.412;

  // In kết quả lên Serial Monitor
  Serial.print("Khoang cach: ");
  Serial.print(distance);
  Serial.println(" cm");

  delay(200); // Chờ 200ms trước khi đo lại
}