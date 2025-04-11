#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>
#include <BH1750.h>
#include <WiFi.h>
#include <Wire.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>

Adafruit_BME280 bme280;

#define I2C_HUB_ADDR        0x70
#define EN_MASK             0x08
#define DEF_CHANNEL         0x00
#define MAX_CHANNEL         0x08

const char* ssid = "Wi-Fi"; // Замените на вашу сеть
const char* password = "password"; // Замените на ваш пароль
const char* serverIP = "192.168.66.240";
WiFiServer server(80);

int sensorValue_L75EN = 0;

BH1750 lightMeter;
String header;
bool logged1 = false;
bool logged2 = false;
HTTPClient http;
WiFiClient cliento;
long int timefuck = 0;
bool switchState = false;

void handleToggle() {
  switchState = !switchState; // Toggle the state
  Serial.print("Switch state: ");
  Serial.println(switchState ? "ON" : "OFF");
  
  // Redirect back to the main page
  server.sendHeader("Location", "/", true);
  server.send(302, "text/plain", ""); 
}

void setup() {
  Serial.begin(115200);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected.");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
  server.begin();
  Wire.begin();
  setBusChannel(0x04);
  lightMeter.begin();
  setBusChannel(0x04);
  bool bme_status = bme280.begin();
  if (!bme_status) {
    Serial.println("Не найден по адресу 0х77, пробую другой...");
    bme_status = bme280.begin(0x76);
    if (!bme_status)
      Serial.println("Датчик не найден, проверьте соединение");
  }
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected.");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
  server.on("/toggle", handleToggle);
  server.begin();
  pinMode(14, OUTPUT);

  delay(1000);
}

void sendHtml(WiFiClient& client) {
  client.println("HTTP/1.1 200 OK");
  client.println("Content-Type: text/html");
  client.println("Connection: close");
  client.println();

  client.println("<!DOCTYPE html><html>");
  client.println("<head>");
  client.println("<meta charset='UTF-8'>");
  client.println("<meta name='viewport' content='width=device-width, initial-scale=1.0'>");
  client.println("<title>ESP32 Sensor Data</title>");
  client.println("<style>");
  client.println("body { font-family: Arial, sans-serif; text-align: center; }");
  client.println(".login-form { width: 300px; margin: 50px auto; padding: 20px; border: 1px solid #ccc; border-radius: 5px; }");
  client.println("input[type=text], input[type=password] { width: 100%; padding: 10px; margin: 8px 0; display: inline-block; border: 1px solid #ccc; box-sizing: border-box; }");
  client.println("button { background-color: #4CAF50; color: white; padding: 10px 20px; margin: 8px 0; border: none; cursor: pointer; width: 100%; }");
  client.println("table { margin: 20px auto; border-collapse: collapse; }");
  client.println("th, td { border: 1px solid black; padding: 8px; }");
  client.println("</style>");
  client.println("</head>");
  client.println("<body>");
  client.println("<h1>ESP32 Sensor Data</h1>");

  if (!logged1 && !logged2) {
    client.println("<div id='loginForm' class='login-form'>");
    client.println("<h2>Login</h2>");
    client.println("<form id='login' method='post'>");
    client.println("<input type='text' id='username' name='username' placeholder='Username'>");
    client.println("<input type='password' id='password' name='password' placeholder='Password'>");
    client.println("<button type='submit'>Login</button>");
    client.println("</form>");
    client.println("</div>");
  } else if (logged1) {
    client.println("<table id='sensorData'>");
    client.println("<tr><th>Датчик</th><th>Значение</th></tr>");
    client.println("<tr><td>Уровень жидкости (MGS-L75EN)</td><td id='sensorValue_L75EN'>0</td></tr>");
    client.println("<tr><td>Освещенность (BH1750)</td><td id='lightLevel'>" + String(lightMeter.readLightLevel()) + " lux</td></tr>");
    client.println("<tr><td>Температура (BME280)</td><td id='temperature'>" + String(bme280.readTemperature()) + " °C</td></tr>");

    client.println("</table>");
    client.println("<h1>Switch is currently: " + String(switchState ? "ON" : "OFF") + "</h1>");
    client.println("<a href='/toggle'><button>Toggle Switch</button></a>");
    client.println("<script>setTimeout(function(){location.reload();}, 1500);</script>");
  } else if (logged2) {
    client.println("<table id='sensorData'>");
    client.println("<tr><th>Датчик</th><th>Значение</th></tr>");
    client.println("<tr><td>Уровень жидкости (MGS-L75EN)</td><td id='sensorValue_L75EN'>0</td></tr>");
    client.println("<tr><td>Влажность (BME280)</td><td id='humidity'>" + String(bme280.readHumidity()) + " %</td></tr>");
    client.println("<tr><td>Давление (BME280)</td><td id='pressure'>" + String(bme280.readPressure() / 100.0F) + " hPa</td></tr>");
    client.println("</table>");
    client.println("<script>setTimeout(function(){location.reload();}, 1500);</script>");
  }
  if (bme280.readTemperature() > 26) {
      client.println("<tr><td id='tempStatus' style=\"color: red\">ГОРЯЧО</td><td></td></tr>");
      client.println("<script>alert('ВЫ ГОРИТЕ!');</script>");
      for (int i = 0; i < 10; i++) Serial.println("Servo and led activated for 3 seconds.");
    } else {
      client.println("<tr><td id='tempStatus' style=\"color: black\">Нормальная температура</td><td></td></tr>");
    }

  client.println("</body>");
  client.println("</html>");
}

