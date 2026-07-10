#include <WiFi.h>
#include <TinyGPS++.h>

TinyGPSPlus gps;
// Define UART pins for GPS
#define GPS_RX 16 // RX pin of ESP32 connected to TX of GPS
#define GPS_TX 17 // TX pin of ESP32 connected to RX of GPS

HardwareSerial gpsSerial(1); // Use UART1 for GPS communication

const char* ssid = "Angel Cofee FPT T1";
const char* password = "camonquykhach";

float Latitude, Longitude;
int year, month, date, hour, minute, second;
String DateString, TimeString, LatitudeString, LongitudeString;

WiFiServer server(80);

void setup() {
  Serial.begin(115200);
  gpsSerial.begin(115200, SERIAL_8N1, GPS_RX, GPS_TX); // Initialize gpsSerial
  Serial.println();
  Serial.print("Connecting to WiFi");
  
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  
  Serial.println("");
  Serial.println("WiFi connected");
  server.begin();
  Serial.println("Server started");
  Serial.println(WiFi.localIP());
}

void loop() {
  // Remove the undefined signal variable
  // int signal = 42; // Example signal value (not used in your code)
  
  while (gpsSerial.available() > 0) {
    if (gps.encode(gpsSerial.read())) {
      if (gps.location.isValid()) {
        Latitude = gps.location.lat();
        LatitudeString = String(Latitude, 6);
        Longitude = gps.location.lng();
        LongitudeString = String(Longitude, 6);
      }

      if (gps.date.isValid()) {
        DateString = "";
        date = gps.date.day();
        month = gps.date.month();
        year = gps.date.year();

        if (date < 10) DateString += '0';
        DateString += String(date) + " / ";
        
        if (month < 10) DateString += '0';
        DateString += String(month) + " / ";

        DateString += String(year);
      }

      if (gps.time.isValid()) {
        TimeString = "";
        hour = gps.time.hour() + 7; // Adjust UTC
        minute = gps.time.minute();
        second = gps.time.second();

        if (hour < 10) TimeString += '0';
        TimeString += String(hour) + " : ";

        if (minute < 10) TimeString += '0';
        TimeString += String(minute) + " : ";

        if (second < 10) TimeString += '0';
        TimeString += String(second);
      }
    }
  }

  WiFiClient client = server.available();
  if (!client) {
    return;
  }

  // Response
  String s = "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n";
  s += "<!DOCTYPE html><html><head><title>L76x GPS Readings</title><style>";
  s += "table, th, td {border: 1px solid blue;} </style></head><body>";
  s += "<h1 style=\"font-size:300%; text-align:center;\">L76x GPS Readings</h1>";
  s += "<p style=\"text-align:center; font-size:150%;\"><b>Location Details</b></p>";
  s += "<table style=\"width:50%; margin:auto;\">";
  s += "<tr><th>Latitude</th><td>" + LatitudeString + "</td></tr>";
  s += "<tr><th>Longitude</th><td>" + LongitudeString + "</td></tr>";
  s += "<tr><th>Date</th><td>" + DateString + "</td></tr>";
  s += "<tr><th>Time</th><td>" + TimeString + "</td></tr>";
  s += "</table>";

  if (gps.location.isValid()) {
    s += "<p style=\"text-align:center;\"><a style=\"color:red; font-size:125%;\" href=\"http://maps.google.com/maps?&z=15&mrt=yp&t=k&q=";
    s += LatitudeString + "+" + LongitudeString + "\" target=\"_top\">Click here</a> to open the location in Google Maps.</p>";
  }

  s += "</body></html>\n";

  client.print(s);
  delay(100);
}