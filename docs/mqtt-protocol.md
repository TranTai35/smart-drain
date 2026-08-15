# Smart Drain — Giao thức MQTT

Tài liệu này quy định **toàn bộ topic và định dạng payload** trao đổi giữa ESP32
và website. Hai bên code độc lập theo file này, không cần bàn thêm.

- Người soạn: MSSV 24127476 (theo phân công mục 7 — "thiết kế topic và định dạng payload")
- Người thực hiện phía ESP32: MSSV 24127410
- Phiên bản: 1.0

> **Quy tắc:** nếu cần đổi bất cứ điều gì trong file này, sửa file rồi báo cả hai bên.
> Không tự đổi trong code riêng

---

## 1. Broker

| Mục | Giá trị |
|---|---|
| Địa chỉ | `broker.hivemq.com` (dự phòng: `test.mosquitto.org`) |
| Port | `1883` (TCP, không TLS) |
| Tài khoản | không cần |
| Client ID ESP32 | `smartdrain-esp32` |
| Client ID Website | `smartdrain-web` |
| Keep-alive | `30` giây |

**Client ID phải khác nhau.** Hai client trùng ID sẽ liên tục đá nhau ra khỏi
broker, biểu hiện là cứ vài giây lại mất kết nối một lần.

### Tiền tố topic

```
BASE = "smartdrain"
```

Cả hai bên khai báo `BASE` thành **một hằng số duy nhất** trong code, mọi topic
đều ghép từ nó:

```cpp
// ESP32 — Config.h
#define MQTT_BASE "smartdrain"
#define TOPIC_TANK_INPUT MQTT_BASE "/tank/input"
```

```python
# Website
BASE = "smartdrain"
TOPIC_TANK_INPUT = f"{BASE}/tank/input"
```

Lý do: HiveMQ và Mosquitto là broker **công cộng**, nhóm khác cũng publish được
vào `smartdrain/...`. Nếu hôm demo bị nhiễu dữ liệu lạ, chỉ cần đổi `BASE` thành
`smartdrain-24127476` ở một chỗ trong mỗi bên là xong trong 1 phút.

---

## 2. Quy ước chung

**Chiều truyền.** Topic dưới `command/` là **web → ESP32**, mọi topic còn lại là
**ESP32 → web**. Web không bao giờ publish vào topic trạng thái, ESP32 không bao
giờ publish vào topic lệnh.

**Định dạng.**
- Topic trạng thái: **JSON**, một object, không lồng nhau.
- Topic lệnh: **chuỗi thuần, viết hoa** (`ON`, `AUTO`...). Cố ý làm vậy để ESP32
  không phải cài thư viện JSON để đọc lệnh.

**Retain.** Mọi topic trạng thái publish với `retain = true`. Broker giữ lại bản
tin cuối cùng và gửi ngay cho client mới kết nối. Không có retain thì mở
dashboard lên sẽ trống trơn cho tới lần publish kế tiếp — nhìn như hệ thống bị
lỗi. Topic lệnh **luôn** `retain = false`, nếu không ESP32 sẽ nhận lại lệnh cũ
mỗi lần khởi động lại và tự bật bơm.

**QoS.** Khi **subscribe** thì đăng ký QoS `1` cho mọi topic.

Khi **publish** thì thực tế khác với dự kiến ban đầu: thư viện PubSubClient trên
ESP32 **không có tham số QoS cho `publish()`**, mọi bản tin gửi đi đều là QoS 0.
Không thể khắc phục bằng cách viết thêm mà phải đổi sang thư viện khác, không đáng
với thời gian còn lại. Trên thực tế điều này không gây vấn đề, vì:

- Mọi topic trạng thái đều có `retain = true`, nên bản tin cuối cùng luôn được
  broker giữ lại; mất một gói thì gói kế tiếp (2 giây sau) sẽ bù.
