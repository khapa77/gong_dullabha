![Status](https://img.shields.io/badge/status-in%20development-yellow)

# Gong - Будильник по расписанию

🚧 **Work in Progress** 🚧  
This project is currently under development.  

Проект умного будильника на базе ESP32-WROOM-32D с веб-интерфейсом для управления расписанием.

![Status](https://img.shields.io/badge/status-in%20development-yellow)

## Функциональность

- Веб-интерфейс для управления будильниками
- Поддержка расписания на 10 дней
- Настройка продолжительности звучания
- Ручное управление будильником
- Отображение текущего времени
- Автоматическая синхронизация времени через NTP
- Воспроизведение звуков через DFPlayer Mini

## Аппаратные требования

- ESP32-WROOM-32D
- DFPlayer Mini
- SD карта для DFPlayer Mini
- Динамик
- Источник питания 5V

## Подключение компонентов

```
ESP32-WROOM-32D
├── GPIO16 (RX) ────────> DFPlayer Mini (TX)
├── GPIO17 (TX) ────────> DFPlayer Mini (RX)
├── 3.3V ───────────────> DFPlayer Mini (VCC)
└── GND ────────────────> DFPlayer Mini (GND)
```

## Установка

1. Установите Arduino IDE
2. Установите необходимые библиотеки:
   - ESP32 Arduino Core
   - ESPAsyncWebServer
   - ArduinoJson
   - DFRobotDFPlayerMini
3. Подключите ESP32 к компьютеру
4. Загрузите проект в Arduino IDE
5. Настройте WiFi параметры в файле `gong.ino`
6. Загрузите скетч на ESP32
7. Загрузите файлы веб-интерфейса в SPIFFS

## Использование

1. Подключитесь к WiFi сети ESP32
2. Откройте веб-браузер и перейдите по адресу `http://[IP-адрес-ESP32]`
3. Настройте будильники через веб-интерфейс

## Структура проекта

```
gong/
├── src/
│   └── gong.ino
├── data/
│   ├── index.html
│   ├── styles.css
│   └── script.js
└── README.md
```

## API Endpoints

- `GET /api/time` - Получить текущее время
- `GET /api/alarms` - Получить список будильников
- `POST /api/alarms` - Добавить будильник
- `DELETE /api/alarms/{id}` - Удалить будильник
- `POST /api/trigger` - Запустить будильник вручную
- `POST /api/settings` - Изменить настройки

## Лицензия

MIT

## Структура проекта

- `backend/` — серверная часть (Flask API)
- `frontend/` — веб-интерфейс (HTML/JS)
- `esp32/` — прошивка для ESP32 (создать при необходимости)

## Запуск backend (Flask)

```bash
cd backend
python3 -m venv venv
source venv/bin/activate
pip install -r requirements.txt
python app.py
```

## Запуск frontend

Откройте файл `frontend/index.html` в браузере. Для работы с API backend должен быть запущен на http://localhost:5000

## Требования
- Python 3.8+
- Flask
- Flask-CORS
- Браузер (Chrome, Firefox, Edge)

## ESP32

Прошивка для ESP32 размещается в папке `esp32/` (создайте при необходимости). Включает работу с WiFi, SPIFFS/LittleFS, DFPlayer Mini, NTP и REST API.

---

Проект реализует веб-интерфейс для управления будильниками, серверную часть на Flask и предполагает интеграцию с ESP32. 
