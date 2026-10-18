// 共通設定と secrets.h の読み込み
#pragma once

#if __has_include("secrets.h")
#include "secrets.h"
#else
#error "include/secrets.h がありません。include/secrets.example.h をコピーして値を埋めてください: cp include/secrets.example.h include/secrets.h"
#endif

#include <Arduino.h>

// オンボード LED（XIAO ESP32S3 はオレンジ LED が GPIO21、アクティブ LOW）
constexpr int LED_PIN = 21;

// MQTT
constexpr uint16_t MQTT_PORT = 8883;
// 受信する撮影コマンドは小さい JSON。画像の送信はバッファを通さずストリームで書くので、
// このサイズに縛られない
constexpr uint16_t MQTT_BUFFER_SIZE = 1024;
// AWS IoT Core の 1 メッセージの上限（128 KB）。送信前に「トピック + ペイロード」がこれ以下か確認する
constexpr size_t MQTT_MAX_MESSAGE_BYTES = 128 * 1024;

// 画像は MQTT に載せるので小さめに撮る（VGA 640x480 で 30〜60 KB 程度）。
// 128 KB を超えたら JPEG の画質を順に下げて撮り直す（0-63、大きいほど低画質・小さいファイル）
constexpr int JPEG_QUALITY_STEPS[] = {12, 20, 30, 45};
