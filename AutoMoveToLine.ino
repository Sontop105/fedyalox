/*
В скетче к набору Динамика (моторной плате MGB-MDYN1.1)
подключен датчик линии MGS-LN19A
Динамика находит черную линию (сравнивая с остальным полотном),
затем - едет по ней
*/
#include <WiFi.h>
#include <Wire.h>

#include <Adafruit_PWMServoDriver.h>                          // библиотека для моторной платы
Adafruit_PWMServoDriver pwm = Adafruit_PWMServoDriver(0x70);  // адрес платы

#define sensor_addr 0x3F  // Переключатели адреса в положении "OFF"

int backLine = 1475; // Изначально как датчик видит черную линию (ориентир).
float speedDino = 40; // Скорость Динамики 
const char* ssid = "Wi-Fi";
const char* password = "password";
WiFiServer server(80); // Номер порта для сервера
String header;
int massSens[17];   // Массив для хранения показаний с сенсоров
byte digitData[17]; // Массив для хранения отцифрованных значений датчиков
// коэффициенты компенсации мощности моторов в зависимости от того, на каком датчике черная линия. Лучше всего располагать их в экспоненциальной прогрессии.
int Kf[] = { 120, 100, 90, 75, 65, 50, 25, 10, 0, -10, -25, -50, -65, -75, -90, -100, -120 };
int findKf[17]; // массив для хранения пропущенных через коэфициент значений с датчиков
int maxKf = 0, minKf = 0;
float Lkf, Rkf;

// переменные для первичного снятия показаний с сенсоров
static volatile int p00 = 0;
static volatile int p01 = 0;
static volatile int p02 = 0;
static volatile int p03 = 0;
static volatile int p04 = 0;
static volatile int p05 = 0;
static volatile int p06 = 0;
static volatile int p07 = 0;
static volatile int p08 = 0;
static volatile int p09 = 0;
static volatile int p10 = 0;
static volatile int p11 = 0;
static volatile int p12 = 0;
static volatile int p13 = 0;
static volatile int p14 = 0;
static volatile int p15 = 0;
static volatile int p16 = 0;
static volatile int p17 = 0;
static volatile int p18 = 0;
int count = 0; // nigga

// --- Добавляем глобальные переменные для поворотов ---
int turnCount = 0;                              // Счетчик поворотов
const int seqLength = 2;
char turnSequence[seqLength] = {'S', 'R'};  // L - налево, R - направо
bool isTurning = false; // Флаг для игнорирования линии во время поворота
int crossCount = 0; // Счетчик перекрестков

