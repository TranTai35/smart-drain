# Smart Drain — Ghi chú sửa lỗi và cấu hình

File này gồm hai phần:

- **Phần 1** — những chỗ còn phải sửa, kèm lý do và cách sửa
- **Phần 2** — những thứ **bắt buộc** phải điền thì app mới chạy được (Wi-Fi,
  MQTT, Firebase, Gmail)

Cập nhật lần cuối: 15/08/2026 · Người soạn: MSSV 24127476

---

# PHẦN 1 — Những chỗ cần sửa

## Đã sửa xong

| # | Chỗ sửa | Nội dung |
|---|---|---|
| 1 | `AlertSystem.*`, `MqttManager.cpp` | **Sửa thứ tự ưu tiên cảnh báo** — lỗi logic khiến `DRAIN_ABNORMAL` gần như không bao giờ hiện được |
| 2 | `Display.cpp` | Viết lại LCD thành các trang luân phiên: mức nước, chế độ + bơm, cảnh báo |
| 3 | `MqttManager.cpp` | Phát hiện thay đổi cảnh báo bằng **mã** thay vì bằng mức |
| 4 | `Config.h` | Thêm `INPUT_DANGER_LEVEL 90`, thay 4 chỗ viết cứng số 90 |
| 5 | `web/static/js/chart.js` | **Biểu đồ phình to vô tận** làm trang dài mãi rồi hỏng hẳn |
| 6 | `docs/mqtt-protocol.md` | Sửa mô tả QoS cho khớp PubSubClient; ghi rõ `SENSOR_ERROR` chưa cài đặt |
| 7 | `web/static/css/style.css` | Thêm màu cho nhãn `ALERT_CLEAR` |

---

### 1. Thứ tự ưu tiên cảnh báo — lỗi quan trọng nhất

**Mức độ: quan trọng.** Đây là lỗi logic chứ không phải lỗi cú pháp, nên chương
trình vẫn chạy bình thường và rất khó phát hiện khi thử bằng tay.

**Trước đây**, cả `getMqttAlertCode()` trong `MqttManager.cpp` lẫn
`determineAlertLevel()` trong `AlertSystem.cpp` đều xếp theo thứ tự:

```
1. OUTPUT_TANK_FULL
2. INPUT_TANK_DANGER      <-- bể thu >= 90%
3. PUMP_TIMEOUT
4. DRAIN_ABNORMAL
```

**Vấn đề:** cảnh báo thoát nước bất thường sinh ra để phát hiện ống bị nghẹt.
Nhưng khi ống nghẹt thì nước không thoát được, nên bể thu tiếp tục dâng và vượt
90%. Lúc đó `INPUT_TANK_DANGER` đứng trên sẽ **đè mất** `DRAIN_ABNORMAL`.

Kết quả: website hiển thị *"Bể thu ở mức nguy hiểm"* thay vì *"Thoát nước bất
thường, kiểm tra ống"*. Người dùng thấy nước cao nhưng không biết nguyên nhân là
ống nghẹt. Đúng tình huống mà chức năng này sinh ra để xử lý thì nó lại im lặng.

`PUMP_TIMEOUT` bị che tương tự: bơm chạy quá giờ thường xảy ra đúng lúc nước
đang nhiều.

**Thứ tự sau khi sửa** (khớp bảng mục 5 của `docs/mqtt-protocol.md`):

```
1. OUTPUT_TANK_FULL       <-- nguy hiểm nhất: nước sắp tràn ra ngoài
2. PUMP_TIMEOUT           <-- lỗi chốt
3. DRAIN_ABNORMAL         <-- lỗi chốt
4. INPUT_TANK_DANGER
```

**Cách sửa đã áp dụng.** Gốc rễ của lỗi không phải thứ tự sai, mà là **thứ tự bị
viết ở hai nơi**. Sửa một chỗ mà quên chỗ kia thì LCD và website sẽ báo khác nhau.

