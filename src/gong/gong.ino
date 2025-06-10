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

// Alarm settings
const char* ALARMS_FILE = "/alarms.json";
const char* SETTINGS_FILE = "/settings.json";
int alarmDuration = 30; // Default duration in seconds
bool isAlarmPlaying = false;
unsigned long alarmStartTime = 0;

// Alarm structure
struct Alarm {
    String time;
    int days[7];
};

// Global variables
Alarm alarms[10]; // Maximum 10 alarms
int alarmCount = 0;

void setup() {
    Serial.begin(115200);
    
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
    timeClient.setTimeOffset(0); // Set your timezone offset in seconds
    
    // Initialize DFPlayer
    mySoftwareSerial.begin(9600);
    if (!myDFPlayer.begin(mySoftwareSerial)) {
        Serial.println("Unable to begin DFPlayer Mini");
        while(true);
    }
    myDFPlayer.volume(20); // Set volume (0-30)
    
    // Load saved data
    loadAlarms();
    loadSettings();
    
    // Setup web server routes
    setupWebServer();
    
    server.begin();
}

void loop() {
    timeClient.update();
    
    // Check if alarm should be playing
    if (isAlarmPlaying) {
        if (millis() - alarmStartTime >= alarmDuration * 1000) {
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
    
    server.on("/api/alarms", HTTP_GET, [](AsyncWebServerRequest *request){
        String response = getAlarmsJson();
        request->send(200, "application/json", response);
    });
    
    server.on("/api/alarms", HTTP_POST, [](AsyncWebServerRequest *request){}, NULL,
        [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total){
            StaticJsonDocument<200> doc;
            deserializeJson(doc, (char*)data);
            
            Alarm alarm;
            alarm.time = doc["time"].as<String>();
            JsonArray days = doc["days"];
            for(int i = 0; i < days.size(); i++) {
                alarm.days[i] = days[i];
            }
            
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
            int duration = doc["duration"] | alarmDuration;
            triggerAlarm(duration);
            request->send(200, "application/json", "{\"status\":\"success\"}");
    });
    
    server.on("/api/settings", HTTP_POST, [](AsyncWebServerRequest *request){}, NULL,
        [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total){
            StaticJsonDocument<200> doc;
            deserializeJson(doc, (char*)data);
            alarmDuration = doc["duration"] | alarmDuration;
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
            if(alarmCount < 10) {
                alarms[alarmCount].time = alarm["time"].as<String>();
                JsonArray days = alarm["days"];
                for(int i = 0; i < days.size(); i++) {
                    alarms[alarmCount].days[i] = days[i];
                }
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
        alarm["time"] = alarms[i].time;
        JsonArray days = alarm.createNestedArray("days");
        for(int j = 0; j < 7; j++) {
            days.add(alarms[i].days[j]);
        }
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
        
        alarmDuration = doc["duration"] | 30;
    }
}

void saveSettings() {
    StaticJsonDocument<200> doc;
    doc["duration"] = alarmDuration;
    
    File file = SPIFFS.open(SETTINGS_FILE, "w");
    serializeJson(doc, file);
    file.close();
}

String getAlarmsJson() {
    StaticJsonDocument<1024> doc;
    JsonArray alarmsArray = doc.createNestedArray("alarms");
    
    for(int i = 0; i < alarmCount; i++) {
        JsonObject alarm = alarmsArray.createNestedObject();
        alarm["time"] = alarms[i].time;
        JsonArray days = alarm.createNestedArray("days");
        for(int j = 0; j < 7; j++) {
            days.add(alarms[i].days[j]);
        }
    }
    
    String response;
    serializeJson(doc, response);
    return response;
}

void addAlarm(Alarm alarm) {
    if(alarmCount < 10) {
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
        if(alarms[i].time == currentTime && alarms[i].days[currentDay] == 1) {
            triggerAlarm(alarmDuration);
            break;
        }
    }
}

void triggerAlarm(int duration) {
    if(!isAlarmPlaying) {
        myDFPlayer.play(1); // Play first track
        isAlarmPlaying = true;
        alarmStartTime = millis();
        alarmDuration = duration;
    }
}

void stopAlarm() {
    if(isAlarmPlaying) {
        myDFPlayer.stop();
        isAlarmPlaying = false;
    }
} 
