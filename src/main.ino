#include <Arduino.h>
#include <SPIFFS.h>
#include <WiFi.h>
#include <WebServer.h>
#include <WiFiClient.h>
#include <DFRobotDFPlayerMini.h>
#include <HardwareSerial.h>
#include <esp_task_wdt.h>
#include <ArduinoJson.h>
#include <time.h>
// #include <NTPClient.h>
// #include <WiFiUdp.h>

#define DEFAULT_WIFI_SSID     "ASUS"
#define DEFAULT_WIFI_PASSWORD "password"

// Подключение DFPlayer Mini:
// ESP32 GPIO16 (RX2) -> DFPlayer Mini TX
// ESP32 GPIO17 (TX2) -> DFPlayer Mini RX
HardwareSerial dfSerial(2); // UART2: RX=16, TX=17
DFRobotDFPlayerMini dfPlayer;

String wifi_ssid = DEFAULT_WIFI_SSID;
String wifi_pass = DEFAULT_WIFI_PASSWORD;
WebServer server(80);

// Структура будильника
struct Alarm {
  String time;
  std::vector<int> days;
  int track;
  int volume;
  String label;
  bool enabled;
  time_t nextTrigger;
};

std::vector<Alarm> alarms;
const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 0;
const int daylightOffset_sec = 3600;

// Task handles for watchdog management
TaskHandle_t mainTaskHandle = NULL;
TaskHandle_t webServerTaskHandle = NULL;

