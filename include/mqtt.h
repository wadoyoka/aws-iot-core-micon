// AWS IoT Core との MQTT 接続（TLS / X.509 クライアント証明書）。
// <prefix>/<thing>/capture で撮影コマンドを受けたら、撮影して
// <prefix>/<thing>/images/<captureId> に JPEG を publish し、結果を <prefix>/<thing>/events に送る。
#pragma once

void initMqtt();

// loop() から毎回呼ぶ。切断していたら再接続する（10 回失敗したら再起動）。
void mqttLoop();
