// XIAO ESP32S3 Sense: AWS IoT Core からの撮影コマンドで JPEG を撮り、MQTT でそのまま送る
//
// 流れ:
//   1. Wi-Fi 接続
//   2. AWS IoT Core に MQTT/TLS (8883) で接続し、<prefix>/<thing>/capture を購読
//   3. コマンド受信 → 撮影 → 128 KB 以内か確認 → <prefix>/<thing>/images/<captureId> に発行
//      （IoT ルールが S3 の captures/<thing>/<captureId>.jpg に保存する）
//   4. 結果を <prefix>/<thing>/events に発行
#include <Arduino.h>
#include <WiFi.h>

#include "camera.h"
#include "config.h"
#include "mqtt.h"

static void connectWiFi() {
  Serial.printf("Connecting to WiFi %s", WIFI_SSID);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  int retries = 0;
  while (WiFi.status() != WL_CONNECTED) {
    if (++retries > 60) {  // 30 秒つながらなければ再起動
      Serial.println("\nWiFi connection failed, restarting...");
      ESP.restart();
    }
    delay(500);
    Serial.print(".");
  }
  Serial.printf("\nWiFi connected: %s\n", WiFi.localIP().toString().c_str());
}

void setup() {
  Serial.begin(115200);
  delay(1000);  // USB CDC のシリアルモニタ接続待ち
  Serial.printf("\n=== aws-iot-core-micon (thing=%s) ===\n", THING_NAME);

  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, HIGH);  // 消灯（アクティブ LOW）

  if (!camera::begin()) {
    Serial.println("Camera init failed, restarting in 10 seconds...");
    delay(10000);
    ESP.restart();
  }

  connectWiFi();
  // 時刻合わせ（同期は待たない。証明書の日付チェックが有効な環境向けの保険）
  configTime(0, 0, "ntp.nict.jp", "time.google.com");

  initMqtt();
}

void loop() {
  // 切れた Wi-Fi は ESP32 が自動で再接続する。MQTT は mqttLoop() が張り直す
  mqttLoop();
  delay(10);
}
