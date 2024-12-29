# Cardputer_Weather_Env4
Cardputer weather station using OpenWeather and the M5Stack Env4 sensor.

This program combines local weather information from OpenWeather with local sensor data from a M5 Env4 sensor. The information is displayed in pages begining with the Open Wearther information and then the Env4 readings.

Notes:
1) An OpenWeather account & API is required for the weatherdata. They offer a free account for usage of less than 1000 calls per day. This program will make fewer calls than that if left on continuously for 24 hours, but if modified in screen duration or number of screens, the number of calls could be affected. Set your daily limit to less than 1000 calls per day to avoid any charges. https://home.openweathermap.org/ to get an API Key.
2) The API key and other needed information like WiFi info are stored in the OpenWeather.txt file stored on the Cardputer SD card.