void setup() {
  // Инициализация последовательного порта
  Serial.begin(115200);
  // Инициализация датчика
  Wire.begin();
  Wire.setClock(100000L);
  delay(100);
  init_sensor();

  pwm.begin();
  // Частота (Гц)
  pwm.setPWMFreq(100);
  // Все порты выключены
  pwm.setPWM(8, 0, 4096);
  pwm.setPWM(9, 0, 4096);
  pwm.setPWM(10, 0, 4096);
  pwm.setPWM(11, 0, 4096);
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
  server.begin();

  delay(1000);
}
String payload;
void loop() {
  WiFiClient client = server.available();
   if (client) { // Если подключается новый клиент,
    Serial.println("New Client."); // выводим сообщение
    String currentLine = "";
    while (client.connected()) { // цикл, пока есть соединение клиента
      if (client.available()) { // если от клиента поступают данные,
        char c = client.read(); // читаем байт, затем
        Serial.write(c); // выводим на экран
        header += c;
        if (c == '\n') { // если байт является переводом строки
          // если пустая строка, мы получили два символа перевода строки
          // значит это конец HTTP-запроса, формируем ответ сервера:
          if (currentLine.length() == 0) {
            // HTTP заголовки начинаются с кода ответа (напр., HTTP / 1.1 200 OK)
            // и content-type, затем пустая строка:
            if (header.indexOf("POST /sensor") >= 0) {
                payload = "";
              while (client.available()) {
                char c = client.read();
                payload += c;
              }
              Serial.println("Received value: " + payload);
              client.println("HTTP/1.1 200 OK");
              client.println("Content-type:text/plain");
              client.println();
              client.println("Value received");
            }
            else {
              client.println("HTTP/1.1 200 OK");
              client.println("Content-type:text/html");
              client.println("Connection: close");
              client.println();
              client.println("<!DOCTYPE html><html>");
              client.println("<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">");
              client.println("<link rel=\"icon\" href=\"data:,\">");
              client.println("<style>html { font-family: Helvetica; display: inline-block; margin: 0px auto; text-align: center;}");
              client.println(".button { background-color: #4CAF50; border:  none; color: white; padding: 16px 40px;");
              client.println("text-decoration: none; font-size: 30px; margin: 2px; cursor: pointer;}");
              client.println(".button2 {background-color:#555555;}</style></head>");
              client.println("<body><h1>ESP32 Web Server</h1>");
              client.println("<p>Sensor Value: " + payload + "</p>");
              client.println("<script>");
              client.println("setInterval(() => {");
              client.println("location.reload();");
              client.println("}, 1000);");
              client.println("</script>");
              client.println("</body></html>");
              client.println();
            }
            break;
          } else { // если получили новую строку, очищаем currentLine
            currentLine = "";
          }
        } else if (c != '\r') { // Если получили что-то ещё кроме возврата строки,
          currentLine += c; // добавляем в конец currentLine
        }
      }
    }
    header = "";
    // Закрываем соединение
    client.stop();
    Serial.println("Client disconnected.");
    Serial.println("");
   }
  if (payload == "1") {
    if (!isTurning) {
      poll_sensor();
      digitSensor();
      addKf();
    
      if (-minKf == maxKf) { // Едем прямо
        motorA_setpower(speedDino, true);
        motorB_setpower(speedDino, true);
      } else {
        maxKf = maxKf < 0 ? 0 : maxKf;
        minKf = minKf > 0 ? 0 : minKf;
        Lkf = speedDino / 100 * (float)minKf;
        Rkf = speedDino / 100 * (float)maxKf;
        motorB_setpower(speedDino - Rkf, true);
        motorA_setpower(speedDino + Lkf, true);
      }
  }
  delay(1000);
  // Если isTurning истинно, пропускаем обновление сенсоров
    }
  }


void executeTurnSequence() {
  isTurning = true; // Начало поворота: игнорируем линию
  char command = turnSequence[turnCount % seqLength]; // Выбор направления по счетчику поворотов
  if (command == 'L') {
    turnLeft();
  } else if (command == 'R') {
    turnRight();
  } else { // 'S'
    moveStraight();
  }
  isTurning = false; // Конец поворота: возвращаем управление по линии
}

// --- Функция проверки перекрестка: все 17 датчиков одинаковы ---
bool isCross() {
    // Check if all sensors show the same value.
    bool allSame = true;
    for (int i = 1; i < 17; i++) {
        if (digitData[i] != digitData[0]) {
            allSame = false;
            break;
            
        }
    }

    // Check for T-intersection on the left side: sensors 0-5 all detect black (1)
    bool leftGroup = true;
    for (int i = 0; i <= 8; i++) {
        if (digitData[i] != 1) {
            leftGroup = false;
            break;
        }
    }

    // Check for T-intersection on the right side: using sensors 11-16 (which corresponds to 12-17 in 1-indexed notation)
    bool rightGroup = true;
    for (int i = 8; i < 17; i++) {
        if (digitData[i] != 1) {
            rightGroup = false;
            break;
        }
    }

    return allSame || leftGroup || rightGroup;
}

// --- Функция для поворота налево ---
void turnLeft() {
    // Левый мотор назад, правый мотор вперед
    motorA_setpower(-speedDino, true);
    motorB_setpower(speedDino, true);
    delay(600); // время для выполнения поворота
    turnCount++;
}

// --- Функция для поворота направо ---
void turnRight() {
    // Левый мотор вперед, правый мотор назад
    motorA_setpower(speedDino, true);
    motorB_setpower(-speedDino, true);
    delay(600); // время для выполнения поворота
    turnCount++;
}

// --- Функция для движения прямо ---
void moveStraight() {
    motorA_setpower(speedDino, true);
    motorB_setpower(speedDino, true);
    delay(300); // время движения
}

// --- Функция выполнения записанной последовательности команд ---
#if 0  // Removed duplicate executeTurnSequence() definition
void executeTurnSequence() {
    for (int i = 0; i < seqLength; i++) {
        if (turnSequence[i] == 'L') {
            turnLeft();
        } else if (turnSequence[i] == 'R') {
            turnRight();
        } else { // 'S'
            moveStraight();
        }
        delay(100); // короткая задержка между командами
    }
}
#endif

void serial() {
  Serial.print(p00);
  Serial.print(" ");
  Serial.print(p01);
  Serial.print(" ");
  Serial.print(p02);
  Serial.print(" ");
  Serial.print(p03);
  Serial.print(" ");
  Serial.print(p04);
  Serial.print(" ");
  Serial.print(p05);
  Serial.print(" ");
  Serial.print(p06);
  Serial.print(" ");
  Serial.print(p07);
  Serial.print(" ");
  Serial.print(p08);
  Serial.print(" ");
  Serial.print(p09);
  Serial.print(" ");
  Serial.print(p10);
  Serial.print(" ");
  Serial.print(p11);
  Serial.print(" ");
  Serial.print(p12);
  Serial.print(" ");
  Serial.print(p13);
  Serial.print(" ");
  Serial.print(p14);
  Serial.print(" ");
  Serial.print(p15);
  Serial.print(" ");
  Serial.print(p16);
  Serial.print(" ");
  Serial.print(p17);
  Serial.print(" ");
  Serial.print(p18);
  Serial.println();
  delay(10);
}

void digitSensor() {
  int l = Targetline();
  digitData[0] = p01 > l ? 1 : 0;
  digitData[1] = p02 > l ? 1 : 0;
  digitData[2] = p03 > l ? 1 : 0;
  digitData[3] = p04 > l ? 1 : 0;
  digitData[4] = p05 > l ? 1 : 0;
  digitData[5] = p06 > l ? 1 : 0;
  digitData[6] = p07 > l ? 1 : 0;
  digitData[7] = p08 > l ? 1 : 0;
  digitData[8] = p09 > l ? 1 : 0;
  digitData[9] = p10 > l ? 1 : 0;
  digitData[10] = p11 > l ? 1 : 0;
  digitData[11] = p12 > l ? 1 : 0;
  digitData[12] = p13 > l ? 1 : 0;
  digitData[13] = p14 > l ? 1 : 0;
  digitData[14] = p15 > l ? 1 : 0;
  digitData[15] = p16 > l ? 1 : 0;
  digitData[16] = p17 > l ? 1 : 0;
}

void addKf() {
  maxKf = 0;
  minKf = 0;
  for (int i = 0; i < 17; i++) {
    findKf[i] = digitData[i] * Kf[i];
    maxKf = max(maxKf, findKf[i]);
    minKf = min(minKf, findKf[i]);
  }
}

// Инициализация датчика
void init_sensor() {
  Wire.beginTransmission(sensor_addr);
  Wire.write(0x10);        // Регистр настройки всей микросхемы
  Wire.write(0b00000000);  // Нормальный режим работы
  Wire.write(0b01001111);  // АЦП в непрерывном режиме, 200 ksps, встроенный ИОН для ЦАП
  Wire.endTransmission();
  delay(1000);
  Wire.beginTransmission(sensor_addr);
  Wire.write(0x20);        // Регистр настройки порта 0 (подключен к оптическому сенсору)
  Wire.write(0b00000000);  // Сброс конфигурации порта
  Wire.write(0b00000000);
  Wire.endTransmission();
  Wire.beginTransmission(sensor_addr);
  Wire.write(0x21);        // Регистр настройки порта 1 (подключен к оптическому сенсору)
  Wire.write(0b00000000);  // Сброс конфигурации порта
  Wire.write(0b00000000);
  Wire.endTransmission();
  Wire.beginTransmission(sensor_addr);
  Wire.write(0x22);        // Регистр настройки порта 2 (подключен к оптическому сенсору)
  Wire.write(0b00000000);  // Сброс конфигурации порта
  Wire.write(0b00000000);
  Wire.endTransmission();
  Wire.beginTransmission(sensor_addr);
  Wire.write(0x23);        // Регистр настройки порта 3 (подключен к оптическому сенсору)
  Wire.write(0b00000000);  // Сброс конфигурации порта
  Wire.write(0b00000000);
  Wire.endTransmission();
  Wire.beginTransmission(sensor_addr);
  Wire.write(0x24);        // Регистр настройки порта 4 (подключен к оптическому сенсору)
  Wire.write(0b00000000);  // Сброс конфигурации порта
  Wire.write(0b00000000);
  Wire.endTransmission();
  Wire.beginTransmission(sensor_addr);
  Wire.write(0x25);        // Регистр настройки порта 5 (подключен к оптическому сенсору)
  Wire.write(0b00000000);  // Сброс конфигурации порта
  Wire.write(0b00000000);
  Wire.endTransmission();
  Wire.beginTransmission(sensor_addr);
  Wire.write(0x26);        // Регистр настройки порта 6 (подключен к оптическому сенсору)
  Wire.write(0b00000000);  // Сброс конфигурации порта
  Wire.write(0b00000000);
  Wire.endTransmission();
  Wire.beginTransmission(sensor_addr);
  Wire.write(0x27);        // Регистр настройки порта 7 (подключен к оптическому сенсору)
  Wire.write(0b00000000);  // Сброс конфигурации порта
  Wire.write(0b00000000);
  Wire.endTransmission();
  Wire.beginTransmission(sensor_addr);
  Wire.write(0x28);        // Регистр настройки порта 8 (подключен к оптическому сенсору)
  Wire.write(0b00000000);  // Сброс конфигурации порта
  Wire.write(0b00000000);
  Wire.endTransmission();
  Wire.beginTransmission(sensor_addr);
  Wire.write(0x29);        // Регистр настройки порта 9 (подключен к оптическому сенсору)
  Wire.write(0b00000000);  // Сброс конфигурации порта
  Wire.write(0b00000000);
  Wire.endTransmission();
  Wire.beginTransmission(sensor_addr);
  Wire.write(0x2A);        // Регистр настройки порта 10 (подключен к оптическому сенсору)
  Wire.write(0b00000000);  // Сброс конфигурации порта
  Wire.write(0b00000000);
  Wire.endTransmission();
  Wire.beginTransmission(sensor_addr);
  Wire.write(0x2B);        // Регистр настройки порта 11 (подключен к оптическому сенсору)
  Wire.write(0b00000000);  // Сброс конфигурации порта
  Wire.write(0b00000000);
  Wire.endTransmission();
  Wire.beginTransmission(sensor_addr);
  Wire.write(0x2C);        // Регистр настройки порта 12 (подключен к оптическому сенсору)
  Wire.write(0b00000000);  // Сброс конфигурации порта
  Wire.write(0b00000000);
  Wire.endTransmission();
  Wire.beginTransmission(sensor_addr);
  Wire.write(0x2D);        // Регистр настройки порта 13 (подключен к оптическому сенсору)
  Wire.write(0b00000000);  // Сброс конфигурации порта
  Wire.write(0b00000000);
  Wire.endTransmission();
  Wire.beginTransmission(sensor_addr);
  Wire.write(0x2E);        // Регистр настройки порта 14 (подключен к оптическому сенсору)
  Wire.write(0b00000000);  // Сброс конфигурации порта
  Wire.write(0b00000000);
  Wire.endTransmission();
  Wire.beginTransmission(sensor_addr);
  Wire.write(0x2F);        // Регистр настройки порта 15 (подключен к оптическому сенсору)
  Wire.write(0b00000000);  // Сброс конфигурации порта
  Wire.write(0b00000000);
  Wire.endTransmission();
  Wire.beginTransmission(sensor_addr);
  Wire.write(0x30);        // Регистр настройки порта 16 (подключен к оптическому сенсору)
  Wire.write(0b00000000);  // Сброс конфигурации порта
  Wire.write(0b00000000);
  Wire.endTransmission();
  Wire.beginTransmission(sensor_addr);
  Wire.write(0x31);        // Регистр настройки порта 17 (подключен к оптическому сенсору)
  Wire.write(0b00000000);  // Сброс конфигурации порта
  Wire.write(0b00000000);
  Wire.endTransmission();
  Wire.beginTransmission(sensor_addr);
  Wire.write(0x32);        // Регистр настройки порта 18 (подключен к оптическому сенсору)
  Wire.write(0b00000000);  // Сброс конфигурации порта
  Wire.write(0b00000000);
  Wire.endTransmission();
  delay(1000);
  Wire.beginTransmission(sensor_addr);
  Wire.write(0x20);        // Регистр настройки порта 0 (подключен к оптическому сенсору)
  Wire.write(0b01110001);  // Диапазон входного напряжения 0 ... 10 В, встроенный ИОН, порт в режиме входа АЦП
  Wire.write(0b11100000);  // Порт не ассоциирован с другим портом, количество выборок АЦП - 128
  Wire.endTransmission();
  Wire.beginTransmission(sensor_addr);
  Wire.write(0x21);        // Регистр настройки порта 1 (подключен к оптическому сенсору)
  Wire.write(0b01110001);  // Диапазон входного напряжения 0 ... 10 В, встроенный ИОН, порт в режиме входа АЦП
  Wire.write(0b11100000);  // Порт не ассоциирован с другим портом, количество выборок АЦП - 128
  Wire.endTransmission();
  Wire.beginTransmission(sensor_addr);
  Wire.write(0x22);        // Регистр настройки порта 2 (подключен к оптическому сенсору)
  Wire.write(0b01110001);  // Диапазон входного напряжения 0 ... 10 В, встроенный ИОН, порт в режиме входа АЦП
  Wire.write(0b11100000);  // Порт не ассоциирован с другим портом, количество выборок АЦП - 128
  Wire.endTransmission();
  Wire.beginTransmission(sensor_addr);
  Wire.write(0x23);        // Регистр настройки порта 3 (подключен к оптическому сенсору)
  Wire.write(0b01110001);  // Диапазон входного напряжения 0 ... 10 В, встроенный ИОН, порт в режиме входа АЦП
  Wire.write(0b11100000);  // Порт не ассоциирован с другим портом, количество выборок АЦП - 128
  Wire.endTransmission();
  Wire.beginTransmission(sensor_addr);
  Wire.write(0x24);        // Регистр настройки порта 4 (подключен к оптическому сенсору)
  Wire.write(0b01110001);  // Диапазон входного напряжения 0 ... 10 В, встроенный ИОН, порт в режиме входа АЦП
  Wire.write(0b11100000);  // Порт не ассоциирован с другим портом, количество выборок АЦП - 128
  Wire.endTransmission();
  Wire.beginTransmission(sensor_addr);
  Wire.write(0x25);        // Регистр настройки порта 5 (подключен к оптическому сенсору)
  Wire.write(0b01110001);  // Диапазон входного напряжения 0 ... 10 В, встроенный ИОН, порт в режиме входа АЦП
  Wire.write(0b11100000);  // Порт не ассоциирован с другим портом, количество выборок АЦП - 128
  Wire.endTransmission();
  Wire.beginTransmission(sensor_addr);
  Wire.write(0x26);        // Регистр настройки порта 6 (подключен к оптическому сенсору)
  Wire.write(0b01110001);  // Диапазон входного напряжения 0 ... 10 В, встроенный ИОН, порт в режиме входа АЦП
  Wire.write(0b11100000);  // Порт не ассоциирован с другим портом, количество выборок АЦП - 128
  Wire.endTransmission();
  Wire.beginTransmission(sensor_addr);
  Wire.write(0x27);        // Регистр настройки порта 7 (подключен к оптическому сенсору)
  Wire.write(0b01110001);  // Диапазон входного напряжения 0 ... 10 В, встроенный ИОН, порт в режиме входа АЦП
  Wire.write(0b11100000);  // Порт не ассоциирован с другим портом, количество выборок АЦП - 128
  Wire.endTransmission();
  Wire.beginTransmission(sensor_addr);
  Wire.write(0x28);        // Регистр настройки порта 8 (подключен к оптическому сенсору)
  Wire.write(0b01110001);  // Диапазон входного напряжения 0 ... 10 В, встроенный ИОН, порт в режиме входа АЦП
  Wire.write(0b11100000);  // Порт не ассоциирован с другим портом, количество выборок АЦП - 128
  Wire.endTransmission();
  Wire.beginTransmission(sensor_addr);
  Wire.write(0x29);        // Регистр настройки порта 9 (подключен к оптическому сенсору)
  Wire.write(0b01110001);  // Диапазон входного напряжения 0 ... 10 В, встроенный ИОН, порт в режиме входа АЦП
  Wire.write(0b11100000);  // Порт не ассоциирован с другим портом, количество выборок АЦП - 128
  Wire.endTransmission();
  Wire.beginTransmission(sensor_addr);
  Wire.write(0x2A);        // Регистр настройки порта 10 (подключен к оптическому сенсору)
  Wire.write(0b01110001);  // Диапазон входного напряжения 0 ... 10 В, встроенный ИОН, порт в режиме входа АЦП
  Wire.write(0b11100000);  // Порт не ассоциирован с другим портом, количество выборок АЦП - 128
  Wire.endTransmission();
  Wire.beginTransmission(sensor_addr);
  Wire.write(0x2B);        // Регистр настройки порта 11 (подключен к оптическому сенсору)
  Wire.write(0b01110001);  // Диапазон входного напряжения 0 ... 10 В, встроенный ИОН, порт в режиме входа АЦП
  Wire.write(0b11100000);  // Порт не ассоциирован с другим портом, количество выборок АЦП - 128
  Wire.endTransmission();
  Wire.beginTransmission(sensor_addr);
  Wire.write(0x2C);        // Регистр настройки порта 12 (подключен к оптическому сенсору)
  Wire.write(0b01110001);  // Диапазон входного напряжения 0 ... 10 В, встроенный ИОН, порт в режиме входа АЦП
  Wire.write(0b11100000);  // Порт не ассоциирован с другим портом, количество выборок АЦП - 128
  Wire.endTransmission();
  Wire.beginTransmission(sensor_addr);
  Wire.write(0x2D);        // Регистр настройки порта 13 (подключен к оптическому сенсору)
  Wire.write(0b01110001);  // Диапазон входного напряжения 0 ... 10 В, встроенный ИОН, порт в режиме входа АЦП
  Wire.write(0b11100000);  // Порт не ассоциирован с другим портом, количество выборок АЦП - 128
  Wire.endTransmission();
  Wire.beginTransmission(sensor_addr);
  Wire.write(0x2E);        // Регистр настройки порта 14 (подключен к оптическому сенсору)
  Wire.write(0b01110001);  // Диапазон входного напряжения 0 ... 10 В, встроенный ИОН, порт в режиме входа АЦП
  Wire.write(0b11100000);  // Порт не ассоциирован с другим портом, количество выборок АЦП - 128
  Wire.endTransmission();
  Wire.beginTransmission(sensor_addr);
  Wire.write(0x2F);        // Регистр настройки порта 15 (подключен к оптическому сенсору)
  Wire.write(0b01110001);  // Диапазон входного напряжения 0 ... 10 В, встроенный ИОН, порт в режиме входа АЦП
  Wire.write(0b11100000);  // Порт не ассоциирован с другим портом, количество выборок АЦП - 128
  Wire.endTransmission();
  Wire.beginTransmission(sensor_addr);
  Wire.write(0x30);        // Регистр настройки порта 16 (подключен к оптическому сенсору)
  Wire.write(0b01110001);  // Диапазон входного напряжения 0 ... 10 В, встроенный ИОН, порт в режиме входа АЦП
  Wire.write(0b11100000);  // Порт не ассоциирован с другим портом, количество выборок АЦП - 128
  Wire.endTransmission();
  Wire.beginTransmission(sensor_addr);
  Wire.write(0x31);        // Регистр настройки порта 17 (подключен к оптическому сенсору)
  Wire.write(0b01110001);  // Диапазон входного напряжения 0 ... 10 В, встроенный ИОН, порт в режиме входа АЦП
  Wire.write(0b11100000);  // Порт не ассоциирован с другим портом, количество выборок АЦП - 128
  Wire.endTransmission();
  Wire.beginTransmission(sensor_addr);
  Wire.write(0x32);        // Регистр настройки порта 18 (подключен к оптическому сенсору)
  Wire.write(0b01111001);  // Диапазон входного напряжения 0 ... 10 В, встроенный ИОН, порт в режиме входа АЦП
  Wire.write(0b11100000);  // Порт не ассоциирован с другим портом, количество выборок АЦП - 128
  Wire.endTransmission();
  delay(1000);
  // Отладка регистров
  /*
    int a = 0;
    int b = 0;
    Wire.beginTransmission(sensor_addr);
    Wire.write(0x45); // Регистр данных АЦП
    Wire.endTransmission();
    Wire.requestFrom(sensor_addr, 2);
    Serial.println(Wire.available());
    a = Wire.read();
    b = Wire.read();
    Serial.println(String(a, 2));
    Serial.println(String(b, 2));
  */
}

// Получение данных с датчика
void poll_sensor() {
  int adc_sensor_data[38] = { 0 };
  Wire.beginTransmission(sensor_addr);
  Wire.write(0x40);  // Регистр данных АЦП
  Wire.endTransmission();
  Wire.requestFrom(sensor_addr, 10);
  if (Wire.available() == 10) {
    adc_sensor_data[0] = Wire.read();  // ADC00
    adc_sensor_data[1] = Wire.read();
    adc_sensor_data[2] = Wire.read();  // ADC01
    adc_sensor_data[3] = Wire.read();
    adc_sensor_data[4] = Wire.read();  // ADC02
    adc_sensor_data[5] = Wire.read();
    adc_sensor_data[6] = Wire.read();  // ADC03
    adc_sensor_data[7] = Wire.read();
    adc_sensor_data[8] = Wire.read();  // ADC04
    adc_sensor_data[9] = Wire.read();
  }
  Wire.beginTransmission(sensor_addr);
  Wire.write(0x45);  // Регистр данных АЦП
  Wire.endTransmission();
  Wire.requestFrom(sensor_addr, 10);
  if (Wire.available() == 10) {
    adc_sensor_data[10] = Wire.read();  // ADC05
    adc_sensor_data[11] = Wire.read();
    adc_sensor_data[12] = Wire.read();  // ADC06
    adc_sensor_data[13] = Wire.read();
    adc_sensor_data[14] = Wire.read();  // ADC07
    adc_sensor_data[15] = Wire.read();
    adc_sensor_data[16] = Wire.read();  // ADC08
    adc_sensor_data[17] = Wire.read();
    adc_sensor_data[18] = Wire.read();  // ADC09
    adc_sensor_data[19] = Wire.read();
  }
  Wire.beginTransmission(sensor_addr);
  Wire.write(0x4A);  // Регистр данных АЦП
  Wire.endTransmission();
  Wire.requestFrom(sensor_addr, 10);
  if (Wire.available() == 10) {
    adc_sensor_data[20] = Wire.read();  // ADC10
    adc_sensor_data[21] = Wire.read();
    adc_sensor_data[22] = Wire.read();  // ADC11
    adc_sensor_data[23] = Wire.read();
    adc_sensor_data[24] = Wire.read();  // ADC12
    adc_sensor_data[25] = Wire.read();
    adc_sensor_data[26] = Wire.read();  // ADC13
    adc_sensor_data[27] = Wire.read();
    adc_sensor_data[28] = Wire.read();  // ADC14
    adc_sensor_data[29] = Wire.read();
  }
  Wire.beginTransmission(sensor_addr);
  Wire.write(0x4F);  // Регистр данных АЦП
  Wire.endTransmission();
  Wire.requestFrom(sensor_addr, 8);
  if (Wire.available() == 8) {
    adc_sensor_data[30] = Wire.read();  // ADC15
    adc_sensor_data[31] = Wire.read();
    adc_sensor_data[32] = Wire.read();  // ADC16
    adc_sensor_data[33] = Wire.read();
    adc_sensor_data[34] = Wire.read();  // ADC17
    adc_sensor_data[35] = Wire.read();
    adc_sensor_data[36] = Wire.read();  // ADC18
    adc_sensor_data[37] = Wire.read();
  }
  p00 = adc_sensor_data[36] * 256 + adc_sensor_data[37];
  p01 = adc_sensor_data[34] * 256 + adc_sensor_data[35];
  p02 = adc_sensor_data[32] * 256 + adc_sensor_data[33];
  p03 = adc_sensor_data[30] * 256 + adc_sensor_data[31];
  p04 = adc_sensor_data[28] * 256 + adc_sensor_data[29];
  p05 = adc_sensor_data[26] * 256 + adc_sensor_data[27];
  p06 = adc_sensor_data[24] * 256 + adc_sensor_data[25];
  p07 = adc_sensor_data[22] * 256 + adc_sensor_data[23];
  p08 = adc_sensor_data[20] * 256 + adc_sensor_data[21];
  p09 = adc_sensor_data[18] * 256 + adc_sensor_data[19];
  p10 = adc_sensor_data[16] * 256 + adc_sensor_data[17];
  p11 = adc_sensor_data[14] * 256 + adc_sensor_data[15];
  p12 = adc_sensor_data[12] * 256 + adc_sensor_data[13];
  p13 = adc_sensor_data[10] * 256 + adc_sensor_data[11];
  p14 = adc_sensor_data[8] * 256 + adc_sensor_data[9];
  p15 = adc_sensor_data[6] * 256 + adc_sensor_data[7];
  p16 = adc_sensor_data[4] * 256 + adc_sensor_data[5];
  p17 = adc_sensor_data[2] * 256 + adc_sensor_data[3];
  p18 = adc_sensor_data[0] * 256 + adc_sensor_data[1];

  massSens[0] = p01;
  massSens[1] = p02;
  massSens[2] = p03;
  massSens[3] = p04;
  massSens[4] = p05;
  massSens[5] = p06;
  massSens[6] = p07;
  massSens[7] = p08;
  massSens[8] = p09;
  massSens[9] = p10;
  massSens[10] = p11;
  massSens[11] = p12;
  massSens[12] = p13;
  massSens[13] = p14;
  massSens[14] = p15;
  massSens[15] = p16;
  massSens[16] = p17;
}

// Функция поиска минимального значения из всех датчиков. 
int Targetline() { 
  int l = 0;
  for (int i = 0; i < 17; i++) {
    l = max(l, massSens[i]);
  }
  if (l == 0) l = 1475;
  l -= 20;
  return l;
}

// Мощность мотора "A" от -100% до +100% (от знака зависит направление вращения)
void motorA_setpower(float pwr, bool invert) {
  // Проверка, инвертирован ли мотор
  if (invert) {
    pwr = -pwr;
  }
  // Проверка диапазонов
  if (pwr < -100) {
    pwr = -100;
  }
  if (pwr > 100) {
    pwr = 100;
  }
  int pwmvalue = fabs(pwr) * 40.95;
  if (pwr < 0) {
    pwm.setPWM(10, 0, 4096);
    pwm.setPWM(11, 0, pwmvalue);
  } else {
    pwm.setPWM(11, 0, 4096);
    pwm.setPWM(10, 0, pwmvalue);
  }
}

// Мощность мотора "B" от -100% до +100% (от знака зависит направление вращения)
void motorB_setpower(float pwr, bool invert) {
  // Проверка, инвертирован ли мотор
  if (!invert) {
    pwr = -pwr;
  }
  // Проверка диапазонов
  if (pwr < -100) {
    pwr = -100;
  }
  if (pwr > 100) {
    pwr = 100;
  }
  int pwmvalue = fabs(pwr) * 40.95;
  if (pwr < 0) {
    pwm.setPWM(8, 0, 4096);
    pwm.setPWM(9, 0, pwmvalue);
  } else {
    pwm.setPWM(9, 0, 4096);
    pwm.setPWM(8, 0, pwmvalue);
  }
}
