#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <SPIFFS.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <ArduinoJson.h>
#include <SoftwareSerial.h>
#include <DFRobotDFPlayerMini.h>

// WiFi credentials
const char* ssid = "KoshkinDom";
const char* password = "mousehouse555";

// Web server
AsyncWebServer server(80);

// NTP Client
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org");

// DFPlayer Mini
SoftwareSerial mySoftwareSerial(16, 17); // RX, TX
DFRobotDFPlayerMini myDFPlayer;

// Database files
const char* ALARMS_FILE = "/alarms.json";
const char* TEMPLATES_FILE = "/templates.json";
const char* SETTINGS_FILE = "/settings.json";

// Settings
struct Settings {
    int alarmDuration;
    int volume;
    bool autoSync;
    String timezone;
};

// Alarm structure
struct Alarm {
    String id;
    String time;
    int days[7];
    bool enabled;
    String templateId;  // Reference to template if this alarm is part of one
};

// Template structure
struct Template {
    String id;
    String name;
    Alarm alarms[10];  // Up to 10 alarms per template
    int alarmCount;
    int duration;  // Duration in days
};

// Global variables
Settings settings = {30, 20, true, "UTC+3"};
Alarm alarms[20];  // Increased to 20 alarms
int alarmCount = 0;
Template templates[5];  // Up to 5 templates
int templateCount = 0;
bool isAlarmPlaying = false;
unsigned long alarmStartTime = 0;

// Helper function to generate unique ID
String generateId() {
    return String(millis()) + String(random(1000));
}

void setup() {
    Serial.begin(115200);
    randomSeed(analogRead(0));
    
    // Initialize SPIFFS
    if(!SPIFFS.begin(true)){
        Serial.println("SPIFFS Mount Failed");
        return;
    }
    
    // Connect to WiFi
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
        delay(1000);
        Serial.println("Connecting to WiFi...");
    }
    Serial.println("Connected to WiFi");
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());
    
    // Initialize NTP
    timeClient.begin();
    timeClient.setTimeOffset(3 * 3600); // UTC+3
    
    // Initialize DFPlayer
    mySoftwareSerial.begin(9600);
    if (!myDFPlayer.begin(mySoftwareSerial)) {
        Serial.println("Unable to begin DFPlayer Mini");
        while(true);
    }
    myDFPlayer.volume(settings.volume);
    
    // Load saved data
    loadSettings();
    loadAlarms();
    loadTemplates();
    
    // Setup web server routes
    setupWebServer();
    
    server.begin();
}

void loop() {
    timeClient.update();
    
    // Check if alarm should be playing
    if (isAlarmPlaying) {
        if (millis() - alarmStartTime >= settings.alarmDuration * 1000) {
            stopAlarm();
        }
    } else {
        checkAlarms();
    }
}