- Website publish lệnh bằng thư viện paho, thư viện này **có** hỗ trợ QoS 1, nên
  chiều quan trọng hơn — lệnh điều khiển đi xuống ESP32 — vẫn được đảm bảo.

**Nhịp gửi.** ESP32 publish mức nước **mỗi 2 giây**. Các topic còn lại publish
**ngay khi giá trị thay đổi**, cộng thêm một lần mỗi 10 giây để làm nhịp tim.

**Thời gian.** ESP32 **không** gửi ngày giờ. ESP32 không có đồng hồ thực, muốn có
phải đồng bộ NTP, thêm việc và thêm chỗ hỏng. ESP32 chỉ gửi `uptime` (số giây kể
từ lúc khởi động); website lấy giờ máy chủ làm mốc thời gian khi ghi Firebase.
`uptime` còn có tác dụng phụ hữu ích: nếu nó đột ngột về gần 0 nghĩa là ESP32 vừa
reset.

**Số liệu.** `percent` là **số nguyên 0–100**. Không gửi số thực, không gửi kèm
ký tự `%`.

---

## 3. Bảng topic đầy đủ

### ESP32 → Website

| Topic | Nội dung | Retain | QoS |
|---|---|---|---|
| `smartdrain/tank/input` | Mức nước bể thu | ✅ | 0 |
| `smartdrain/tank/output` | Mức nước bể xả | ✅ | 0 |
| `smartdrain/pump/state` | Trạng thái máy bơm | ✅ | 1 |
| `smartdrain/mode` | Chế độ Auto/Manual | ✅ | 1 |
| `smartdrain/alert` | Cảnh báo hiện tại | ✅ | 1 |
| `smartdrain/status` | Online/Offline **(mới)** | ✅ | 1 |
| `smartdrain/config` | Ngưỡng đang áp dụng **(mới)** | ✅ | 1 |

### Website → ESP32

| Topic | Nội dung | Retain | QoS |
|---|---|---|---|
| `smartdrain/command/pump` | Bật/tắt bơm | ❌ | 1 |
| `smartdrain/command/mode` | Đổi chế độ | ❌ | 1 |
| `smartdrain/command/buzzer` | Tắt/bật tiếng còi **(mới)** | ❌ | 1 |
| `smartdrain/command/config` | Đổi ngưỡng **(mới)** | ❌ | 1 |

Bốn topic đánh dấu **(mới)** chưa có trong mục 4.7 của báo cáo. Chúng cần thiết
vì mục 3.2 đã hứa các chức năng "Hiển thị trạng thái ESP32", "Tắt buzzer từ web",
"Cài đặt ngưỡng" và "Cài đặt thời gian tối đa" — không có đường truyền thì không
làm được. Bảy topic cũ giữ nguyên tên, chỉ bổ sung thêm nên báo cáo chỉ cần thêm
dòng, không phải sửa dòng nào.

---

## 4. Chi tiết từng topic

### 4.1 `smartdrain/tank/input` và `smartdrain/tank/output`

```json
{"percent": 72, "adc": 2126, "level": "HIGH", "uptime": 348}
```

| Trường | Kiểu | Ghi chú |
|---|---|---|
| `percent` | int 0–100 | Mức nước tương đối |
| `adc` | int 0–4095 | Giá trị ADC thô, dùng để đối chiếu khi hiệu chuẩn |
| `level` | string | `LOW` / `MEDIUM` / `HIGH` / `DANGER` |
| `uptime` | int | Số giây từ lúc ESP32 khởi động |

`level` tính theo đúng bảng mục 4.2 của báo cáo, và trùng với hàm
`getWaterLevelName()` đã có sẵn trong `WaterSensor.cpp`:

| percent | level |
|---|---|
| 0–29 | `LOW` |
| 30–69 | `MEDIUM` |
| 70–89 | `HIGH` |
| 90–100 | `DANGER` |

ESP32 gửi luôn `level` thay vì để web tự tính, để LCD và website không bao giờ
hiển thị lệch nhau — chỉ có một chỗ quyết định ngưỡng.

