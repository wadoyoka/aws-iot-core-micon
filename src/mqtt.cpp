#include "mqtt.h"

#include <Arduino.h>
#include <ArduinoJson.h>
#include <PubSubClient.h>
#include <WiFiClientSecure.h>

#include "camera.h"
#include "config.h"

static WiFiClientSecure espClient;
static PubSubClient mqttClient(espClient);

static String captureTopic;
static String imagesTopicPrefix;
static String eventsTopic;

static void publishEvent(const String& captureId, const char* status, size_t bytes,
                         const char* error) {
  JsonDocument doc;
  doc["captureId"] = captureId;
  doc["status"] = status;
  doc["bytes"] = bytes;
  if (error != nullptr) doc["error"] = error;
  String json;
  serializeJson(doc, json);
  mqttClient.publish(eventsTopic.c_str(), json.c_str());
  Serial.printf("Event: %s\n", json.c_str());
}

// 撮影して <prefix>/<thing>/images/<captureId> に JPEG をそのまま publish する。
// IoT Core の上限 128 KB を超えたら画質を下げて撮り直す。
static void captureAndPublish(const String& captureId) {
  String topic = imagesTopicPrefix + captureId;

  for (int quality : JPEG_QUALITY_STEPS) {
    camera_fb_t* fb = camera::capture(quality);
    if (fb == nullptr) {
      publishEvent(captureId, "failed", 0, "camera capture failed");
      return;
    }

    // MQTT のメッセージサイズ = トピック長 (2 バイト) + トピック + ペイロード
    size_t messageBytes = 2 + topic.length() + fb->len;
    if (messageBytes > MQTT_MAX_MESSAGE_BYTES) {
      Serial.printf("Too large: %u bytes (quality=%d), retry with lower quality\n",
                    (unsigned)messageBytes, quality);
      camera::release(fb);
      continue;
    }

    // publish() は内部バッファ（最大 64 KB）を通すので、ソケットへ直接書く
    size_t bytes = fb->len;
    bool ok = mqttClient.beginPublish(topic.c_str(), bytes, false) &&
              mqttClient.write(fb->buf, bytes) == bytes && mqttClient.endPublish();
    camera::release(fb);

    Serial.printf("Image publish %s: %s (%u bytes, quality=%d)\n", ok ? "OK" : "failed",
                  topic.c_str(), (unsigned)bytes, quality);
    publishEvent(captureId, ok ? "published" : "failed", bytes,
                 ok ? nullptr : "mqtt publish failed");
    return;
  }

  publishEvent(captureId, "failed", 0, "image exceeds MQTT message limit");
}

static void mqttCallback(char* topic, byte* payload, unsigned int length) {
  Serial.printf("Message on %s: %.*s\n", topic, (int)length, (const char*)payload);

  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, (const char*)payload, length);
  if (error) {
    Serial.printf("deserializeJson() failed: %s\n", error.c_str());
    return;
  }

  // payload は PubSubClient の内部バッファ。送信で上書きされる前に String へコピーする
  String captureId = doc["captureId"] | "";

  // captureId はトピックの 1 階層になるので、英数字と - だけを許す
  bool valid = captureId.length() > 0 && captureId.length() <= 64;
  for (char c : captureId) valid = valid && (isalnum((unsigned char)c) || c == '-');
  if (!valid) {
    Serial.printf("Invalid captureId: %s\n", captureId.c_str());
    return;
  }

  digitalWrite(LED_PIN, LOW);  // 撮影・送信中は LED 点灯（アクティブ LOW）
  captureAndPublish(captureId);
  digitalWrite(LED_PIN, HIGH);
}

void initMqtt() {
  String base = String(TOPIC_PREFIX) + "/" + THING_NAME;
  captureTopic = base + "/capture";
  imagesTopicPrefix = base + "/images/";
  eventsTopic = base + "/events";

  espClient.setCACert(AWS_ROOT_CA);
  espClient.setCertificate(DEVICE_CERT);
  espClient.setPrivateKey(DEVICE_PRIVATE_KEY);

  mqttClient.setServer(AWS_IOT_ENDPOINT, MQTT_PORT);
  mqttClient.setBufferSize(MQTT_BUFFER_SIZE);
  mqttClient.setSocketTimeout(30);
  mqttClient.setCallback(mqttCallback);
}

static void mqttReconnect() {
  int retries = 0;
  const int maxRetries = 10;
  while (!mqttClient.connected() && retries < maxRetries) {
    Serial.print("Attempting MQTT connection...");
    // クライアント ID は Thing 名と同じでなければならない（IoT ポリシーの条件）
    if (mqttClient.connect(THING_NAME)) {
      Serial.println("connected");
      mqttClient.subscribe(captureTopic.c_str(), 1);
      Serial.printf("Subscribed: %s\n", captureTopic.c_str());
    } else {
      retries++;
      // rc の意味は README の「MQTT の接続失敗」を参照
      Serial.printf("failed, rc=%d try again in 5 seconds (%d/%d)\n", mqttClient.state(),
                    retries, maxRetries);
      delay(5000);
    }
  }
  if (!mqttClient.connected()) {
    Serial.println("MQTT connection failed after max retries, restarting...");
    ESP.restart();
  }
}

void mqttLoop() {
  if (!mqttClient.connected()) {
    mqttReconnect();
  }
  mqttClient.loop();
}