void setupWebServer() {
    // Serve static files from SPIFFS
    server.serveStatic("/", SPIFFS, "/").setDefaultFile("index.html");
    
    // API endpoints
    server.on("/api/time", HTTP_GET, [](AsyncWebServerRequest *request){
        String time = timeClient.getFormattedTime();
        String date = getFormattedDate();
        
        StaticJsonDocument<200> doc;
        doc["time"] = time;
        doc["date"] = date;
        
        String response;
        serializeJson(doc, response);
        request->send(200, "application/json", response);
    });
    
    // Get all data
    server.on("/api/data", HTTP_GET, [](AsyncWebServerRequest *request){
        StaticJsonDocument<2048> doc;
        
        // Add settings
        JsonObject settingsObj = doc.createNestedObject("settings");
        settingsObj["duration"] = settings.alarmDuration;
        settingsObj["volume"] = settings.volume;
        settingsObj["autoSync"] = settings.autoSync;
        settingsObj["timezone"] = settings.timezone;
        
        // Add alarms
        JsonArray alarmsArray = doc.createNestedArray("alarms");
        for(int i = 0; i < alarmCount; i++) {
            JsonObject alarm = alarmsArray.createNestedObject();
            alarm["id"] = alarms[i].id;
            alarm["time"] = alarms[i].time;
            JsonArray days = alarm.createNestedArray("days");
            for(int j = 0; j < 7; j++) {
                days.add(alarms[i].days[j]);
            }
            alarm["enabled"] = alarms[i].enabled;
            alarm["templateId"] = alarms[i].templateId;
        }
        
        // Add templates
        JsonArray templatesArray = doc.createNestedArray("templates");
        for(int i = 0; i < templateCount; i++) {
            JsonObject template = templatesArray.createNestedObject();
            template["id"] = templates[i].id;
            template["name"] = templates[i].name;
            template["duration"] = templates[i].duration;
            
            JsonArray templateAlarms = template.createNestedArray("alarms");
            for(int j = 0; j < templates[i].alarmCount; j++) {
                JsonObject alarm = templateAlarms.createNestedObject();
                alarm["id"] = templates[i].alarms[j].id;
                alarm["time"] = templates[i].alarms[j].time;
                JsonArray days = alarm.createNestedArray("days");
                for(int k = 0; k < 7; k++) {
                    days.add(templates[i].alarms[j].days[k]);
                }
                alarm["enabled"] = templates[i].alarms[j].enabled;
            }
        }
        
        String response;
        serializeJson(doc, response);
        request->send(200, "application/json", response);
    });
    
    // Save template
    server.on("/api/templates", HTTP_POST, [](AsyncWebServerRequest *request){}, NULL,
        [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total){
            StaticJsonDocument<1024> doc;
            deserializeJson(doc, (char*)data);
            
            if(templateCount < 5) {
                Template template;
                template.id = generateId();
                template.name = doc["name"].as<String>();
                template.duration = doc["duration"] | 10;
                
                JsonArray alarmsArray = doc["alarms"];
                template.alarmCount = 0;
                for(JsonObject alarmObj : alarmsArray) {
                    if(template.alarmCount < 10) {
                        Alarm alarm;
                        alarm.id = generateId();
                        alarm.time = alarmObj["time"].as<String>();
                        JsonArray days = alarmObj["days"];
                        for(int i = 0; i < days.size(); i++) {
                            alarm.days[i] = days[i];
                        }
                        alarm.enabled = alarmObj["enabled"] | true;
                        alarm.templateId = template.id;
                        
                        template.alarms[template.alarmCount++] = alarm;
                    }
                }
                
                templates[templateCount++] = template;
                saveTemplates();
                
                request->send(200, "application/json", "{\"status\":\"success\",\"id\":\"" + template.id + "\"}");
            } else {
                request->send(400, "application/json", "{\"status\":\"error\",\"message\":\"Maximum number of templates reached\"}");
            }
    });
    
    server.on("/api/alarms", HTTP_GET, [](AsyncWebServerRequest *request){
        String response = getAlarmsJson();
        request->send(200, "application/json", response);
    });
    
    server.on("/api/alarms", HTTP_POST, [](AsyncWebServerRequest *request){}, NULL,
        [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total){
            StaticJsonDocument<200> doc;
            deserializeJson(doc, (char*)data);
            
            Alarm alarm;
            alarm.id = generateId();
            alarm.time = doc["time"].as<String>();
            JsonArray days = doc["days"];
            for(int i = 0; i < days.size(); i++) {
                alarm.days[i] = days[i];
            }
            alarm.enabled = true;
            alarm.templateId = "";
            
            addAlarm(alarm);
            request->send(200, "application/json", "{\"status\":\"success\"}");
    });
    
    server.on("^\\/api\\/alarms\\/([0-9]+)$", HTTP_DELETE, [](AsyncWebServerRequest *request){
        String id = request->pathArg(0);
        deleteAlarm(id.toInt());
        request->send(200, "application/json", "{\"status\":\"success\"}");
    });
    
    server.on("/api/trigger", HTTP_POST, [](AsyncWebServerRequest *request){}, NULL,
        [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total){
            StaticJsonDocument<200> doc;
            deserializeJson(doc, (char*)data);
            int duration = doc["duration"] | settings.alarmDuration;
            triggerAlarm(duration);
            request->send(200, "application/json", "{\"status\":\"success\"}");
    });
    
    server.on("/api/settings", HTTP_POST, [](AsyncWebServerRequest *request){}, NULL,
        [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total){
            StaticJsonDocument<200> doc;
            deserializeJson(doc, (char*)data);
            settings.alarmDuration = doc["duration"] | settings.alarmDuration;
            settings.volume = doc["volume"] | settings.volume;
            settings.autoSync = doc["autoSync"] | settings.autoSync;
            settings.timezone = doc["timezone"] | settings.timezone;
            saveSettings();
            request->send(200, "application/json", "{\"status\":\"success\"}");
    });
}