Vì vậy toàn bộ quyết định về cảnh báo được gom về `AlertSystem` — nay là nguồn sự
thật duy nhất, có bốn hàm mới:

```cpp
const char* getAlertCode();        // "DRAIN_ABNORMAL"
const char* getAlertSeverity();    // "WARNING"
const char* getAlertMessage();     // mô tả đầy đủ, cho MQTT
const char* getAlertShortText();   // bản 16 ký tự, cho LCD
bool        isAlertActive();
```

Cả bốn đều suy ra từ cùng một biến `currentInternalAlert`, nên **không thể xảy ra
mâu thuẫn** giữa mã, mức độ và mô tả.

`MqttManager.cpp` đã bỏ ba hàm `getMqttAlertCode/Severity/Message()` và gọi thẳng
các hàm trên. Trước đây ba hàm đó tự xét lại điều kiện một lần nữa — chính là chỗ
dễ sinh mâu thuẫn.

> **Cái bẫy đã tránh được:** `getMqttAlertSeverity()` cũ xét điều kiện độc lập với
> `getMqttAlertCode()`. Nếu chỉ đổi thứ tự ở một hàm thì sẽ có lúc gửi đi
> `code = "PUMP_TIMEOUT"` kèm `severity = "DANGER"`, trong khi bảng mục 5 quy định
> `PUMP_TIMEOUT` là `WARNING`. Hai trường trong cùng một bản tin mâu thuẫn nhau.

**Một điểm cố ý giữ khác biệt:** `PUMP_TIMEOUT` và `DRAIN_ABNORMAL` gửi lên
website là `WARNING`, nhưng LED và buzzer trên board vẫn báo ở mức `ALERT_DANGER`.
Mục 3.1 báo cáo yêu cầu buzzer kêu khi *"có lỗi vận hành"*, và người đứng cạnh mô
hình cần biết ngay. `severity` là cách phân loại cho website, `AlertLevel` là mức
độ báo động tại chỗ — hai thứ khác nhau.

**Cách kiểm tra trên mô hình thật:** đổ nước cho bể thu trên 90%, gập ống hút lại
hoặc bịt đầu ống, bật bơm ở chế độ Manual. Sau `drain_check` giây (mặc định 30):

- LCD phải hiện `! CANH BAO` / `Ong bi nghet?`
- Website phải hiện *"Thoát nước bất thường, kiểm tra ống"*
- **Không** được hiện *"Bể thu ở mức nguy hiểm"*

### 2. LCD hiển thị luân phiên

`Display.cpp` trước đây chỉ hiện mức nước hai bể và tên cấp độ. Mục 3.1 báo cáo
yêu cầu hiển thị thêm **chế độ, trạng thái máy bơm và cảnh báo**.

Màn 16×2 chỉ có 32 ký tự và đã dùng hết, không nhét thêm được, nên các trang được
hiển thị luân phiên:

```
Trang 1 (5 giây)      Trang 2 (2 giây)      Trang 3 (2,5 giây)
IN:72%  OUT:45%       Mode: AUTO            ! CANH BAO
HIGH    MEDIUM        Pump: ON 15s          Ong bi nghet?
```

**Trang mức nước hiển thị lâu hơn hẳn** (5 giây so với 2 giây) vì đây là thông
tin cần theo dõi liên tục, còn chế độ và cảnh báo chỉ cần liếc qua là nắm được.
Ba con số nằm ở đầu `Display.cpp`, muốn chỉnh thì sửa trực tiếp:

```cpp
const unsigned long PAGE_WATER_MS  = 5000UL;
const unsigned long PAGE_STATUS_MS = 2000UL;
const unsigned long PAGE_ALERT_MS  = 2500UL;
```

Hai chi tiết đáng chú ý:

- **Trang cảnh báo chỉ xuất hiện khi thực sự có cảnh báo.** Lúc bình thường màn
  hình chỉ xoay giữa hai trang đầu, không hiện dòng "Binh thuong" vô nghĩa.