### 4.2 `smartdrain/pump/state`

```json
{"state": "ON", "source": "AUTO", "runtime": 15, "uptime": 348}
```

| Trường | Kiểu | Ghi chú |
|---|---|---|
| `state` | `ON` / `OFF` | Trạng thái relay thực tế |
| `source` | string | Nguyên nhân của lần chuyển trạng thái gần nhất |
| `runtime` | int | Số giây bơm đã chạy liên tục; bằng `0` khi bơm tắt |

Giá trị `source`:

| Giá trị | Ý nghĩa |
|---|---|
| `AUTO` | Logic Auto bật/tắt |
| `MANUAL` | Người dùng bấm nút trên board hoặc bấm trên web |
| `SAFETY` | Hệ thống tự tắt để bảo vệ (bể xả đầy, quá giờ, thoát nước bất thường) |
| `BOOT` | Trạng thái mặc định khi vừa khởi động |

**Bắt buộc:** ESP32 publish topic này **sau mỗi lệnh nhận được, kể cả khi lệnh bị
từ chối**. Ví dụ web gửi `ON` lúc bể xả đang 85% — theo mục 4.3 lệnh bị từ chối,
ESP32 vẫn phải publish `{"state":"OFF","source":"SAFETY",...}`. Website lấy topic
này làm nguồn sự thật duy nhất cho nút bật/tắt, không tự đổi giao diện khi người
dùng bấm. Nhờ vậy màn hình luôn phản ánh đúng thực tế phần cứng.

### 4.3 `smartdrain/mode`

```json
{"mode": "AUTO", "uptime": 348}
```

`mode` là `AUTO` hoặc `MANUAL`.

### 4.4 `smartdrain/alert`

```json
{"code": "OUTPUT_TANK_FULL", "severity": "DANGER",
 "message": "Be xa da day, da dung bom", "active": true,
 "muted": false, "uptime": 348}
```

| Trường | Kiểu | Ghi chú |
|---|---|---|
| `code` | string | Mã cảnh báo, xem bảng mục 5 |
| `severity` | `INFO` / `WARNING` / `DANGER` | Website dùng để chọn màu |
| `message` | string | Mô tả ngắn, **không dấu tiếng Việt** |
| `active` | bool | `false` nghĩa là đã hết cảnh báo |
| `muted` | bool | Còi đang bị tắt tiếng hay không |

Tại một thời điểm chỉ có **một** cảnh báo trên topic này. Nếu nhiều điều kiện
cùng xảy ra, gửi cảnh báo có `severity` cao nhất; cùng mức thì gửi cái xảy ra
trước. Khi hết cảnh báo, publish `{"code":"NONE","severity":"INFO","active":false,...}`
chứ **đừng** publish chuỗi rỗng — web cần biết rõ là "đã hết", khác với "chưa
từng có tin nào".

`message` viết **không dấu**. LCD 1602 không hiển thị được tiếng Việt có dấu, và
dùng chung một chuỗi cho cả LCD lẫn web thì đỡ phải quản lý hai bản.

### 4.5 `smartdrain/status` *(mới)*

Payload là chuỗi thuần: `ONLINE` hoặc `OFFLINE`.

Đây là topic **Last Will and Testament (LWT)**, xem mục 6. Đây là cách duy nhất
để web biết ESP32 đã rớt mạng.

### 4.6 `smartdrain/config` *(mới)*

```json
{"pump_start": 70, "pump_stop": 30, "output_limit": 80,
 "max_runtime": 120, "drain_check": 30, "drain_min_drop": 5}
```