// Helper functions
String getFormattedDate() {
    time_t epochTime = timeClient.getEpochTime();
    struct tm *ptm = gmtime((time_t *)&epochTime);
    char dateString[20];
    strftime(dateString, sizeof(dateString), "%d.%m.%Y", ptm);
    return String(dateString);
}

void loadAlarms() {
    if(SPIFFS.exists(ALARMS_FILE)) {
        File file = SPIFFS.open(ALARMS_FILE, "r");
        StaticJsonDocument<1024> doc;
        deserializeJson(doc, file);
        file.close();
        
        JsonArray alarmsArray = doc["alarms"];
        alarmCount = 0;
        for(JsonObject alarm : alarmsArray) {
            if(alarmCount < 20) {
                alarms[alarmCount].id = alarm["id"].as<String>();
                alarms[alarmCount].time = alarm["time"].as<String>();
                JsonArray days = alarm["days"];
                for(int i = 0; i < days.size(); i++) {
                    alarms[alarmCount].days[i] = days[i];
                }
                alarms[alarmCount].enabled = alarm["enabled"] | true;
                alarms[alarmCount].templateId = alarm["templateId"].as<String>();
                alarmCount++;
            }
        }
    }
}

void saveAlarms() {
    StaticJsonDocument<1024> doc;
    JsonArray alarmsArray = doc.createNestedArray("alarms");
    
    for(int i = 0; i < alarmCount; i++) {
        JsonObject alarm = alarmsArray.createNestedObject();
        alarm["id"] = alarms[i].id;
        alarm["time"] = alarms[i].time;
        JsonArray days = alarm.createNestedArray("days");
        for(int j = 0; j < 7; j++) {
            days.add(alarms[i].days[j]);
        }
        alarm["enabled"] = alarms[i].enabled;
        alarm["templateId"] = alarms[i].templateId;
    }
    
    File file = SPIFFS.open(ALARMS_FILE, "w");
    serializeJson(doc, file);
    file.close();
}

void loadSettings() {
    if(SPIFFS.exists(SETTINGS_FILE)) {
        File file = SPIFFS.open(SETTINGS_FILE, "r");
        StaticJsonDocument<200> doc;
        deserializeJson(doc, file);
        file.close();
        
        settings.alarmDuration = doc["duration"] | 30;
        settings.volume = doc["volume"] | 20;
        settings.autoSync = doc["autoSync"] | true;
        settings.timezone = doc["timezone"] | "UTC+3";
    }
}

void saveSettings() {
    StaticJsonDocument<200> doc;
    doc["duration"] = settings.alarmDuration;
    doc["volume"] = settings.volume;
    doc["autoSync"] = settings.autoSync;
    doc["timezone"] = settings.timezone;
    
    File file = SPIFFS.open(SETTINGS_FILE, "w");
    serializeJson(doc, file);
    file.close();
}

String getAlarmsJson() {
    StaticJsonDocument<1024> doc;
    JsonArray alarmsArray = doc.createNestedArray("alarms");
    
    for(int i = 0; i < alarmCount; i++) {
        JsonObject alarm = alarmsArray.createNestedObject();
        alarm["id"] = alarms[i].id;
        alarm["time"] = alarms[i].time;
        JsonArray days = alarm.createNestedArray("days");
        for(int j = 0; j < 7; j++) {
            days.add(alarms[i].days[j]);
        }
        alarm["enabled"] = alarms[i].enabled;
        alarm["templateId"] = alarms[i].templateId;
    }
    
    String response;
    serializeJson(doc, response);
    return response;
}

