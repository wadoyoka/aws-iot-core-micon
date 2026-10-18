# aws-iot-core-micon

Seeed Studio XIAO ESP32S3 Sense 用のカメラファームウェアです。
AWS IoT Core から MQTT で撮影コマンドを受け取り、JPEG を撮影して**そのまま MQTT で送ります**（デモ用に HTTP を使わない構成）。
画像は IoT ルールが S3 に保存します。

```
Lambda ──(QoS1)──▶ cameras/<thing>/capture ──▶ XIAO ESP32S3 Sense
                                                            │ 撮影 (VGA JPEG, ≤ 128 KB)
                                                            ▼
XIAO ──▶ cameras/<thing>/images/<captureId> ──▶ IoT ルール ──▶ S3 captures/<thing>/<captureId>.jpg
XIAO ──▶ cameras/<thing>/events             ──▶ IoT ルール ──▶ CloudWatch Logs
```

- ボード: Seeed Studio XIAO ESP32S3 Sense（カメラ拡張ボード装着、8MB PSRAM）
- 開発環境: PlatformIO（Arduino フレームワーク）
- 配線: 不要（カメラは拡張ボードのコネクタに接続済みのもの、電源は USB-C）

## 必要なもの

- [mise](https://mise.jdx.dev/)（または PlatformIO Core 6.1.x を直接インストール）
- 2.4GHz 帯の Wi-Fi（ESP32-S3 は 5GHz 非対応）
- AWS IoT Core にモノ（Thing）と IoT ポリシーが作成済みであること（以下の例ではどちらも `xiao-camera`）

```sh
mise install   # このディレクトリの .mise.toml に従って python / platformio を入れる
```

## セットアップ手順

### 1. デバイス証明書を作成する（AWS IoT コンソールで手動）

1. AWS IoT コンソール → **管理 → すべてのデバイス → モノ** → `xiao-camera` を開く
2. **証明書** タブ → **証明書を作成**
3. 次の 3 ファイルをダウンロードする（秘密鍵はこの画面でしか取得できない）
   - デバイス証明書 `xxxxxxxx-certificate.pem.crt`
   - 秘密鍵 `xxxxxxxx-private.pem.key`
   - ルート CA `AmazonRootCA1.pem`
4. 証明書を **有効化** する
5. 証明書にポリシー `xiao-camera` をアタッチする
6. 証明書をモノ `xiao-camera` にアタッチする

ポリシーで許可されているのは以下だけです。

| 操作 | 対象 |
| --- | --- |
| `iot:Connect` | クライアント ID = Thing 名 |
| `iot:Subscribe` / `iot:Receive` | `cameras/<thing>/capture` |
| `iot:Publish` | `cameras/<thing>/images/*`、`cameras/<thing>/events` |

### 2. `include/secrets.h` を作成する

```sh
cp include/secrets.example.h include/secrets.h
```

`include/secrets.h` は `.gitignore` 済みです。無いままビルドすると `#error` で止まります。

| 定数 | 値 |
| --- | --- |
| `WIFI_SSID` / `WIFI_PASSWORD` | Wi-Fi の SSID / パスワード |
| `AWS_IOT_ENDPOINT` | AWS IoT コンソール → **設定** のデバイスデータエンドポイント、または `aws iot describe-endpoint --endpoint-type iot:Data-ATS`（`xxxx-ats.iot.ap-northeast-1.amazonaws.com`） |
| `THING_NAME` | モノの名前（例 `xiao-camera`） |
| `TOPIC_PREFIX` | IoT ポリシー・IoT ルールで使うトピックの先頭（既定 `cameras`） |
| `AWS_ROOT_CA` | `AmazonRootCA1.pem` の中身 |
| `DEVICE_CERT` | `xxxxxxxx-certificate.pem.crt` の中身 |
| `DEVICE_PRIVATE_KEY` | `xxxxxxxx-private.pem.key` の中身 |

PEM は `R"EOF( ... )EOF"` の中に `-----BEGIN ...-----` から `-----END ...-----` までをそのまま貼り付けます。

### 3. 書き込み

XIAO を USB-C で接続して:

```sh
mise run flash     # 書き込み → シリアルモニタ (115200bps)
```

その他のタスク:

| タスク | 内容 |
| --- | --- |
| `mise run build` | ビルドのみ（`pio run`） |
| `mise run upload` | 書き込み（`pio run -t upload`） |
| `mise run monitor` | シリアルモニタ（`pio device monitor`） |
| `mise run clean` | ビルド成果物の削除 |

書き込みに失敗する場合は、BOOT ボタンを押しながら RESET を押して（または USB を挿し直して）ダウンロードモードにしてから再実行してください。

正常に起動するとシリアルに次のようなログが出ます。

```
=== aws-iot-core-micon (thing=xiao-camera) ===
Connecting to WiFi your-ssid....
WiFi connected: 192.168.x.x
Attempting MQTT connection...connected
Subscribed: cameras/xiao-camera/capture
```

## MQTT トピックとペイロード

### コマンド: `cameras/<thing>/capture`（Lambda → デバイス、QoS 1）

```json
{"captureId": "20261004T121314Z-1a2b3c4d", "requestedAt": "2026-10-04T12:13:14Z"}
```

受信コールバックの中で撮影から送信まで行います（デモ用に単純化）。処理中に届いた次のコマンドは、終わってから処理されます。

### 画像: `cameras/<thing>/images/<captureId>`（デバイス → IoT ルール → S3、QoS 0）

ペイロードは JPEG のバイナリそのもの（JSON ではない）です。IoT ルールが `SELECT *` で受けて、
S3 の `captures/<thing>/<captureId>.jpg` に保存します。

AWS IoT Core のメッセージは **128 KB が上限**です。

- 送る前に「トピック長 2 バイト＋トピック＋ JPEG」が 128 KB 以内か確認します（`MQTT_MAX_MESSAGE_BYTES`）。
- 超えたら、JPEG の画質を `12 → 20 → 30 → 45` の順に下げて撮り直します（`include/config.h` の `JPEG_QUALITY_STEPS`）。
- どの画質でも収まらなければ、送らずに `failed` を返します。

PubSubClient の内部バッファは最大 64 KB なので、画像はバッファを通さず
`beginPublish` / `write` / `endPublish` でストリームとして書きます。

### 結果: `cameras/<thing>/events`（デバイス → IoT ルール → CloudWatch Logs）

```json
{"captureId":"20261004T121314Z-1a2b3c4d","status":"published","bytes":45678}
{"captureId":"20261004T121314Z-1a2b3c4d","status":"failed","bytes":0,"error":"image exceeds MQTT message limit at every jpeg quality"}
```

`error` は失敗時のみ含まれます。

## 動作の詳細

- 撮影: VGA（640x480）。撮影前に画質を設定し、古いフレームを 1 枚捨ててから取得します（VGA なら通常 30〜60 KB）
- `captureId` はトピックの 1 階層になるので、英数字と `-` 以外を含むコマンドは撮影せず `failed` を返します
- Wi-Fi は起動時に接続し（30 秒つながらなければ再起動）、切れたら ESP32 が自動で再接続します。MQTT は切れていたら 5 秒おきに再接続し、10 回失敗したら再起動します
- 撮影〜送信中はオンボード LED（GPIO21、オレンジ）が点灯します

## トラブルシューティング

### MQTT の接続失敗（`failed, rc=...`）

| rc | 意味 | よくある原因 |
| --- | --- | --- |
| `-4` CONNECTION_TIMEOUT | 応答なし | エンドポイント誤り、ネットワーク遮断 |
| `-2` CONNECT_FAILED | TCP/TLS 接続失敗 | ルート CA / 証明書 / 秘密鍵の貼り間違い、証明書が無効化されている |
| `-3` CONNECTION_LOST | 接続後に切断 | **ポリシー違反**（クライアント ID が Thing 名と違う、許可外トピックへの購読/発行）。AWS IoT はポリシー違反時に CONNACK エラーではなく切断で返すことが多い |
| `5` UNAUTHORIZED | 認可されていない | 証明書にポリシーが付いていない / 証明書が非アクティブ |
| `4` BAD_CREDENTIALS | 認証失敗 | 証明書がアカウントに登録されていない |

- 接続直後に切断を繰り返す場合は、`THING_NAME` が AWS IoT のモノの名前（ポリシーで許可したクライアント ID）と一致しているか、同じクライアント ID で別のデバイス/テストクライアントが接続していないかを確認してください（同一 ID の接続は先の接続が切られます）。

### TLS と時刻同期

- 起動時に NTP で時刻合わせを始めますが、同期は待ちません（arduino-esp32 の TLS は既定で証明書の日付を検証しないため）。`rc=-2` が続き、ルート CA や証明書に問題が無い場合は、NTP 同期を待ってから接続するようにしてみてください。
- `X509 - Certificate verification failed` などが出る場合は、`AWS_ROOT_CA` に `AmazonRootCA1.pem` が正しく貼られているか確認してください。

### 送信の失敗（events の `error`）

| エラー | 原因と対処 |
| --- | --- |
| `image exceeds MQTT message limit at every jpeg quality` | 最低画質でも 128 KB を超えた。`camera.cpp` の `frame_size` を下げる（QVGA など） |
| `mqtt publish failed` | 送信中に MQTT が切れた。電波状況を確認。シリアルの `Image publish failed` を確認 |
| `invalid captureId: ...` | コマンドの `captureId` に使えない文字が入っている |
| `camera capture failed` | カメラの接続不良、PSRAM 設定の問題。リセットして `[camera]` のログを確認 |

画像が S3 に保存されない（`published` なのに Web で pending のまま）場合は、IoT ルールのエラーが
IoT ルールのエラー用ロググループ（名前が `/rule-errors` で終わるもの）に出ていないか確認してください。

### その他

- `[camera] esp_camera_init 失敗: 0x105` などはカメラ拡張ボードの接続を確認してください。
- PSRAM が「なし」と表示される場合は `platformio.ini` の `board_build.arduino.memory_type = qio_opi` を確認してください。