| Trường | Đơn vị | Mặc định | Ý nghĩa |
|---|---|---|---|
| `pump_start` | % | 70 | Mức bể thu để Auto bật bơm |
| `pump_stop` | % | 30 | Mức bể thu để Auto tắt bơm |
| `output_limit` | % | 80 | Mức bể xả cấm bơm |
| `max_runtime` | giây | 120 | Thời gian bơm chạy liên tục tối đa |
| `drain_check` | giây | 30 | Sau bao lâu thì kiểm tra hiệu quả thoát nước |
| `drain_min_drop` | % | 5 | Mức giảm tối thiểu trong khoảng đó |

ESP32 publish topic này lúc khởi động và sau mỗi lần nhận lệnh đổi ngưỡng, để web
hiển thị đúng giá trị đang thực sự áp dụng.

> `max_runtime`, `drain_check` và `drain_min_drop` là giá trị tạm. Chốt lại sau
> khi 410 đo lưu lượng thật của bơm MB370 trên mô hình.

### 4.7 `smartdrain/command/pump`

Payload: `ON` hoặc `OFF`.

ESP32 xử lý theo mục 4.3 của báo cáo:
- Đang ở chế độ `AUTO` → **bỏ qua** lệnh (không đổi trạng thái), publish lại
  `pump/state` như cũ.
- Đang `MANUAL` và nhận `ON` nhưng bể xả `>= output_limit` → **từ chối**, publish
  `pump/state` với `state: OFF`, `source: SAFETY`, và publish cảnh báo
  `OUTPUT_TANK_FULL`.
- `OFF` **luôn được chấp nhận** trong mọi chế độ. Lệnh dừng không bao giờ bị từ
  chối — đây là nút an toàn cuối cùng.

### 4.8 `smartdrain/command/mode`

Payload: `AUTO` hoặc `MANUAL`.

Khi chuyển sang `AUTO`, ESP32 áp dụng lại logic ngưỡng ngay lần lặp kế tiếp,
không giữ trạng thái bơm cũ.

### 4.9 `smartdrain/command/buzzer` *(mới)*

Payload: `MUTE` hoặc `UNMUTE`.

`MUTE` chỉ tắt tiếng còi. Cảnh báo vẫn còn hiệu lực, LED vẫn sáng, `alert` vẫn
`active: true` — đúng như mục 4.5 của báo cáo. ESP32 publish lại `alert` với
`muted` đã cập nhật. Khi phát sinh **cảnh báo mới khác mã**, `muted` tự về `false`
để còi kêu lại.

### 4.10 `smartdrain/command/config` *(mới)*

Payload: **một** cặp `KEY=VALUE` mỗi bản tin.

```
pump_start=75
max_runtime=90
```

Muốn đổi nhiều ngưỡng thì gửi nhiều bản tin liên tiếp. Cố ý thiết kế như vậy để
ESP32 đọc bằng `sscanf` một dòng, không cần thư viện JSON.

`KEY` dùng đúng tên trường ở mục 4.6. ESP32 kiểm tra giá trị hợp lệ trước khi
nhận, gặp giá trị vô lý thì bỏ qua và giữ nguyên ngưỡng cũ:

- `0 <= pump_stop < pump_start <= 100`
- `0 < output_limit <= 100`
- `10 <= max_runtime <= 600`

Sau khi áp dụng (hoặc từ chối), publish lại `smartdrain/config`. Web đọc topic đó
để biết ngưỡng nào thực sự có hiệu lực.

---

## 5. Bảng mã cảnh báo

| `code` | `severity` | Điều kiện phát sinh | `message` gợi ý |
|---|---|---|---|
| `NONE` | `INFO` | Không có cảnh báo | `He thong binh thuong` |
| `INPUT_TANK_DANGER` | `DANGER` | Bể thu `>= 90%` | `Be thu o muc nguy hiem` |
| `OUTPUT_TANK_FULL` | `DANGER` | Bể xả `>= output_limit` | `Be xa da day, da dung bom` |
| `PUMP_TIMEOUT` | `WARNING` | Bơm chạy quá `max_runtime` | `Bom chay qua thoi gian cho phep` |
| `DRAIN_ABNORMAL` | `WARNING` | Sau `drain_check` giây bơm chạy mà bể thu giảm chưa tới `drain_min_drop` | `Thoat nuoc bat thuong, kiem tra ong` |
| `SENSOR_ERROR` | `WARNING` | ADC nằm ngoài dải hợp lệ | `Cam bien khong phan hoi` |

