#include <Arduino.h>
#include <SPIFFS.h>
#include <WiFi.h>
#include <WebServer.h>
#include <WiFiClient.h>

#include <DFRobotDFPlayerMini.h>
#include <esp_task_wdt.h>
// #include <NTPClient.h>
// #include <WiFiUdp.h>

// Объявление Serial для ESP32
extern HardwareSerial Serial;

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
    String html = "<!DOCTYPE html><html><head>";
    html += "<meta charset='UTF-8'>";
    html += "<meta name='viewport' content='width=device-width, initial-scale=1.0'>";
    html += "<title>ESP32 Гонг Дуллабха</title>";
    html += "</head><body>";
    html += "<h1>ESP32 Гонг Дуллабха</h1>";
    html += "<p>Добро пожаловать в систему управления гонгом!</p>";
    html += "<ul>";
    html += "<li><a href='/wifi'>Настройка WiFi</a></li>";
    html += "<li><a href='/status'>Статус системы</a></li>";
    html += "<li><a href='/audio'>Управление аудио</a></li>";
    html += "</ul>";
    html += "</body></html>";
    server.send(200, "text/html; charset=utf-8", html);
  });

  // Страница статуса системы
  server.on("/status", HTTP_GET, [](){
    String html = "<!DOCTYPE html><html><head>";
    html += "<meta charset='UTF-8'>";
    html += "<meta name='viewport' content='width=device-width, initial-scale=1.0'>";
    html += "<title>Статус системы</title>";
    html += "</head><body>";
    html += "<h2>Статус системы</h2>";
    html += "<p><strong>WiFi:</strong> ";
    html += (WiFi.status() == WL_CONNECTED ? "Подключен" : "Отключен");
    html += "</p>";
    if (WiFi.status() == WL_CONNECTED) {
      html += "<p><strong>IP адрес:</strong> ";
      html += WiFi.localIP().toString();
      html += "</p>";
      html += "<p><strong>SSID:</strong> ";
      html += wifi_ssid;
      html += "</p>";
    }
    html += "<p><strong>DFPlayer:</strong> ";
    html += (dfPlayer.available() ? "Готов" : "Не найден");
    html += "</p>";
    html += "<p><a href='/'>Назад</a></p>";
    html += "</body></html>";
    server.send(200, "text/html; charset=utf-8", html);
  });

  // Страница управления аудио
  server.on("/audio", HTTP_GET, [](){
    String html = "<!DOCTYPE html><html><head>";
    html += "<meta charset='UTF-8'>";
    html += "<meta name='viewport' content='width=device-width, initial-scale=1.0'>";
    html += "<title>Управление аудио</title>";
    html += "</head><body>";
    html += "<h2>Управление аудио</h2>";
    html += "<h3>Воспроизведение</h3>";
    html += "<button onclick='playAudio()'>Воспроизвести</button> ";
    html += "<button onclick='stopAudio()'>Остановить</button><br><br>";
    html += "<h3>Громкость</h3>";
    html += "<input type='range' id='volume' min='0' max='30' value='20' onchange='setVolume(this.value)'> ";
    html += "<span id='volValue'>20</span><br><br>";
    html += "<h3>Выбор трека</h3>";
    html += "<input type='number' id='track' min='1' max='100' value='1'> ";
    html += "<button onclick='playTrack()'>Воспроизвести трек</button><br><br>";
    html += "<p><a href='/'>Назад</a></p>";
    html += "<script>";
    html += "function playAudio() { fetch('/api/audio/play', {method:'POST'}).then(r=>r.json()).then(d=>alert(d.status)); }";
    html += "function stopAudio() { fetch('/api/audio/stop', {method:'POST'}).then(r=>r.json()).then(d=>alert(d.status)); }";
    html += "function setVolume(val) { document.getElementById('volValue').textContent = val; fetch('/api/audio/volume?value='+val, {method:'POST'}).then(r=>r.json()).then(d=>alert('Громкость: ' + d.volume)); }";
    html += "function playTrack() { let track = document.getElementById('track').value; fetch('/api/audio/track?num='+track, {method:'POST'}).then(r=>r.json()).then(d=>alert(d.status + ' трек: ' + d.track)); }";
    html += "</script>";
    html += "</body></html>";
    server.send(200, "text/html; charset=utf-8", html);
  });

  // Веб-форма для смены WiFi
  server.on("/wifi", HTTP_GET, [](){
    String html = "<!DOCTYPE html><html><head>";
    html += "<meta charset='UTF-8'>";
    html += "<meta name='viewport' content='width=device-width, initial-scale=1.0'>";
    html += "<title>Настройка WiFi</title>";
    html += "</head><body>";
    html += "<h2>Настройка WiFi</h2>";
    html += "<form method='POST' action='/api/wifi'>";
    html += "SSID: <input name='ssid' value='";
    html += wifi_ssid;
    html += "'><br>";
    html += "Пароль: <input name='pass' type='password' value='";
    html += wifi_pass;
    html += "'><br>";
    html += "<input type='submit' value='Сохранить'>";
    html += "</form>";
    html += "<p><a href='/'>Назад</a></p>";
    html += "</body></html>";
    server.send(200, "text/html; charset=utf-8", html);
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
        String html = "<!DOCTYPE html><html><head>";
        html += "<meta charset='UTF-8'>";
        html += "<meta name='viewport' content='width=device-width, initial-scale=1.0'>";
        html += "<title>WiFi сохранен</title>";
        html += "</head><body>";
        html += "<h2>WiFi сохранен</h2>";
        html += "<p>Настройки WiFi успешно сохранены. Устройство перезагружается...</p>";
        html += "</body></html>";
        server.send(200, "text/html; charset=utf-8", html);
        delay(1000);
        ESP.restart();
      } else {
        server.send(500, "text/plain; charset=utf-8", "Ошибка записи файла конфигурации");
      }
    } else {
      server.send(400, "text/plain; charset=utf-8", "SSID и пароль обязательны");
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

  // Обработчик для несуществующих страниц (404)
  server.onNotFound([](){
    String html = "<!DOCTYPE html><html><head>";
    html += "<meta charset='UTF-8'>";
    html += "<meta name='viewport' content='width=device-width, initial-scale=1.0'>";
    html += "<title>404 - Страница не найдена</title>";
    html += "</head><body>";
    html += "<h1>404 - Страница не найдена</h1>";
    html += "<p>Запрашиваемая страница не существует.</p>";
    html += "<p><a href='/'>Вернуться на главную</a></p>";
    html += "</body></html>";
    server.send(404, "text/html; charset=utf-8", html);
  });

  server.begin();
  Serial.println("Веб-сервер запущен. Откройте /wifi для настройки WiFi.");

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
  
  // Basic application logic
  // Check WiFi connection status
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi connection lost, attempting to reconnect...");
    connectWiFi(10, 500);
  }
  
  // Small delay to prevent watchdog from triggering
  delay(1000);
}