- **Cảnh báo mới làm màn hình nhảy sang ngay**, không bắt người xem đợi hết 5
  giây của trang đang hiển thị.

Đổi trang bằng `millis()`, **không dùng `delay()`** — `delay()` sẽ chặn
`mqtt.loop()` và làm ESP32 bị broker ngắt kết nối vì quá keep-alive.

Mọi dòng đều được đệm khoảng trắng cho đủ đúng 16 ký tự trước khi in. LCD không
tự xóa ký tự cũ, in chuỗi ngắn đè lên chuỗi dài sẽ để lại phần đuôi của chuỗi
trước — ví dụ `Pump: ON 15s` đổi thành `Pump: OFF` sẽ thành `Pump: OFF15s`.

### 3. Phát hiện thay đổi cảnh báo bằng mã thay vì bằng mức

`updateStatePublishing()` trong `MqttManager.cpp` trước đây so sánh
`currentAlertLevel` để biết trạng thái có đổi hay không. Nhưng **cả bốn cảnh báo
đều cho `ALERT_DANGER`**, nên khi chuyển từ cảnh báo này sang cảnh báo khác — ví
dụ `OUTPUT_TANK_FULL` → `INPUT_TANK_DANGER` — hệ thống không nhận ra là đã đổi.

Website phải chờ tới nhịp tim 10 giây sau mới thấy cảnh báo mới. Nay so sánh bằng
`getAlertCode()` nên phát hiện ngay.

### 4. Biểu đồ phình to vô tận

**Hiện tượng:** mở trang Tổng quan để một lúc thì khung biểu đồ cao dần lên, trang
web dài mãi ra. Khoảng 20–30 giây sau biểu đồ biến thành ô trắng có biểu tượng mặt
buồn.

**Nguyên nhân:** trong JavaScript, `canvas.height` **chính là** thuộc tính `height`
của thẻ `<canvas>` chứ không phải một biến riêng. Code cũ đọc chiều cao từ thuộc
tính đó rồi nhân với `devicePixelRatio` và ghi ngược lại:

```js
var cssHeight = parseInt(canvas.getAttribute("height"), 10) || 220;
canvas.height = cssHeight * ratio;   // <-- ghi de luon thuoc tinh vua doc
```

Lần vẽ sau lại đọc đúng giá trị vừa ghi, nên chiều cao **tăng theo cấp số nhân**.
Với màn hình đặt tỷ lệ 125% (rất phổ biến trên Windows):

```
220 → 275 → 344 → 430 → 537 → ... → 1.642.397
```

Dashboard vẽ lại mỗi giây, nên chỉ sau **23 giây** canvas đã vượt giới hạn kích
thước của trình duyệt. Lúc đó trình duyệt không vẽ nổi nữa và hiện ô trắng kèm
biểu tượng ảnh hỏng — đúng hiện tượng quan sát được.

Máy đặt tỷ lệ 100% (`devicePixelRatio = 1`) sẽ **không** gặp lỗi này, vì nhân với
1 thì giá trị không đổi. Đó là lý do lỗi không phải lúc nào cũng xuất hiện.

**Cách sửa:** đọc chiều cao gốc đúng một lần rồi cất vào `canvas.dataset.baseHeight`
— chỗ mà hàm vẽ không bao giờ ghi đè. Đồng thời chặn `devicePixelRatio` tối đa ở 2,
vì màn hình khai báo tỷ lệ 3–4 sẽ tạo canvas rất lớn mà mắt thường không thấy đẹp hơn.

Cùng một hàm vẽ dùng cho cả trang Tổng quan lẫn trang Lịch sử, nên sửa một chỗ là
hết cả hai.

### 5. Gom số 90 viết cứng về một hằng số

Thêm `INPUT_DANGER_LEVEL 90` vào `Config.h`, thay 4 chỗ viết cứng rải rác.
**Không đổi hành vi** — vẫn là 90%, chỉ để sau này chỉnh một dòng thay vì bốn.