// Web server task function
void webServerTask(void *parameter) {
  for(;;) {
    // Reset watchdog timer for this task
    esp_task_wdt_reset();
    
    // Small delay to prevent watchdog from triggering
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}

// Main application task function
void mainTask(void *parameter) {
  for(;;) {
    // Reset watchdog timer for this task
    esp_task_wdt_reset();
    
    // Main application logic can go here
    // For now, just a delay
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}

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

// Функции для работы с будильниками
void loadAlarms() {
  File file = SPIFFS.open("/alarms.json", "r");
  if (!file) {
    Serial.println("Файл будильников не найден, создаем новый");
    return;
  }
  
  DynamicJsonDocument doc(2048);
  DeserializationError error = deserializeJson(doc, file);
  file.close();
  
  if (error) {
    Serial.println("Ошибка чтения файла будильников");
    return;
  }
  
  alarms.clear();
  JsonArray alarmsArray = doc["alarms"];
  for (JsonObject alarmObj : alarmsArray) {
    Alarm alarm;
    alarm.time = alarmObj["time"].as<String>();
    alarm.track = alarmObj["track"] | 1;
    alarm.volume = alarmObj["volume"] | 20;
    alarm.label = alarmObj["label"].as<String>();
    alarm.enabled = alarmObj["enabled"] | true;
    
    JsonArray daysArray = alarmObj["days"];
    for (int day : daysArray) {
      alarm.days.push_back(day);
    }
    
    alarms.push_back(alarm);
  }
  
  Serial.printf("Загружено %d будильников\n", alarms.size());
}

void saveAlarms() {
  DynamicJsonDocument doc(2048);
  JsonArray alarmsArray = doc.createNestedArray("alarms");
  
  for (const Alarm& alarm : alarms) {
    JsonObject alarmObj = alarmsArray.createNestedObject();
    alarmObj["time"] = alarm.time;
    alarmObj["track"] = alarm.track;
    alarmObj["volume"] = alarm.volume;
    alarmObj["label"] = alarm.label;
    alarmObj["enabled"] = alarm.enabled;
    
    JsonArray daysArray = alarmObj.createNestedArray("days");
    for (int day : alarm.days) {
      daysArray.add(day);
    }
  }
  
  File file = SPIFFS.open("/alarms.json", "w");
  if (file) {
    serializeJson(doc, file);
    file.close();
    Serial.println("Будильники сохранены");
  } else {
    Serial.println("Ошибка сохранения будильников");
  }
}

void checkAlarms() {
  if (!time(nullptr)) return; // Время не синхронизировано
  
  time_t now = time(nullptr);
  struct tm* timeinfo = localtime(&now);
  int currentDay = timeinfo->tm_wday;
  int currentHour = timeinfo->tm_hour;
  int currentMin = timeinfo->tm_min;
  
  for (Alarm& alarm : alarms) {
    if (!alarm.enabled) continue;
    
    // Проверяем, активен ли будильник для текущего дня
    bool dayActive = false;
    for (int day : alarm.days) {
      if (day == currentDay) {
        dayActive = true;
        break;
      }
    }
    
    if (!dayActive) continue;
    
    // Парсим время будильника
    int alarmHour, alarmMin;
    if (sscanf(alarm.time.c_str(), "%d:%d", &alarmHour, &alarmMin) != 2) continue;
    
    // Проверяем, пора ли сработать
    if (currentHour == alarmHour && currentMin == alarmMin) {
      Serial.printf("Будильник сработал: %s\n", alarm.label.c_str());
      
      // Устанавливаем громкость и воспроизводим трек
      dfPlayer.volume(alarm.volume);
      dfPlayer.play(alarm.track);
      
      // Небольшая задержка, чтобы будильник не срабатывал несколько раз
      delay(1000);
    }
  }
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
  
  // Initialize watchdog timer
  esp_task_wdt_init(10, true); // 10 second timeout, panic on timeout
  esp_task_wdt_add(NULL); // Add current task (setup task) to watchdog
  
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

  // Инициализация времени
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  
  // Загрузка будильников
  loadAlarms();

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

  // Главная страница
  server.on("/", HTTP_GET, [](){
    File file = SPIFFS.open("/index.html", "r");
    if (!file) {
      server.send(404, "text/plain", "File not found");
      return;
    }
    server.streamFile(file, "text/html");
    file.close();
  });
  
  // Веб-форма для смены WiFi
  server.on("/wifi", HTTP_GET, [](){
    String html = "<html><body><h2>Настройка WiFi</h2>"
      "<form method='POST' action='/api/wifi'>"
      "SSID: <input name='ssid' value='" + wifi_ssid + "'><br>"
      "Пароль: <input name='pass' type='password' value='" + wifi_pass + "'><br>"
      "<input type='submit' value='Сохранить'>"
      "</form></body></html>";
    server.send(200, "text/html", html);
  });

  // REST API для смены WiFi
  server.on("/api/wifi", HTTP_POST, [](){
    String newSsid = server.arg("ssid");
    String newPass = server.arg("pass");
    if (newSsid.length() > 0 && newPass.length() > 0) {
      File configFile = SPIFFS.open("/config.txt", FILE_WRITE);
      if (configFile) {
        configFile.println("wifi_ssid=" + newSsid);
        configFile.println("wifi_pass=" + newPass);
        configFile.close();
        server.send(200, "text/html", "<html><body>WiFi сохранён. Перезагрузка...</body></html>");
        delay(1000);
        ESP.restart();
      } else {
        server.send(500, "text/plain", "Ошибка записи файла конфигурации");
      }
    } else {
      server.send(400, "text/plain", "SSID и пароль обязательны");
    }
  });

  // --- REST API для управления DFPlayer Mini ---
  // Воспроизвести текущий трек
  server.on("/api/audio/play", HTTP_POST, [](){
    dfPlayer.start();
    server.send(200, "application/json", "{\"status\":\"playing\"}");
  });
  // Остановить воспроизведение
  server.on("/api/audio/stop", HTTP_POST, [](){
    dfPlayer.stop();
    server.send(200, "application/json", "{\"status\":\"stopped\"}");
  });
  // Установить громкость: /api/audio/volume?value=20
  server.on("/api/audio/volume", HTTP_POST, [](){
    if (server.hasArg("value")) {
      int vol = server.arg("value").toInt();
      vol = constrain(vol, 0, 30);
      dfPlayer.volume(vol);
      server.send(200, "application/json", "{\"status\":\"ok\",\"volume\":" + String(vol) + "}");
    } else {
      server.send(400, "application/json", "{\"error\":\"volume required\"}");
    }
  });
  // Воспроизвести определённый трек: /api/audio/track?num=1
  server.on("/api/audio/track", HTTP_POST, [](){
    if (server.hasArg("num")) {
      int num = server.arg("num").toInt();
      if (num > 0) {
        dfPlayer.play(num);
        server.send(200, "application/json", "{\"status\":\"playing\",\"track\":" + String(num) + "}");
      } else {
        server.send(400, "application/json", "{\"error\":\"invalid track number\"}");
      }
    } else {
      server.send(400, "application/json", "{\"error\":\"track number required\"}");
    }
  });
  // --- конец REST API DFPlayer ---

  // --- REST API для будильников ---
  // Получить список будильников
  server.on("/api/alarms", HTTP_GET, [](){
    DynamicJsonDocument doc(2048);
    JsonArray alarmsArray = doc.createNestedArray("alarms");
    
    for (const Alarm& alarm : alarms) {
      JsonObject alarmObj = alarmsArray.createNestedObject();
      alarmObj["time"] = alarm.time;
      alarmObj["track"] = alarm.track;
      alarmObj["volume"] = alarm.volume;
      alarmObj["label"] = alarm.label;
      alarmObj["enabled"] = alarm.enabled;
      
      JsonArray daysArray = alarmObj.createNestedArray("days");
      for (int day : alarm.days) {
        daysArray.add(day);
      }
    }
    
    String response;
    serializeJson(doc, response);
    server.send(200, "application/json", response);
  });
  
  // Добавить будильник
  server.on("/api/alarms", HTTP_POST, [](){
    if (server.hasHeader("Content-Type") && server.header("Content-Type") == "application/json") {
      String body = server.arg("plain");
      DynamicJsonDocument doc(1024);
      DeserializationError error = deserializeJson(doc, body);
      
      if (error) {
        server.send(400, "text/plain", "Invalid JSON");
        return;
      }
      
      Alarm alarm;
      alarm.time = doc["time"].as<String>();
      alarm.track = doc["track"] | 1;
      alarm.volume = doc["volume"] | 20;
      alarm.label = doc["label"].as<String>();
      alarm.enabled = doc["enabled"] | true;
      
      JsonArray daysArray = doc["days"];
      for (int day : daysArray) {
        alarm.days.push_back(day);
      }
      
      alarms.push_back(alarm);
      saveAlarms();
      
      server.send(200, "application/json", "{\"status\":\"added\"}");
    } else {
      server.send(400, "text/plain", "Content-Type must be application/json");
    }
  });
  
  // Переключить статус будильника
  server.on("/api/alarms/([0-9]+)/toggle", HTTP_POST, [](){
    int index = server.pathArg(0).toInt();
    if (index >= 0 && index < alarms.size()) {
      alarms[index].enabled = !alarms[index].enabled;
      saveAlarms();
      server.send(200, "application/json", "{\"status\":\"toggled\"}");
    } else {
      server.send(404, "text/plain", "Alarm not found");
    }
  });
  
  // Удалить будильник
  server.on("/api/alarms/([0-9]+)", HTTP_DELETE, [](){
    int index = server.pathArg(0).toInt();
    if (index >= 0 && index < alarms.size()) {
      alarms.erase(alarms.begin() + index);
      saveAlarms();
      server.send(200, "application/json", "{\"status\":\"deleted\"}");
    } else {
      server.send(404, "text/plain", "Alarm not found");
    }
  });
  
  // --- конец REST API для будильников ---
  
  // --- REST API для WiFi информации ---
  server.on("/api/wifi/info", HTTP_GET, [](){
    DynamicJsonDocument doc(512);
    doc["ssid"] = wifi_ssid;
    doc["status"] = WiFi.status() == WL_CONNECTED ? "Подключен" : "Отключен";
    doc["ip"] = WiFi.localIP().toString();
    doc["signal"] = WiFi.RSSI();
    
    String response;
    serializeJson(doc, response);
    server.send(200, "application/json", response);
  });
  
  // --- конец REST API для WiFi информации ---

  server.begin();
  Serial.println("Веб-сервер запущен. Откройте / для доступа к интерфейсу.");

  // Create tasks for watchdog management
  xTaskCreatePinnedToCore(
    mainTask,           // Task function
    "MainTask",         // Task name
    4096,               // Stack size
    NULL,               // Task parameters
    1,                  // Task priority
    &mainTaskHandle,    // Task handle
    1                   // Core to run on (Core 1)
  );
  
  xTaskCreatePinnedToCore(
    webServerTask,      // Task function
    "WebServerTask",    // Task name
    4096,               // Stack size
    NULL,               // Task parameters
    1,                  // Task priority
    &webServerTaskHandle, // Task handle
    0                   // Core to run on (Core 0)
  );
  
  // Add tasks to watchdog timer
  if (mainTaskHandle != NULL) {
    esp_task_wdt_add(mainTaskHandle);
  }
  if (webServerTaskHandle != NULL) {
    esp_task_wdt_add(webServerTaskHandle);
  }
  
  Serial.println("Tasks created and added to watchdog timer");

  // Для LittleFS используйте #include <LittleFS.h> и LittleFS.begin() вместо SPIFFS
}

void loop() {
  // Reset watchdog timer for the main loop task
  esp_task_wdt_reset();
  
  // Handle web server requests
  server.handleClient();
  
  // Проверка будильников
  checkAlarms();
  
  // Basic application logic
  // Check WiFi connection status
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi connection lost, attempting to reconnect...");
    connectWiFi(10, 500);
  }
  
  // Small delay to prevent watchdog from triggering
  delay(1000);
}
