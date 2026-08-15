# Website Smart Drain

Website quản lý hệ thống thoát nước, viết bằng Python Flask + HTML/CSS/JavaScript.
Nhận dữ liệu từ ESP32 qua MQTT, lưu lên Firebase và gửi cảnh báo email.

Phần việc của MSSV 24127476.

---

## Chạy lần đầu

```bash
cd web
pip install -r requirements.txt
python app.py
```

Mở trình duyệt tại **http://127.0.0.1:5000**

Chạy được ngay mà không cần cấu hình gì thêm: khi chưa khai báo Firebase, website
tự lưu vào SQLite ở `web/data/smartdrain.db`.

Điều kiện duy nhất: **ESP32 phải đang chạy và cùng `MQTT_BASE`**. Giá trị mặc
định hai bên đều là `smartdrain`.

---

## Ba trang

| Đường dẫn | Nội dung |
|---|---|
| `/` | Mức nước hai bể, trạng thái bơm, chế độ, ESP32 online/offline, cảnh báo, biểu đồ 10 phút gần nhất |
| `/control` | Chuyển Auto/Manual, bật/tắt bơm, tắt còi, chỉnh sáu ngưỡng vận hành |
| `/history` | Biểu đồ mức nước theo thời gian, nhật ký sự kiện, bộ lọc, xác nhận cảnh báo |

Giao diện tự cập nhật mỗi giây bằng JavaScript, không phải tải lại trang.

---

## Cấu hình

Chép `.env.example` thành `.env` rồi điền thông tin:

```bash
copy .env.example .env
```

File `.env` đã được `.gitignore` bỏ qua vì chứa mật khẩu.

### Firebase

```
FIREBASE_DB_URL=https://smart-drain-xxxxx-default-rtdb.asia-southeast1.firebasedatabase.app
```

Website ghi vào ba nhánh, đúng như mục 4.9 báo cáo:

```
smartdrain/current    trạng thái mới nhất
smartdrain/history    mức nước hai bể theo thời gian
smartdrain/events     nhật ký bật/tắt bơm, đổi chế độ, cảnh báo
```

Dùng REST API nên **không cần file service account JSON**, chỉ cần URL database.
Trong lúc làm đồ án nên để Firebase Rules ở chế độ test.

Nếu Firebase mất kết nối, website vẫn chạy bình thường — chỉ ghi log lỗi.
Điều khiển bơm quan trọng hơn việc lưu lịch sử.

### Email cảnh báo

```
EMAIL_ENABLED=1
SMTP_USER=địa_chỉ_gmail_của_bạn@gmail.com
SMTP_PASSWORD=mật_khẩu_ứng_dụng_16_ký_tự
EMAIL_TO=người_nhận@gmail.com
```

Gmail yêu cầu **App password**, không dùng mật khẩu đăng nhập thường. Tạo tại
Google Account → Security → 2-Step Verification → App passwords.

Email chỉ gửi khi cả ba điều kiện cùng đúng: cảnh báo đang hiệu lực, mức độ
WARNING hoặc DANGER, và mã cảnh báo khác lần gửi trước. Thêm giới hạn tối thiểu
120 giây giữa hai email để không bị spam khi cảnh báo chớp tắt.

---

## Cấu trúc

| File | Nhiệm vụ |
|---|---|
| `app.py` | Flask: ba trang và các API |
| `mqtt_client.py` | Nhận dữ liệu ESP32, giữ trạng thái, ghi sự kiện, gửi lệnh |
| `store.py` | Lưu trữ — Firebase hoặc SQLite, cùng một giao diện |
| `notifier.py` | Gửi email cảnh báo |
| `config.py` | Toàn bộ cấu hình, đọc từ `.env` |
| `static/js/chart.js` | Biểu đồ vẽ bằng Canvas thuần |

Biểu đồ **cố ý không dùng Chart.js**: thư viện đó phải tải từ CDN, mà hôm trình
diễn rất có thể không có Internet vì ESP32 chạy bằng hotspot điện thoại.

---

## Nguyên tắc quan trọng

**ESP32 là nguồn sự thật duy nhất.** Bấm nút trên web không làm giao diện tự đổi
trạng thái — nó chờ ESP32 publish lại `pump/state` rồi mới vẽ theo dữ liệu thật.
Nhờ vậy khi lệnh bị từ chối (đang ở Auto, bể xả đầy, đang có lỗi chốt), màn hình
vẫn phản ánh đúng phần cứng thay vì hiện sai.

**Lệnh tắt bơm không bao giờ bị chặn**, kể cả khi ESP32 báo lỗi. Đây là nút an
toàn cuối cùng.

**Kiểm tra hai lớp.** Website tự kiểm tra trước khi gửi (đúng mục 4.8 báo cáo:
chỉ gửi lệnh bật bơm khi đang ở Manual và bể xả chưa vượt ngưỡng), rồi ESP32
kiểm tra lại lần nữa. Lớp trên cho thông báo lỗi dễ hiểu, lớp dưới đảm bảo an toàn
thật sự.

---

## API

| Phương thức | Đường dẫn | Nội dung |
|---|---|---|
| GET | `/api/state` | Toàn bộ trạng thái hiện tại |
| POST | `/api/command/pump` | `{"action": "ON"\|"OFF"}` |
| POST | `/api/command/mode` | `{"mode": "AUTO"\|"MANUAL"}` |
| POST | `/api/command/buzzer` | `{"action": "MUTE"\|"UNMUTE"}` |
| POST | `/api/command/config` | `{"key": "pump_start", "value": 75}` |
| POST | `/api/alert/ack` | `{"id": "..."}` |
| GET | `/api/history` | `?hours=6` hoặc `?from=2026-08-15&to=2026-08-16` |
| GET | `/api/events` | Thêm `&type=ALERT` để lọc |

---

## Khi có lỗi

**Trang hiện "ESP32: Offline"** — kiểm tra ESP32 đã nối Wi-Fi chưa (xem Serial
Monitor), và `MQTT_BASE` hai bên có trùng nhau không.

**Không thấy dữ liệu dù ESP32 báo đã publish** — nhiều khả năng hai bên đang khác
`MQTT_BASE`. Xem toàn bộ bản tin thật trên broker:

```bash
mosquitto_sub -h broker.hivemq.com -t "smartdrain/#" -v
```

**Mất kết nối liên tục vài giây một lần** — hai client trùng `MQTT_CLIENT_ID` và
đang đá nhau ra khỏi broker. ESP32 phải là `smartdrain-esp32`, website phải là
`smartdrain-web`.

**Dữ liệu lạ không phải của nhóm** — broker HiveMQ là công cộng. Đổi `MQTT_BASE`
thành `smartdrain-24127476` ở **cả** `.env` và `esp32/SmartDrain_ESP32/Config.h`.