> `getWaterLevelName()` trong `WaterSensor.cpp` cũng có số 90 nhưng **cố ý không
> đổi**. Đó là mốc đặt tên cấp độ (HIGH → DANGER) dùng cho **cả hai bể**, khác với
> ngưỡng cảnh báo chỉ áp cho bể thu. Gộp chung sẽ khiến chỉnh ngưỡng cảnh báo lại
> vô tình đổi nhãn hiển thị của bể xả.

---

## Cần kiểm tra lại trên phần cứng thật

Toàn bộ thay đổi ở trên **chưa được nạp lên ESP32 thật**, mới chỉ kiểm thử logic
bằng cách biên dịch riêng phần thuật toán trên máy tính. Cần nạp lại và thử:

- [ ] LCD xoay trang đúng, chữ không bị sót đuôi của trang trước
- [ ] Thời gian trang mức nước đúng là dài hơn trang chế độ
- [ ] Gập ống → sau 30 giây LCD hiện `Ong bi nghet?`, web hiện "Thoát nước bất thường"
- [ ] Bể xả đầy → cảnh báo bể xả được ưu tiên hơn mọi cảnh báo khác
- [ ] Buzzer vẫn kêu khi có lỗi bơm, và tắt được bằng nút "Tắt còi" trên web

---

## Cần sửa — MSSV 24127410

### 1. `SENSOR_ERROR` chưa được cài đặt — không bắt buộc

`docs/mqtt-protocol.md` có mã này nhưng firmware chưa bao giờ phát. Website đã xử
lý sẵn nên nếu thêm thì không cần sửa gì bên web.

Nếu còn thời gian, cách phát hiện đơn giản: nếu giá trị ADC nằm ngoài dải hợp lệ
(ví dụ dưới 50 hoặc trên 4000) liên tục vài giây thì coi như cảm biến bị tuột dây
hoặc chập.

Không làm cũng không ảnh hưởng gì đến báo cáo, vì mục 3.1 không hứa chức năng này.

---

## Cần sửa — MSSV 24127119

### 1. LCD — đã viết xong, cần xem lại và thử trên mô hình

