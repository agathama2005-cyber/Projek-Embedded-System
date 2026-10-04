```cpp
#include <Wire.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <OneWire.h>
#include <DallasTemperature.h>

// =====================
// WIFI SETUP
// =====================
const char* WIFI_SSID = "YOUR_WIFI_SSID";
const char* WIFI_PASSWORD = "YOUR_WIFI_PASSWORD";

// =====================
// MQTT SETUP
// =====================
const char* MQTT_HOST = "YOUR_MQTT_HOST";
const int MQTT_PORT = 8883;
const char* MQTT_USERNAME = "YOUR_MQTT_USERNAME";
const char* MQTT_PASSWORD = "YOUR_MQTT_PASSWORD";
const char* MQTT_TOPIC_SENSOR = "YOUR_MQTT_TOPIC";

// =====================
// MQTT INTERVAL
// =====================

// AMAN publish tiap 30 detik
const unsigned long MQTT_NORMAL_INTERVAL = 30000;

// ALERT dan BAHAYA publish tiap 1 detik
const unsigned long MQTT_ALERT_INTERVAL = 1000;
const unsigned long MQTT_DANGER_INTERVAL = 1000;

// Reconnect WiFi setiap 10 detik jika gagal
const unsigned long WIFI_RECONNECT_INTERVAL = 10000;

// Reconnect MQTT setiap 5 detik jika gagal
const unsigned long MQTT_RECONNECT_INTERVAL = 5000;

// =====================
// PIN SETUP
// =====================
#define PIN_DS18B20 4
#define PIN_MQ2 35
#define PIN_FLAME 18
#define PIN_LED_R 12
#define PIN_LED_G 14
#define PIN_LED_Y 13
#define PIN_BUZZER 19

// Banyak flame sensor module aktif LOW
// LOW = api terdeteksi
// HIGH = tidak ada api
#define FLAME_DETECTED LOW

// =====================
// THRESHOLD SENSOR
// =====================
const int mq2AlertThreshold = 3950;
const float tempAlertThreshold = 40.0;

const int mq2DangerThreshold = 4090;
const float tempDangerThreshold = 60.0;

// =====================
// DS18B20 SETUP
// =====================
OneWire oneWire(PIN_DS18B20);
DallasTemperature sensors(&oneWire);

// =====================
// OLED SETUP
// =====================
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1

Adafruit_SSD1306 display(
    SCREEN_WIDTH,
    SCREEN_HEIGHT,
    &Wire,
    OLED_RESET
);

// =====================
// MQTT CLIENT SETUP
// =====================
WiFiClientSecure secureClient;
PubSubClient mqttClient(secureClient);

unsigned long lastMqttPublish = 0;
unsigned long lastMqttReconnectAttempt = 0;
unsigned long lastWiFiReconnectAttempt = 0;

String previousStateText = "";

// =====================
// STATE MACHINE
// =====================
enum SystemState {
    STATE_NORMAL,
    STATE_ALERT,
    STATE_EMERGENCY
};

SystemState currentState = STATE_NORMAL;

// =====================
// BUZZER TIMING
// =====================
unsigned long lastBuzzerToggle = 0;
bool buzzerOn = false;

// =====================
// LED CONTROL
// =====================
void turnOffAllLed() {
    digitalWrite(PIN_LED_R, LOW);
    digitalWrite(PIN_LED_Y, LOW);
    digitalWrite(PIN_LED_G, LOW);
}

void setLedNormal() {
    turnOffAllLed();
    digitalWrite(PIN_LED_G, HIGH);
}

void setLedAlert() {
    turnOffAllLed();
    digitalWrite(PIN_LED_Y, HIGH);
}

void setLedEmergency() {
    turnOffAllLed();
    digitalWrite(PIN_LED_R, HIGH);
}

// =====================
// STATUS TEXT
// =====================
String getStateText(SystemState state) {
    switch (state) {
        case STATE_NORMAL:
            return "AMAN";

        case STATE_ALERT:
            return "ALERT";

        case STATE_EMERGENCY:
            return "BAHAYA";

        default:
            return "UNKNOWN";
    }
}

// =====================
// WIFI CONNECT
// =====================
void connectWiFi() {
    if (WiFi.status() == WL_CONNECTED) {
        return;
    }

    Serial.println();
    Serial.print("Connecting to WiFi: ");
    Serial.println(WIFI_SSID);

    WiFi.mode(WIFI_STA);

    // Jangan panggil WiFi.disconnect(true) terus-menerus di loop.
    // Cukup begin ulang berdasarkan interval reconnect.
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
}

// =====================
// WIFI STATUS CHECK
// =====================
void handleWiFiConnection() {
    if (WiFi.status() == WL_CONNECTED) {
        return;
    }

    unsigned long now = millis();

    if (now - lastWiFiReconnectAttempt >= WIFI_RECONNECT_INTERVAL) {
        lastWiFiReconnectAttempt = now;
        connectWiFi();
    }
}

// =====================
// MQTT CONNECT
// =====================
bool connectMQTT() {
    if (WiFi.status() != WL_CONNECTED) {
        return false;
    }

    if (mqttClient.connected()) {
        return true;
    }

    Serial.println("Connecting to MQTT broker...");

    String clientId = "firesentry-";
    clientId += String((uint32_t)ESP.getEfuseMac(), HEX);

    bool connected = mqttClient.connect(
        clientId.c_str(),
        MQTT_USERNAME,
        MQTT_PASSWORD
    );

    if (connected) {
        Serial.println("MQTT connected!");
    } else {
        Serial.print("MQTT connection failed, rc=");
        Serial.println(mqttClient.state());
    }

    return connected;
}

// =====================
// MQTT STATUS CHECK
// =====================
void handleMQTTConnection() {
    if (WiFi.status() != WL_CONNECTED) {
        return;
    }

    if (mqttClient.connected()) {
        mqttClient.loop();
        return;
    }

    unsigned long now = millis();

    if (now - lastMqttReconnectAttempt >= MQTT_RECONNECT_INTERVAL) {
        lastMqttReconnectAttempt = now;
        connectMQTT();
    }
}

// =====================
// PUBLISH MQTT PAYLOAD
// =====================
void publishSensorData(
    int mq2Value,
    float tempC,
    bool flameDetected,
    bool tempError,
    bool gasAlert,
    bool gasDanger,
    bool tempAlert,
    bool tempDanger,
    String stateText
) {
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("MQTT publish skipped: WiFi not connected");
        return;
    }

    if (!mqttClient.connected()) {
        Serial.println("MQTT publish skipped: MQTT not connected");
        return;
    }

    String payload = "{";

    payload += "\"gas\":";
    payload += String(mq2Value);
    payload += ",";

    payload += "\"temperature\":";

    if (tempError) {
        payload += "null";
    } else {
        payload += String(tempC, 2);
    }

    payload += ",";

    payload += "\"flameDetected\":";
    payload += flameDetected ? "true" : "false";
    payload += ",";

    payload += "\"tempError\":";
    payload += tempError ? "true" : "false";
    payload += ",";

    payload += "\"gasAlert\":";
    payload += gasAlert ? "true" : "false";
    payload += ",";

    payload += "\"gasDanger\":";
    payload += gasDanger ? "true" : "false";
    payload += ",";

    payload += "\"tempAlert\":";
    payload += tempAlert ? "true" : "false";
    payload += ",";

    payload += "\"tempDanger\":";
    payload += tempDanger ? "true" : "false";
    payload += ",";

    payload += "\"state\":\"";
    payload += stateText;
    payload += "\"";

    payload += "}";

    bool success = mqttClient.publish(
        MQTT_TOPIC_SENSOR,
        payload.c_str()
    );

    if (success) {
        Serial.print("MQTT Published: ");
        Serial.println(payload);
    } else {
        Serial.println("MQTT publish failed");
    }
}

// =====================
// OLED UPDATE
// =====================
void updateOLED(
    int mq2Value,
    float tempC,
    bool flameDetected,
    bool tempError,
    String stateText
) {
    display.clearDisplay();

    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 0);

    display.println("FIRESENTRY");
    display.println("----------------");

    display.print("Gas : ");
    display.println(mq2Value);

    display.print("Temp : ");

    if (tempError) {
        display.println("ERROR");
    } else {
        display.print(tempC, 2);
        display.println(" C");
    }

    display.print("Flame : ");
    display.println(flameDetected ? "YES" : "NO");

    display.print("Status: ");
    display.println(stateText);

    display.print("MQTT : ");
    display.println(mqttClient.connected() ? "OK" : "OFF");

    display.display();
}

// =====================
// GET MQTT INTERVAL BY STATE
// =====================
unsigned long getMqttIntervalByState(String stateText) {
    if (stateText == "AMAN") {
        return MQTT_NORMAL_INTERVAL;
    }

    if (stateText == "ALERT") {
        return MQTT_ALERT_INTERVAL;
    }

    if (stateText == "BAHAYA") {
        return MQTT_DANGER_INTERVAL;
    }

    return MQTT_NORMAL_INTERVAL;
}

// =====================
// SETUP
// =====================
void setup() {
    Serial.begin(115200);
    delay(1000);

    Serial.println();
    Serial.println("FIRESENTRY Integrated System");
    Serial.println("Mode: MQTT Hybrid Interval");
    Serial.println("AMAN: 30 detik | ALERT/BAHAYA: 1 detik");

    // OLED I2C ESP32 default:
    // SDA = GPIO 21
    // SCL = GPIO 22
    Wire.begin(21, 22);

    if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
        Serial.println(
            "OLED tidak terdeteksi. Cek alamat I2C / wiring SDA SCL."
        );

        while (true) {
            delay(100);
        }
    }

    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);

    display.setCursor(0, 0);
    display.println("FIRESENTRY");
    display.println("Initializing...");
    display.display();

    // DS18B20
    sensors.begin();
    sensors.setResolution(10);

    int jumlahSensor = sensors.getDeviceCount();

    Serial.print("Jumlah DS18B20 terdeteksi: ");
    Serial.println(jumlahSensor);

    // Pin mode sensor
    pinMode(PIN_MQ2, INPUT);
    pinMode(PIN_FLAME, INPUT);

    // Pin mode output
    pinMode(PIN_BUZZER, OUTPUT);
    pinMode(PIN_LED_R, OUTPUT);
    pinMode(PIN_LED_Y, OUTPUT);
    pinMode(PIN_LED_G, OUTPUT);

    digitalWrite(PIN_BUZZER, LOW);
    turnOffAllLed();

    // TLS MQTT
    // Untuk testing, setInsecure lebih mudah.
    // Untuk produksi, sebaiknya gunakan CA certificate.
    secureClient.setInsecure();

    mqttClient.setServer(MQTT_HOST, MQTT_PORT);
    mqttClient.setBufferSize(512);

    // Connect WiFi awal
    connectWiFi();
    delay(1000);
}

// =====================
// LOOP
// =====================
void loop() {

    // =====================
    // 1. Jaga koneksi WiFi & MQTT
    // =====================
    handleWiFiConnection();

    if (WiFi.status() == WL_CONNECTED) {
        handleMQTTConnection();
    }

    // =====================
    // 2. READ SENSOR
    // =====================
    sensors.requestTemperatures();

    float tempC = sensors.getTempCByIndex(0);
    int mq2Value = analogRead(PIN_MQ2);
    int flameState = digitalRead(PIN_FLAME);

    bool flameDetected = (flameState == FLAME_DETECTED);
    bool tempError = (tempC == DEVICE_DISCONNECTED_C);

    // Kalau DS18B20 error, suhu tidak dipakai untuk pengambilan keputusan
    bool tempAlert = (!tempError && tempC > tempAlertThreshold);
    bool tempDanger = (!tempError && tempC > tempDangerThreshold);

    bool gasAlert = (mq2Value > mq2AlertThreshold);
    bool gasDanger = (mq2Value > mq2DangerThreshold);

    // =====================
    // 3. STATE DECISION
    // BAHAYA dicek dulu karena paling prioritas
    // =====================
    if (flameDetected || gasDanger || tempDanger) {
        currentState = STATE_EMERGENCY;
    } else if (gasAlert || tempAlert) {
        currentState = STATE_ALERT;
    } else {
        currentState = STATE_NORMAL;
    }

    String stateText = getStateText(currentState);

    // =====================
    // 4. OUTPUT CONTROL
    // =====================
    unsigned long now = millis();

    switch (currentState) {

        case STATE_NORMAL:
            setLedNormal();
            digitalWrite(PIN_BUZZER, LOW);
            buzzerOn = false;
            break;

        case STATE_ALERT:
            setLedAlert();

            // Buzzer lambat: beep setiap 500 ms
            if (now - lastBuzzerToggle >= 500) {
                buzzerOn = !buzzerOn;
                digitalWrite(
                    PIN_BUZZER,
                    buzzerOn ? HIGH : LOW
                );
                lastBuzzerToggle = now;
            }
            break;

        case STATE_EMERGENCY:
            setLedEmergency();

            // Buzzer cepat: beep setiap 150 ms
            if (now - lastBuzzerToggle >= 150) {
                buzzerOn = !buzzerOn;
                digitalWrite(
                    PIN_BUZZER,
                    buzzerOn ? HIGH : LOW
                );
                lastBuzzerToggle = now;
            }
            break;
    }

    // =====================
    // 5. OLED DISPLAY
    // =====================
    updateOLED(
        mq2Value,
        tempC,
        flameDetected,
        tempError,
        stateText
    );

    // =====================
    // 6. SERIAL DEBUG
    // =====================
    Serial.print("WiFi: ");
    Serial.print(
        WiFi.status() == WL_CONNECTED ? "OK" : "OFF"
    );

    Serial.print(" | MQTT: ");
    Serial.print(
        mqttClient.connected() ? "OK" : "OFF"
    );

    Serial.print(" | Gas: ");
    Serial.print(mq2Value);

    Serial.print(" | Temp: ");

    if (tempError) {
        Serial.print("ERROR");
    } else {
        Serial.print(tempC, 2);
        Serial.print(" C");
    }

    Serial.print(" | Flame: ");
    Serial.print(flameDetected ? "YES" : "NO");

    Serial.print(" | gasAlert: ");
    Serial.print(gasAlert ? "true" : "false");

    Serial.print(" | gasDanger: ");
    Serial.print(gasDanger ? "true" : "false");

    Serial.print(" | tempAlert: ");
    Serial.print(tempAlert ? "true" : "false");

    Serial.print(" | tempDanger: ");
    Serial.print(tempDanger ? "true" : "false");

    Serial.print(" | Status: ");
    Serial.println(stateText);

    // =====================
    // 7. MQTT PUBLISH HYBRID
    // AMAN -> tiap 30 detik
    // ALERT -> tiap 1 detik
    // BAHAYA -> tiap 1 detik
    // Jika state berubah -> publish langsung
    // =====================
    bool stateChanged = (stateText != previousStateText);

    unsigned long mqttInterval =
        getMqttIntervalByState(stateText);

    if (mqttClient.connected()) {

        if (
            stateChanged ||
            (millis() - lastMqttPublish >= mqttInterval)
        ) {
            publishSensorData(
                mq2Value,
                tempC,
                flameDetected,
                tempError,
                gasAlert,
                gasDanger,
                tempAlert,
                tempDanger,
                stateText
            );

            lastMqttPublish = millis();
            previousStateText = stateText;
        }
    }

    delay(50);
}
```
