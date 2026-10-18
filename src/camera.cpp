#include "camera.h"

#include <Arduino.h>

#include "config.h"

namespace {

// XIAO ESP32S3 Sense のカメラピン配置
constexpr int PIN_PWDN = -1;
constexpr int PIN_RESET = -1;
constexpr int PIN_XCLK = 10;
constexpr int PIN_SIOD = 40;
constexpr int PIN_SIOC = 39;
constexpr int PIN_Y9 = 48;
constexpr int PIN_Y8 = 11;
constexpr int PIN_Y7 = 12;
constexpr int PIN_Y6 = 14;
constexpr int PIN_Y5 = 16;
constexpr int PIN_Y4 = 18;
constexpr int PIN_Y3 = 17;
constexpr int PIN_Y2 = 15;
constexpr int PIN_VSYNC = 38;
constexpr int PIN_HREF = 47;
constexpr int PIN_PCLK = 13;

}  // namespace

namespace camera {

bool begin() {
  camera_config_t config = {};
  config.pin_pwdn = PIN_PWDN;
  config.pin_reset = PIN_RESET;
  config.pin_xclk = PIN_XCLK;
  config.pin_sccb_sda = PIN_SIOD;
  config.pin_sccb_scl = PIN_SIOC;
  config.pin_d7 = PIN_Y9;
  config.pin_d6 = PIN_Y8;
  config.pin_d5 = PIN_Y7;
  config.pin_d4 = PIN_Y6;
  config.pin_d3 = PIN_Y5;
  config.pin_d2 = PIN_Y4;
  config.pin_d1 = PIN_Y3;
  config.pin_d0 = PIN_Y2;
  config.pin_vsync = PIN_VSYNC;
  config.pin_href = PIN_HREF;
  config.pin_pclk = PIN_PCLK;
  config.xclk_freq_hz = 20000000;
  config.ledc_timer = LEDC_TIMER_0;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.pixel_format = PIXFORMAT_JPEG;

  // MQTT（1 メッセージ 128 KB まで）に載せるので VGA に抑える。
  // 1 バッファ + WHEN_EMPTY にして、画質を変えたあと古い設定のフレームが残らないようにする
  config.frame_size = FRAMESIZE_VGA;  // 640x480
  config.jpeg_quality = JPEG_QUALITY_STEPS[0];
  config.fb_count = 1;
  config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;
  if (psramFound()) {
    config.fb_location = CAMERA_FB_IN_PSRAM;
  } else {
    Serial.println("[camera] PSRAM が見つかりません。フレームバッファを DRAM に置きます");
    config.fb_location = CAMERA_FB_IN_DRAM;
  }

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("[camera] esp_camera_init 失敗: 0x%x\n", err);
    return false;
  }

  sensor_t* s = esp_camera_sensor_get();
  if (s != nullptr) {
    Serial.printf("[camera] センサー PID=0x%x\n", s->id.PID);
    if (s->id.PID == OV3660_PID) {
      // OV3660 は初期状態だと上下反転・彩度が強めなので補正
      s->set_vflip(s, 1);
      s->set_brightness(s, 1);
      s->set_saturation(s, -2);
    }
  }

  // 起動直後の数フレームは露出/ホワイトバランスが安定しないので捨てる
  for (int i = 0; i < 3; i++) {
    camera_fb_t* fb = esp_camera_fb_get();
    if (fb) esp_camera_fb_return(fb);
  }
  Serial.println("[camera] 初期化完了");
  return true;
}

camera_fb_t* capture(int jpegQuality) {
  sensor_t* s = esp_camera_sensor_get();
  if (s != nullptr) s->set_quality(s, jpegQuality);

  // 古いフレーム（バッファに残っていたもの・前の画質のもの）を 1 枚捨てる
  camera_fb_t* stale = esp_camera_fb_get();
  if (stale) esp_camera_fb_return(stale);

  camera_fb_t* fb = esp_camera_fb_get();
  if (fb == nullptr) {
    Serial.println("[camera] フレーム取得に失敗");
    return nullptr;
  }
  if (fb->format != PIXFORMAT_JPEG) {
    Serial.println("[camera] JPEG 以外のフォーマットが返されました");
    esp_camera_fb_return(fb);
    return nullptr;
  }
  Serial.printf("[camera] 撮影: %ux%u, quality=%d, %u bytes\n", fb->width, fb->height, jpegQuality,
                (unsigned)fb->len);
  return fb;
}

void release(camera_fb_t* fb) {
  if (fb) esp_camera_fb_return(fb);
}

}  // namespace camera
