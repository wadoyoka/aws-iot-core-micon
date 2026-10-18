// カメラ (OV2640 / OV3660) の初期化と JPEG 撮影
#pragma once

#include <esp_camera.h>

namespace camera {

// カメラを初期化する。成功で true。
bool begin();

// 指定した JPEG 画質（0-63、大きいほど低画質・小さいファイル）で新しいフレームを 1 枚撮影する。
// 古いフレームを捨ててから撮るので、直前の状態ではなく「今」の画像になる。
// 使い終わったら必ず release() で返却すること。失敗時は nullptr。
camera_fb_t* capture(int jpegQuality);

void release(camera_fb_t* fb);

}  // namespace camera