void handleLogin(WiFiClient& client) {
  String payload = "";
  while (client.available()) {
    payload += (char)client.read();
  }
  Serial.println("Received login request: " + payload);

  if (payload.indexOf("username=admin1&password=password") != -1) {
    logged1 = true;
    client.println("HTTP/1.1 302 Found");
    client.println("Location: /");
    client.println();
  } else if (payload.indexOf("username=admin2&password=password") != -1) {
    logged2 = true;
    client.println("HTTP/1.1 302 Found");
    client.println("Location: /");
    client.println();
  } else {
    client.println("HTTP/1.1 401 Unauthorized");
    client.println("Content-type:text/plain");
    client.println();
    client.println("Invalid credentials");
  }
}

bool setBusChannel(uint8_t i2c_channel) {
  if (i2c_channel >= MAX_CHANNEL) {
    return false;
  } else {
    Wire.beginTransmission(I2C_HUB_ADDR);
    Wire.write(i2c_channel | EN_MASK); // для микросхемы PCA9547
    // Wire.write(0x01 << i2c_channel); // Для микросхемы PW548A
    Wire.endTransmission();
    return true;
  }
}
void loop() {
  digitalWrite(14, switchState);
  int sensorValue = lightMeter.readLightLevel();
  http.begin(cliento, "http://" + String(serverIP) + "/sensor");
  WiFiClient client = server.available();
  if (client) {
    Serial.println("New Client.");
    String currentLine = "";
    while (client.connected()) {
      if (client.available()) {
        char c = client.read();
        Serial.write(c);
        header += c;
        if (c == '\n') {
          if (currentLine.length() == 0) {
            if (header.indexOf("GET /") >= 0) {
              sendHtml(client); // Вызываем функцию отправки HTML
            } else if (header.indexOf("POST /") >= 0) {
              handleLogin(client);
            }
            break;
          } else {
            currentLine = "";
          }
        } else if (c != '\r') {
          currentLine += c;
        }
      }
    }
    header = "";
    client.stop();
    Serial.println("Client disconnected.");
    Serial.println("");
  }
  if ((millis()-timefuck)%1000==0) {
    http.addHeader("Content-Type", "text/plain");
    if (bme280.readTemperature() > 26) {
      int httpCode = http.POST("1");
      if (httpCode > 0) {
        Serial.println("Value sent to server");
      } else {
        Serial.println("Failed to send value to server");
      }
      http.end();
      timefuck = millis();
    } else {
      int httpCode = http.POST("0");
      if (httpCode > 0) {
        Serial.println("Value sent to server");
      } else {
        Serial.println("Failed to send value to server");
      }
      http.end();
      timefuck = millis();
    }
}
}

