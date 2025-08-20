#!/bin/bash

# Автоматическое определение порта (первый найденный ESP32)
PORT=$(ls /dev/cu.usbserial* 2>/dev/null | head -n1)
if [ -z "$PORT" ]; then
    PORT=$(ls /dev/cu.wchusbserial* 2>/dev/null | head -n1)
fi

if [ -z "$PORT" ]; then
    echo "ESP32 не найден!"
    exit 1
fi

echo "Используется порт: $PORT"

# Параметры SPIFFS (настройте под вашу плату)
mkspiffs -c ./data -b 4096 -p 256 -s 0x280000 spiffs_image.bin
esptool.py --chip esp32 --port $PORT erase_region 0x180000 0x280000
esptool.py --chip esp32 --port $PORT --baud 921600 write_flash -z 0x180000 spiffs_image.bin
rm spiffs_image.bin

echo "SPIFFS успешно обновлен!"