Phần LCD trong mục 7 giao cho 119 nhưng đã được viết sẵn (xem mục 2 phần "Đã sửa
xong" ở trên). Việc còn lại là **nạp lên board và xem có ưng không**, đặc biệt là
thời gian mỗi trang — ba con số nằm ngay đầu `Display.cpp`, sửa rất nhanh.

Nếu thấy trang mức nước còn hiện quá ngắn hoặc quá dài, chỉnh `PAGE_WATER_MS`.

### 2. Các mục cần sửa trong báo cáo

Xem chi tiết ở `docs/can-sua-119-410.md` phần B. Tóm tắt:

- Mục 4.1: đang mô tả hiệu chuẩn 2 điểm (`ADC_DRY`/`ADC_FULL`), thực tế code dùng
  **5 điểm** nội suy từng đoạn. Cách làm thật tốt hơn mô tả trong báo cáo.
- Mục 4.7: thiếu 4 topic (`status`, `config`, `command/buzzer`, `command/config`)
- Mục 3.2 / 4.9 / 7 / 9: đang nói ba kiểu về email và Telegram, phải thống nhất
- Mục 4.6: nên thêm một câu giải thích vì sao cảnh báo phải chốt lại

---

# PHẦN 2 — Những chỗ phải điền để chạy được

Bốn nhóm dưới đây. **Nhóm 1 và 2 là bắt buộc**, không có thì hệ thống không chạy.
Nhóm 3 và 4 không có thì vẫn chạy, chỉ mất tính năng.

---

## 1. Wi-Fi cho ESP32 — BẮT BUỘC

**File:** `esp32/SmartDrain_ESP32/Config.h`

```c
#define WIFI_SSID     "YOUR_WIFI_SSID"        // <-- đang là giá trị mẫu
#define WIFI_PASSWORD "YOUR_WIFI_PASSWORD"    // <-- đang là giá trị mẫu
```

Điền tên và mật khẩu Wi-Fi thật, rồi nạp lại chương trình.

> **Cảnh báo cho ngày trình diễn:** ESP32 **không vào được Wi-Fi trường**. Mạng
> trường dùng WPA2-Enterprise (phải đăng nhập bằng tài khoản), ESP32 hỗ trợ rất
> khó khăn. Nhiều mạng còn chặn port 1883.
>
> **Phải dùng hotspot điện thoại**, và thử trước ít nhất một lần bằng đúng chiếc
> điện thoại sẽ mang đi hôm đó. Nếu đến hôm demo mới phát hiện thì hỏng cả phần
> website.
>
> Hotspot phải để băng tần **2.4 GHz**. ESP32 không bắt được 5 GHz. Trên iPhone
> cần bật "Maximize Compatibility", trên Android chọn băng tần 2.4 GHz trong cài
> đặt điểm phát sóng.

**Kiểm tra:** mở Serial Monitor ở tốc độ **9600**, phải thấy:

```
[WIFI] Ket noi thanh cong
[WIFI] IP: 192.168.x.x
[MQTT] Ket noi thanh cong
[MQTT] Da subscribe 4 topic command
```

---

## 2. MQTT — BẮT BUỘC hai bên trùng nhau

Mặc định đã trùng sẵn nên **không cần sửa gì**. Chỉ cần chú ý khi đổi.

| Nơi | Giá trị |
|---|---|
| `esp32/SmartDrain_ESP32/Config.h` | `#define MQTT_BASE "smartdrain"` |
| `web/.env` | `MQTT_BASE=smartdrain` |

**Hai chỗ này phải giống hệt nhau.** Lệch một ký tự là ESP32 và website không
nhìn thấy nhau, mà không có báo lỗi nào — website chỉ hiện "ESP32: Offline".

### Khi nào nên đổi

`broker.hivemq.com` là broker **công cộng**, nhóm khác cũng publish được vào
`smartdrain/...`. Nếu hôm demo thấy dữ liệu lạ nhảy vào, đổi cả hai chỗ thành:

```
smartdrain-24127476
```

Sửa mất 1 phút nhưng **nhớ nạp lại chương trình cho ESP32**, không phải chỉ sửa web.

### Client ID phải khác nhau

| Bên | Client ID |
|---|---|
| ESP32 | `smartdrain-esp32` |
| Website | `smartdrain-web` |

Nếu trùng, hai bên sẽ liên tục đá nhau ra khỏi broker. Dấu hiệu: cứ vài giây lại
mất kết nối một lần.

### Xem trực tiếp dữ liệu trên broker

Khi nghi ngờ, dùng lệnh này để xem mọi bản tin đang chạy:

```bash
mosquitto_sub -h broker.hivemq.com -t "smartdrain/#" -v
```

---

## 3. Firebase — không bắt buộc

**Không cấu hình thì website tự lưu vào SQLite** ở `web/data/smartdrain.db`. Mọi
chức năng lịch sử, biểu đồ, bộ lọc đều chạy bình thường.

Nhưng mục 4.9 báo cáo có nói dùng Firebase, nên nếu kịp thì làm.

### Các bước

1. Vào <https://console.firebase.google.com> → **Add project**
2. Đặt tên, ví dụ `smart-drain`. Tắt Google Analytics cho nhanh.
3. Menu trái → **Build** → **Realtime Database** → **Create Database**
4. Chọn vùng **Singapore (asia-southeast1)** cho gần Việt Nam
5. Chọn **Start in test mode** → Enable
6. Chép URL hiện ở đầu trang, dạng:
   `https://smart-drain-xxxxx-default-rtdb.asia-southeast1.firebasedatabase.app`

### Điền vào `web/.env`

```
FIREBASE_DB_URL=https://smart-drain-xxxxx-default-rtdb.asia-southeast1.firebasedatabase.app
```

Không cần `FIREBASE_AUTH` khi rule đang ở test mode. Website dùng REST API nên
**không cần file service account JSON**.

Khởi động lại website, dòng đầu tiên phải in ra:

```
[STORE] Dung Firebase: https://...
```

### Lưu ý

Test mode cho phép ai biết URL cũng đọc ghi được, và **rule tự hết hạn sau 30
ngày**. Với đồ án thì chấp nhận được, nhưng đừng để dữ liệu thật vào đó.

Nếu Firebase mất kết nối giữa chừng, website vẫn chạy — chỉ ghi log lỗi rồi tiếp
tục. Điều khiển bơm quan trọng hơn việc lưu lịch sử.

---

## 4. Gmail gửi cảnh báo — không bắt buộc

Không cấu hình thì mọi thứ vẫn chạy, chỉ không có email.

### Tạo App password

Gmail **không cho dùng mật khẩu đăng nhập thường** để gửi qua SMTP. Phải tạo mật
khẩu ứng dụng riêng:

1. Vào <https://myaccount.google.com/security>
2. Bật **2-Step Verification** (bắt buộc, không bật thì không tạo được)
3. Tìm **App passwords** (có thể phải gõ vào ô tìm kiếm)
4. Chọn App: *Mail*, Device: *Other* → đặt tên `Smart Drain`
5. Google hiện một chuỗi **16 ký tự**, dạng `abcd efgh ijkl mnop`

Chuỗi này chỉ hiện **một lần duy nhất**, chép ngay.

### Điền vào `web/.env`

```
EMAIL_ENABLED=1
SMTP_HOST=smtp.gmail.com
SMTP_PORT=587
SMTP_USER=tenban@gmail.com
SMTP_PASSWORD=abcdefghijklmnop
EMAIL_TO=nguoinhan@gmail.com
```

`SMTP_PASSWORD` **xoá hết dấu cách** trong chuỗi 16 ký tự.

`EMAIL_TO` nhận nhiều địa chỉ, ngăn nhau bằng dấu phẩy.

### Khi nào email được gửi

Cả ba điều kiện phải cùng đúng:

- Cảnh báo đang có hiệu lực (`active = true`)
- Mức độ là `WARNING` hoặc `DANGER`
- Mã cảnh báo **khác** lần gửi gần nhất

Cộng thêm giới hạn tối thiểu **120 giây** giữa hai email, để không bị spam khi
cảnh báo chớp tắt liên tục. Đổi bằng `EMAIL_MIN_INTERVAL`.

Khởi động website, nếu thiếu thông tin sẽ có dòng cảnh báo ngay trong console.

---

## Danh sách kiểm tra trước ngày trình diễn

- [ ] Đã điền `WIFI_SSID` và `WIFI_PASSWORD` thật vào `Config.h`
- [ ] Đã nạp lại chương trình cho ESP32 sau khi sửa
- [ ] Đã thử hotspot điện thoại **sẽ mang đi hôm đó**, để 2.4 GHz
- [ ] `MQTT_BASE` hai bên giống hệt nhau
- [ ] Serial Monitor để 9600, thấy đủ WiFi + MQTT connected
- [ ] Website mở lên thấy "ESP32: Online"
- [ ] Bật/tắt bơm từ web chạy được
- [ ] Đổ nước thử đủ ba kịch bản: Auto bật bơm, bể xả đầy, ống nghẹt
- [ ] Trang lịch sử có dữ liệu và biểu đồ vẽ được
- [ ] Đã chép `.env.example` thành `.env` (nếu dùng Firebase hoặc email)
- [ ] `.env` **không** được commit lên GitHub

---

## Chạy website

```bash
cd web
pip install -r requirements.txt
python app.py
```

Mở <http://127.0.0.1:5000>

Chi tiết hơn xem `web/README.md`.