void addAlarm(Alarm alarm) {
    if(alarmCount < 20) {
        alarms[alarmCount] = alarm;
        alarmCount++;
        saveAlarms();
    }
}

void deleteAlarm(int index) {
    if(index >= 0 && index < alarmCount) {
        for(int i = index; i < alarmCount - 1; i++) {
            alarms[i] = alarms[i + 1];
        }
        alarmCount--;
        saveAlarms();
    }
}

void checkAlarms() {
    if(isAlarmPlaying) return;
    
    String currentTime = timeClient.getFormattedTime();
    int currentDay = timeClient.getDay();
    
    for(int i = 0; i < alarmCount; i++) {
        if(alarms[i].time == currentTime && alarms[i].days[currentDay] == 1 && alarms[i].enabled) {
            triggerAlarm(settings.alarmDuration);
            break;
        }
    }
}

void triggerAlarm(int duration) {
    if(!isAlarmPlaying) {
        myDFPlayer.play(1); // Play first track
        isAlarmPlaying = true;
        alarmStartTime = millis();
        settings.alarmDuration = duration;
    }
}

void stopAlarm() {
    if(isAlarmPlaying) {
        myDFPlayer.stop();
        isAlarmPlaying = false;
    }
}

void loadTemplates() {
    if(SPIFFS.exists(TEMPLATES_FILE)) {
        File file = SPIFFS.open(TEMPLATES_FILE, "r");
        StaticJsonDocument<2048> doc;
        deserializeJson(doc, file);
        file.close();
        
        JsonArray templatesArray = doc["templates"];
        templateCount = 0;
        for(JsonObject templateObj : templatesArray) {
            if(templateCount < 5) {
                Template template;
                template.id = templateObj["id"].as<String>();
                template.name = templateObj["name"].as<String>();
                template.duration = templateObj["duration"] | 10;
                
                JsonArray alarmsArray = templateObj["alarms"];
                template.alarmCount = 0;
                for(JsonObject alarmObj : alarmsArray) {
                    if(template.alarmCount < 10) {
                        Alarm alarm;
                        alarm.id = alarmObj["id"].as<String>();
                        alarm.time = alarmObj["time"].as<String>();
                        JsonArray days = alarmObj["days"];
                        for(int i = 0; i < days.size(); i++) {
                            alarm.days[i] = days[i];
                        }
                        alarm.enabled = alarmObj["enabled"] | true;
                        alarm.templateId = template.id;
                        
                        template.alarms[template.alarmCount++] = alarm;
                    }
                }
                
                templates[templateCount++] = template;
            }
        }
    }
}

void saveTemplates() {
    StaticJsonDocument<2048> doc;
    JsonArray templatesArray = doc.createNestedArray("templates");
    
    for(int i = 0; i < templateCount; i++) {
        JsonObject template = templatesArray.createNestedObject();
        template["id"] = templates[i].id;
        template["name"] = templates[i].name;
        template["duration"] = templates[i].duration;
        
        JsonArray alarmsArray = template.createNestedArray("alarms");
        for(int j = 0; j < templates[i].alarmCount; j++) {
            JsonObject alarm = alarmsArray.createNestedObject();
            alarm["id"] = templates[i].alarms[j].id;
            alarm["time"] = templates[i].alarms[j].time;
            JsonArray days = alarm.createNestedArray("days");
            for(int k = 0; k < 7; k++) {
                days.add(templates[i].alarms[j].days[k]);
            }
            alarm["enabled"] = templates[i].alarms[j].enabled;
        }
    }
    
    File file = SPIFFS.open(TEMPLATES_FILE, "w");
    serializeJson(doc, file);
    file.close();
} 