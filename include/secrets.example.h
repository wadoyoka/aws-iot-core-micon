// secrets.h のテンプレート。
// このファイルを include/secrets.h にコピーして実際の値を埋めてください。
//   cp include/secrets.example.h include/secrets.h
#pragma once

// ---- Wi-Fi ----------------------------------------------------------------
// ESP32-S3 は 2.4GHz 帯のみ対応
#define WIFI_SSID     "your-ssid"
#define WIFI_PASSWORD "your-password"

// ---- AWS IoT Core ---------------------------------------------------------
#define AWS_IOT_ENDPOINT "xxxxxxxxxxxxxx-ats.iot.ap-northeast-1.amazonaws.com"

// MQTT クライアント ID はこの値と一致している必要がある（IoT ポリシーで制限）
#define THING_NAME "xiao-camera"

// 購読: <TOPIC_PREFIX>/<THING_NAME>/capture
// 発行: <TOPIC_PREFIX>/<THING_NAME>/images/<captureId>（JPEG）, <TOPIC_PREFIX>/<THING_NAME>/events
#define TOPIC_PREFIX "cameras"

// ---- 証明書 ---------------------------------------------------------------
// AmazonRootCA1.pem の中身をそのまま貼り付け
// （IoT Core の ATS エンドポイントの検証に使用）
static const char AWS_ROOT_CA[] = R"EOF(
-----BEGIN CERTIFICATE-----
...AmazonRootCA1.pem の内容...
-----END CERTIFICATE-----
)EOF";

// デバイス証明書（xxxxxxxx-certificate.pem.crt）
static const char DEVICE_CERT[] = R"EOF(
-----BEGIN CERTIFICATE-----
...デバイス証明書の内容...
-----END CERTIFICATE-----
)EOF";

// デバイス秘密鍵（xxxxxxxx-private.pem.key）
static const char DEVICE_PRIVATE_KEY[] = R"EOF(
-----BEGIN RSA PRIVATE KEY-----
...秘密鍵の内容...
-----END RSA PRIVATE KEY-----
)EOF";
