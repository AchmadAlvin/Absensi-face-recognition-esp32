#include "esp_camera.h"
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>

#define CAMERA_MODEL_AI_THINKER // Has PSRAM
#include "camera_pins.h"

// wifi
const char *ssid = "Woy";
const char *password = "eek kowe";

// MQTT Configuration
const char* mqtt_server = "broker.emqx.io";
const char* mqtt_topic = "la/server/url";
String serverUrl = "";

WiFiClient espClient;
PubSubClient client(espClient);

// config cam32
void initCamera() {
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sccb_sda = SIOD_GPIO_NUM;
  config.pin_sccb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;  // for streaming
  config.frame_size = FRAMESIZE_QVGA;
  config.jpeg_quality = 10;
  config.fb_count = 2;

  // Initialize camera
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
    ESP.restart();
  } else {
    Serial.println("Camera initialized successfully!");

    // Adjust sensor (opsional)
    sensor_t *s = esp_camera_sensor_get();
    s->set_vflip(s, 1);  // Flip vertical
    s->set_hmirror(s, 1); // Mirror horizontal
  }
}

// ===========================
// WiFi Connection
// ===========================
// void connectWiFi() {
//   WiFi.begin(ssid, password);
//   WiFi.setSleep(false);

//   Serial.print("Connecting to WiFi...");
//   while (WiFi.status() != WL_CONNECTED) {
//     delay(500);
//     Serial.print(".");
//   }
//   Serial.println("\nWiFi connected!");
// }
void connectWiFi() {
  WiFi.disconnect(); // Putuskan koneksi dari jaringan lain
  WiFi.mode(WIFI_STA); // Pastikan mode yang digunakan hanya sebagai station
  WiFi.begin(ssid, password);
  WiFi.setSleep(false);

  Serial.print("Connecting to WiFi...");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected!");
}


// MQTT Callback and Connection
void callback(char* topic, byte* payload, unsigned int length) {
  payload[length] = '\0';
  serverUrl = String((char*)payload);
  Serial.println("URL received from MQTT: " + serverUrl);
}

void connectMQTT() {
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);

  while (!client.connected()) {
    Serial.print("Connecting to MQTT...");
    if (client.connect("ESP32-CAM")) {
      Serial.println("Connected!");
      client.subscribe(mqtt_topic);
    } else {
      Serial.print("Failed, rc=");
      Serial.print(client.state());
      Serial.println(" Retrying in 5 seconds...");
      delay(5000);
    }
  }
}

//kirim foto ke server
void sendPhoto() {
  if (serverUrl == "") {
    Serial.println("Server URL not received from MQTT!");
    return;
  }

  camera_fb_t *fb = esp_camera_fb_get();
  if (!fb) {
    Serial.println("Failed to capture image");
    return;
  }

  Serial.println("Sending image to server...");
  HTTPClient http;
  http.begin(serverUrl);
  http.addHeader("Content-Type", "image/jpeg");

  int httpResponseCode = http.POST(fb->buf, fb->len);
  if (httpResponseCode > 0) {
    Serial.print("Image sent successfully! Response: ");
    Serial.println(http.getString());
  } else {
    Serial.print("Failed to send image, error code: ");
    Serial.println(httpResponseCode);
  }

  http.end();
  esp_camera_fb_return(fb);
}

// ===========================
// Setup Function
// ===========================
void setup() {
  Serial.begin(115200);
  Serial.setDebugOutput(true);
  Serial.println();

  // Initialize WiFi, MQTT, and Camera
  connectWiFi();
  connectMQTT();
  initCamera();
}

// ===========================
// Loop Function
// ===========================
void loop() {
  if (!client.connected()) {
    connectMQTT();
  }
  client.loop();

  // Capture and send photo every 5 seconds
  static unsigned long lastCaptureTime = 0;
  if (millis() - lastCaptureTime >= 5000) {
    lastCaptureTime = millis();
    sendPhoto();
  }
}