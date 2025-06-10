#include <Arduino.h>
#include <SPIFFS.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <DFRobotDFPlayerMini.h>
#include <HardwareSerial.h>
// #include <NTPClient.h>
// #include <WiFiUdp.h>

#define DEFAULT_WIFI_SSID     "KoshkinDom"
#define DEFAULT_WIFI_PASSWORD "mousehouse555"

// Подключение DFPlayer Mini:
// ESP32 GPIO16 (RX2) -> DFPlayer Mini TX
// ESP32 GPIO17 (TX2) -> DFPlayer Mini RX
HardwareSerial dfSerial(2); // UART2: RX=16, TX=17
DFRobotDFPlayerMini dfPlayer;

String wifi_ssid = DEFAULT_WIFI_SSID;
String wifi_pass = DEFAULT_WIFI_PASSWORD;
AsyncWebServer server(80);

void readWiFiConfig() {
  File configFile = SPIFFS.open("/config.txt", FILE_READ);
  if (!configFile) {
    Serial.println("Не удалось открыть config.txt для чтения, используются значения по умолчанию");
    return;
  }
  while (configFile.available()) {
    String line = configFile.readStringUntil('\n');
    line.trim();
    if (line.startsWith("wifi_ssid=")) {
      wifi_ssid = line.substring(String("wifi_ssid=").length());
    } else if (line.startsWith("wifi_pass=")) {
      wifi_pass = line.substring(String("wifi_pass=").length());
    }
  }
  configFile.close();
  Serial.print("WiFi параметры из файла: SSID=");
  Serial.print(wifi_ssid);
  Serial.print(", PASS=");
  Serial.println(wifi_pass);
}

bool connectWiFi(int maxAttempts, int retryDelayMs) {
  WiFi.mode(WIFI_STA);
  WiFi.begin(wifi_ssid.c_str(), wifi_pass.c_str());
  Serial.print("Подключение к WiFi");
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < maxAttempts) {
    delay(retryDelayMs);
    Serial.print(".");
    attempts++;
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println();
    Serial.print("WiFi подключен. IP адрес: ");
    Serial.println(WiFi.localIP());
    return true;
  } else {
    Serial.println();
    Serial.println("Ошибка подключения к WiFi");
    return false;
  }
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  // Инициализация SPIFFS
  if (!SPIFFS.begin(true)) {
    Serial.println("Ошибка монтирования SPIFFS");
    while (1) delay(1000);
  } else {
    Serial.println("SPIFFS успешно смонтирован");
  }

  // Пример записи файла конфигурации (только если файла нет)
  if (!SPIFFS.exists("/config.txt")) {
    File configFile = SPIFFS.open("/config.txt", FILE_WRITE);
    if (configFile) {
      configFile.println("wifi_ssid=" DEFAULT_WIFI_SSID);
      configFile.println("wifi_pass=" DEFAULT_WIFI_PASSWORD);
      configFile.close();
      Serial.println("Конфиг записан по умолчанию");
    }
  }

  // Чтение параметров WiFi из файла
  readWiFiConfig();

  // Подключение к WiFi с повторными попытками
  int maxRetries = 5;
  int retryDelay = 2000; // 2 секунды
  int tries = 0;
  while (!connectWiFi(10, 500) && tries < maxRetries) {
    Serial.print("Повторная попытка подключения к WiFi (" + String(tries+1) + "/" + String(maxRetries) + ")\n");
    tries++;
    delay(3000);
  }
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Не удалось подключиться к WiFi после нескольких попыток. Перезагрузка...");
    delay(5000);
    ESP.restart();
  }

  // Инициализация DFPlayer Mini
  dfSerial.begin(9600, SERIAL_8N1, 16, 17); // RX=16, TX=17
  if (!dfPlayer.begin(dfSerial)) {
    Serial.println("DFPlayer Mini не найден!");
  } else {
    Serial.println("DFPlayer Mini готов!");
    dfPlayer.volume(20); // Громкость 0-30
    // dfPlayer.play(1);    // Воспроизвести первый трек на SD (убрано для ручного управления)
  }

  // Веб-форма для смены WiFi
  server.on("/wifi", HTTP_GET, [](AsyncWebServerRequest *request){
    String html = "<html><body><h2>Настройка WiFi</h2>"
      "<form method='POST' action='/api/wifi'>"
      "SSID: <input name='ssid' value='" + wifi_ssid + "'><br>"
      "Пароль: <input name='pass' type='password' value='" + wifi_pass + "'><br>"
      "<input type='submit' value='Сохранить'>"
      "</form></body></html>";
    request->send(200, "text/html", html);
  });

  // REST API для смены WiFi
  server.on("/api/wifi", HTTP_POST, [](AsyncWebServerRequest *request){
    String newSsid, newPass;
    if (request->hasParam("ssid", true)) newSsid = request->getParam("ssid", true)->value();
    if (request->hasParam("pass", true)) newPass = request->getParam("pass", true)->value();
    if (newSsid.length() > 0 && newPass.length() > 0) {
      File configFile = SPIFFS.open("/config.txt", FILE_WRITE);
      if (configFile) {
        configFile.println("wifi_ssid=" + newSsid);
        configFile.println("wifi_pass=" + newPass);
        configFile.close();
        request->send(200, "text/html", "<html><body>WiFi сохранён. Перезагрузка...</body></html>");
        delay(1000);
        ESP.restart();
      } else {
        request->send(500, "text/plain", "Ошибка записи файла конфигурации");
      }
    } else {
      request->send(400, "text/plain", "SSID и пароль обязательны");
    }
  });

  // --- REST API для управления DFPlayer Mini ---
  // Воспроизвести текущий трек
  server.on("/api/audio/play", HTTP_POST, [](AsyncWebServerRequest *request){
    dfPlayer.start();
    request->send(200, "application/json", "{\"status\":\"playing\"}");
  });
  // Остановить воспроизведение
  server.on("/api/audio/stop", HTTP_POST, [](AsyncWebServerRequest *request){
    dfPlayer.stop();
    request->send(200, "application/json", "{\"status\":\"stopped\"}");
  });
  // Установить громкость: /api/audio/volume?value=20
  server.on("/api/audio/volume", HTTP_POST, [](AsyncWebServerRequest *request){
    if (request->hasParam("value", true)) {
      int vol = request->getParam("value", true)->value().toInt();
      vol = constrain(vol, 0, 30);
      dfPlayer.volume(vol);
      request->send(200, "application/json", "{\"status\":\"ok\",\"volume\":" + String(vol) + "}");
    } else {
      request->send(400, "application/json", "{\"error\":\"volume required\"}");
    }
  });
  // Воспроизвести определённый трек: /api/audio/track?num=1
  server.on("/api/audio/track", HTTP_POST, [](AsyncWebServerRequest *request){
    if (request->hasParam("num", true)) {
      int num = request->getParam("num", true)->value().toInt();
      if (num > 0) {
        dfPlayer.play(num);
        request->send(200, "application/json", "{\"status\":\"playing\",\"track\":" + String(num) + "}");
      } else {
        request->send(400, "application/json", "{\"error\":\"invalid track number\"}");
      }
    } else {
      request->send(400, "application/json", "{\"error\":\"track number required\"}");
    }
  });
  // --- конец REST API DFPlayer ---

  server.begin();
  Serial.println("Веб-сервер запущен. Откройте /wifi для настройки WiFi.");

  // Для LittleFS используйте #include <LittleFS.h> и LittleFS.begin() вместо SPIFFS
}

void loop() {
  // TODO: Основная логика работы будильников
} 