> `SENSOR_ERROR` **chưa được firmware cài đặt** — đây là mã dự phòng. Website đã
> xử lý sẵn nên nếu sau này ESP32 phát mã này thì giao diện hiển thị được ngay,
> không cần sửa gì bên web.

### Cảnh báo tự tắt và cảnh báo phải chốt

`INPUT_TANK_DANGER`, `OUTPUT_TANK_FULL`, `SENSOR_ERROR` **tự tắt** khi điều kiện
không còn.

`PUMP_TIMEOUT` và `DRAIN_ABNORMAL` phải **chốt lại (latch)**: bơm dừng và cảnh
báo giữ nguyên cho tới khi người dùng can thiệp. Nếu tự tắt, hệ thống sẽ bật bơm
lại ngay và lặp vô hạn vòng "chạy → lỗi → chạy". Cảnh báo chốt được xoá khi nhận
`command/pump = OFF` hoặc `command/mode` (bất kể giá trị nào).

Riêng chức năng **"Xác nhận cảnh báo"** ở mục 3.2 xử lý hoàn toàn phía website —
người quản lý đánh dấu đã xử lý, Flask ghi cờ vào Firebase. Việc này không đụng
tới ESP32 nên không cần thêm topic.

---

## 6. Last Will and Testament

Mục 3.2 yêu cầu website hiển thị ESP32 `Online`/`Offline`. LWT là cơ chế của
chính broker: ESP32 khai báo trước một "di chúc" lúc kết nối, và nếu ESP32 mất
kết nối đột ngột (rớt Wi-Fi, rút điện, treo), broker **tự** publish bản tin đó
thay cho ESP32.

Cấu hình LWT phía ESP32:

| Tham số | Giá trị |
|---|---|
| Will topic | `smartdrain/status` |
| Will payload | `OFFLINE` |
| Will QoS | `1` |
| Will retain | `true` |

Ngay sau khi kết nối thành công, ESP32 publish `ONLINE` (retain, QoS 1) vào cùng
topic để ghi đè di chúc.

Không có LWT thì web chỉ đoán được bằng cách đếm thời gian im lặng — chậm, thiếu
chính xác, và hôm demo rút dây ESP32 ra thì màn hình vẫn báo Online thêm cả chục
giây.

---

## 7. Code mẫu cho ESP32

Dùng thư viện **PubSubClient**.

```cpp
#include <WiFi.h>
#include <PubSubClient.h>
#include "Config.h"

WiFiClient wifiClient;
PubSubClient mqtt(wifiClient);

void connectMQTT()
{
    while (!mqtt.connected())
    {
        // connect(clientId, user, pass, willTopic, willQos, willRetain, willMessage)
        bool ok = mqtt.connect("smartdrain-esp32", NULL, NULL,
                               MQTT_BASE "/status", 1, true, "OFFLINE");
        if (ok)
        {
            mqtt.publish(MQTT_BASE "/status", "ONLINE", true);

            mqtt.subscribe(MQTT_BASE "/command/pump", 1);
            mqtt.subscribe(MQTT_BASE "/command/mode", 1);
            mqtt.subscribe(MQTT_BASE "/command/buzzer", 1);
            mqtt.subscribe(MQTT_BASE "/command/config", 1);

            publishConfig();
            publishPumpState();
            publishMode();
        }
        else
        {
            delay(2000);
        }
    }
}
```

Publish trạng thái — dùng `snprintf`, không cần thư viện JSON:

```cpp
void publishTank(const char* topic, int percent, int adc)
{
    char payload[128];
    snprintf(payload, sizeof(payload),
             "{\"percent\":%d,\"adc\":%d,\"level\":\"%s\",\"uptime\":%lu}",
             percent, adc, getWaterLevelName(percent), millis() / 1000);

    mqtt.publish(topic, payload, true);   // retain = true
}
```

Nhận lệnh — `payload` **không có ký tự kết thúc chuỗi**, phải tự thêm:

```cpp
void onMqttMessage(char* topic, byte* payload, unsigned int length)
{
    char msg[64];
    if (length >= sizeof(msg)) length = sizeof(msg) - 1;
    memcpy(msg, payload, length);
    msg[length] = '\0';

    if (strcmp(topic, MQTT_BASE "/command/pump") == 0)
    {
        handlePumpCommand(msg);          // luôn publishPumpState() ở cuối
    }
    else if (strcmp(topic, MQTT_BASE "/command/config") == 0)
    {
        char key[24];
        int value;
        if (sscanf(msg, "%23[^=]=%d", key, &value) == 2)
        {
            applyConfig(key, value);     // luôn publishConfig() ở cuối
        }
    }
}
```

Hai điểm dễ vấp:

**Buffer mặc định của PubSubClient là 256 byte**, tính cả topic lẫn payload. Payload
của mình ngắn nên vừa, nhưng cứ nới rộng cho chắc, gọi ngay sau `setServer`:

```cpp
mqtt.setBufferSize(512);
```

Publish quá buffer sẽ **thất bại im lặng** — hàm trả về `false` và không có báo
lỗi nào. Rất khó tìm ra nếu không biết trước.

**Không được `delay()` lâu trong `loop()`.** `mqtt.loop()` phải được gọi liên
tục, nếu không client sẽ bị broker ngắt vì quá keep-alive. File
`SmartDrain_ESP32.ino` hiện có `delay(500)` cuối `loop()`, và `readAverageADC()`
có `delay(2)` × 10 lần × 2 cảm biến — cộng lại hơn nửa giây mỗi vòng. Nên chuyển
sang mốc thời gian bằng `millis()` thay vì `delay()` khi ghép MQTT vào.

---

## 8. Checklist cho MSSV 24127410

- [ ] Khai báo `MQTT_BASE` và các topic trong `Config.h`
- [ ] Kết nối MQTT **kèm LWT** vào `smartdrain/status`, publish `ONLINE` sau khi connect
- [ ] `setBufferSize(512)`
- [ ] Publish `tank/input` + `tank/output` mỗi 2 giây, retain
- [ ] Publish `pump/state`, `mode`, `alert`, `config` khi đổi giá trị, retain
- [ ] Subscribe đủ **4** topic `command/*`
- [ ] Publish lại `pump/state` sau **mọi** lệnh, kể cả lệnh bị từ chối
- [ ] `command/pump = OFF` không bao giờ bị từ chối
- [ ] Chốt `PUMP_TIMEOUT` và `DRAIN_ABNORMAL`, xoá khi có lệnh pump OFF / đổi mode
- [ ] `MUTE` chỉ tắt còi, không xoá cảnh báo
- [ ] Bỏ `delay()` trong `loop()`, chuyển sang `millis()`

## 9. Kiểm tra nhanh không cần phần cứng

Website có sẵn bộ giả lập ESP32 tại `web/tools/fake_esp32.py`, chạy đúng giao
thức trong file này. **410 có thể dùng nó làm bản tham chiếu** — nó cài đặt đầy
đủ logic Auto/Manual, chống tràn, quá giờ và thoát nước bất thường, tức là đúng
những gì firmware cần làm.

Xem trực tiếp mọi bản tin đang chạy trên broker:

```bash
mosquitto_sub -h broker.hivemq.com -t "smartdrain/#" -v
```

Gửi thử một lệnh bằng tay:

```bash
mosquitto_pub -h broker.hivemq.com -t "smartdrain/command/pump" -m "ON"
```
