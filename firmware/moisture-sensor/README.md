# Moisture Sensor Firmware

## Description
This firmware, implemented as an Arduino .ino sketch, is designed for the NPH Automation moisture sensor.

The primary function of this sensor is to automatically log soil moisture levels through Bluetooth Low Energy (BLE) readings. A corresponding library for interfacing with this sensor is available in the libs directory.

The sensor returns moisture values as float64 data types, ranging from 0 to 1, where a value of 1 indicates the presence of water, and 0 represents completely dry soil.

## Hardware
* Arduino Nano ESP32
* MODMYPI Soil Moisture Sensor Kit
* Power source