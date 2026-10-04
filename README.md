# FireSentry: Mitigasi Kebakaran Dini Berbasis IoT

Project matkul Embedded System, FILKOM UB 2026.
Sistem deteksi kebakaran dini berbasis ESP32 dengan sensor fusion (Flame IR, MQ-2, DS18B20),
peringatan lokal (LED, buzzer, OLED), dan monitoring jarak jauh lewat MQTT dan Telegram.

## Arsitektur
Sensor → ESP32 (sensor fusion + state machine) → LED, buzzer, OLED
                                               → WiFi → MQTT Broker (HiveMQ Cloud, TLS 8883) → Dashboard dan Telegram Bot

## Status Sistem
| Status | Pemicu | Output lokal | Interval publish MQTT |
|---|---|---|---|
| AMAN | Semua parameter di bawah threshold | LED hijau | 30 detik |
| ALERT | Gas > 3950 atau suhu > 40°C | LED kuning, buzzer 500 ms | 1 detik |
| BAHAYA | Api terdeteksi, gas > 4090, atau suhu > 60°C | LED merah, buzzer 150 ms | 1 detik |

Publish juga dilakukan langsung setiap kali status berubah.

## Fitur
- State machine tiga level dengan prioritas BAHAYA > ALERT > AMAN
- Adaptive publish interval untuk menghemat bandwidth
- Auto-reconnect WiFi dan MQTT non-blocking (berbasis `millis()`)
- Payload JSON terstruktur (gas, suhu, flame, flag threshold, status)
- Deteksi sensor suhu error (DS18B20 terputus tidak dipakai untuk keputusan)
- Notifikasi Telegram: [isi cara integrasinya, misal "data dari MQTT diteruskan ke Telegram Bot lewat ..."]

## Komponen
ESP32 DevKit, Flame IR Sensor (KY-026), MQ-2, DS18B20, OLED 0,96" I2C (SSD1306),
LED merah/kuning/hijau, buzzer, resistor 220Ω dan 4,7kΩ (pull-up OneWire), breadboard

## Wiring
| Komponen | Pin ESP32 |
|---|---|
| DS18B20 (OneWire) | GPIO 4 |
| MQ-2 (analog) | GPIO 35 |
| Flame sensor (digital) | GPIO 18 |
| LED merah / hijau / kuning | GPIO 12 / 14 / 13 |
| Buzzer | GPIO 19 |
| OLED I2C | SDA 21 / SCL 22 |

[Skematik]<img width="992" height="858" alt="image" src="https://github.com/user-attachments/assets/a10dcdfe-f615-4064-abff-a8e78631e10c" />


## Desain Enclosure 3D
Enclosure didesain di Fusion 360. DS18B20 sengaja dipasang di luar kotak agar mengukur suhu
lingkungan, bukan panas komponen elektronik. MQ-2 dan flame sensor ada di bagian atas,
OLED dan tiga LED indikator di tengah.

![Render 3D](docs/enclosure-3d.png)

![Hasil jadi] 
<img width="747" height="424" alt="image" src="https://github.com/user-attachments/assets/bd3d793a-bf16-4311-954f-fc796922545c" />

## Hasil Pengujian
| Pengujian | Hasil |
|---|---|
| Deteksi api | 0,40 detik pada 5 cm, efektif sampai 30 cm |
| Latensi MQTT | rata-rata ±1,09 detik (success rate 95,5%) |
| Latensi Telegram Bot API | rata-rata ±5,2 detik |
| Reliability | stabil 3 jam operasi kontinu tanpa crash |

## Library
`WiFi`, `WiFiClientSecure`, `PubSubClient`, `Adafruit_GFX`, `Adafruit_SSD1306`, `OneWire`, `DallasTemperature`

## Konfigurasi
Salin `secrets.example.h` jadi `secrets.h`, lalu isi kredensial WiFi dan MQTT milikmu.

## Kontribusi Saya
Merancang wiring seluruh rangkaian dan mendesain enclosure 3D di Fusion 360.

## Tim
Rezky Auliasarie, Agatha Triotama, Zeyra Rahma Alivia, Muhammad Naufal M. R, Delvin Nhean Olamina
