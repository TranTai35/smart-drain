"""
config.py - Toan bo cau hinh cua website Smart Drain.

Moi thong so deu doc tu bien moi truong, co gia tri mac dinh dung ngay duoc.
Tao file .env (chep tu .env.example) de dat thong tin rieng: Firebase, email...
Khong commit file .env len GitHub vi trong do co mat khau.
"""

import os

try:
    from dotenv import load_dotenv

    load_dotenv()
except ImportError:  # pragma: no cover - chay duoc ca khi chua cai python-dotenv
    pass


def _get(name, default):
    value = os.environ.get(name, "").strip()
    return value if value else default


def _get_int(name, default):
    try:
        return int(_get(name, str(default)))
    except ValueError:
        return default


def _get_bool(name, default=False):
    return _get(name, "1" if default else "0").lower() in ("1", "true", "yes", "on")


# ---------------------------------------------------------------------------
# MQTT - phai trung voi Config.h ben ESP32
# ---------------------------------------------------------------------------

MQTT_HOST = _get("MQTT_HOST", "broker.hivemq.com")
MQTT_PORT = _get_int("MQTT_PORT", 1883)
MQTT_KEEP_ALIVE = _get_int("MQTT_KEEP_ALIVE", 30)

# Doi BASE thi phai doi ca MQTT_BASE trong esp32/SmartDrain_ESP32/Config.h
MQTT_BASE = _get("MQTT_BASE", "smartdrain")

# Phai khac client id cua ESP32 ("smartdrain-esp32"), neu trung se da nhau ra
MQTT_CLIENT_ID = _get("MQTT_CLIENT_ID", "smartdrain-web")

TOPIC_TANK_INPUT = f"{MQTT_BASE}/tank/input"
TOPIC_TANK_OUTPUT = f"{MQTT_BASE}/tank/output"
TOPIC_PUMP_STATE = f"{MQTT_BASE}/pump/state"
TOPIC_MODE = f"{MQTT_BASE}/mode"
TOPIC_ALERT = f"{MQTT_BASE}/alert"
TOPIC_STATUS = f"{MQTT_BASE}/status"
TOPIC_CONFIG = f"{MQTT_BASE}/config"

TOPIC_COMMAND_PUMP = f"{MQTT_BASE}/command/pump"
TOPIC_COMMAND_MODE = f"{MQTT_BASE}/command/mode"
TOPIC_COMMAND_BUZZER = f"{MQTT_BASE}/command/buzzer"
TOPIC_COMMAND_CONFIG = f"{MQTT_BASE}/command/config"

SUBSCRIBE_TOPICS = [
    TOPIC_TANK_INPUT,
    TOPIC_TANK_OUTPUT,
    TOPIC_PUMP_STATE,
    TOPIC_MODE,
    TOPIC_ALERT,
    TOPIC_STATUS,
    TOPIC_CONFIG,
]

# ---------------------------------------------------------------------------
# ESP32
# ---------------------------------------------------------------------------

# Neu qua ngan nay giay ma khong nhan duoc ban tin nao thi coi ESP32 mat ket noi.
# ESP32 gui muc nuoc moi 2 giay nen 15 giay la du rong de khong bao nham.
ESP32_OFFLINE_TIMEOUT = _get_int("ESP32_OFFLINE_TIMEOUT", 15)

# ---------------------------------------------------------------------------
# Luu tru
# ---------------------------------------------------------------------------

# De trong thi website tu dong dung SQLite trong web/data/smartdrain.db.
# Vi du: https://smart-drain-xxxx-default-rtdb.asia-southeast1.firebasedatabase.app
FIREBASE_DB_URL = _get("FIREBASE_DB_URL", "")

# Database secret hoac ID token. De trong neu rule dang o che do test.
FIREBASE_AUTH = _get("FIREBASE_AUTH", "")

SQLITE_PATH = _get(
    "SQLITE_PATH",
    os.path.join(os.path.dirname(os.path.abspath(__file__)), "data", "smartdrain.db"),
)

# Chu ky luu mot ban ghi lich su, tinh bang giay.
# Muc 4.9 bao cao: luu khi trang thai doi hoac theo chu ky, tranh qua nhieu ban ghi.
HISTORY_SAMPLE_INTERVAL = _get_int("HISTORY_SAMPLE_INTERVAL", 10)

# ---------------------------------------------------------------------------
# Email canh bao
# ---------------------------------------------------------------------------

EMAIL_ENABLED = _get_bool("EMAIL_ENABLED", False)
SMTP_HOST = _get("SMTP_HOST", "smtp.gmail.com")
SMTP_PORT = _get_int("SMTP_PORT", 587)
SMTP_USER = _get("SMTP_USER", "")

# Voi Gmail phai dung "App password" 16 ky tu, khong dung mat khau tai khoan.
SMTP_PASSWORD = _get("SMTP_PASSWORD", "")
EMAIL_FROM = _get("EMAIL_FROM", SMTP_USER)
EMAIL_TO = [e.strip() for e in _get("EMAIL_TO", "").split(",") if e.strip()]

# Khoang cach toi thieu giua hai email, tranh spam khi canh bao chop tat lien tuc
EMAIL_MIN_INTERVAL = _get_int("EMAIL_MIN_INTERVAL", 120)

# Chi gui email cho cac muc do nay
EMAIL_SEVERITIES = {"DANGER", "WARNING"}

# ---------------------------------------------------------------------------
# Flask
# ---------------------------------------------------------------------------

FLASK_HOST = _get("FLASK_HOST", "0.0.0.0")
FLASK_PORT = _get_int("FLASK_PORT", 5000)
FLASK_DEBUG = _get_bool("FLASK_DEBUG", False)

# ---------------------------------------------------------------------------
# Hien thi
# ---------------------------------------------------------------------------

# Ten tieng Viet co dau cho tung ma canh bao. ESP32 gui chuoi khong dau vi phai
# dung chung cho LCD 1602, website hien thi lai cho de doc.
ALERT_MESSAGES_VI = {
    "NONE": "Hệ thống bình thường",
    "INPUT_TANK_DANGER": "Bể thu ở mức nguy hiểm",
    "OUTPUT_TANK_FULL": "Bể xả đã đầy, đã dừng bơm",
    "PUMP_TIMEOUT": "Bơm chạy quá thời gian cho phép",
    "DRAIN_ABNORMAL": "Thoát nước bất thường, kiểm tra ống",
    "SENSOR_ERROR": "Cảm biến không phản hồi",
}

LEVEL_NAMES_VI = {
    "LOW": "Thấp",
    "MEDIUM": "Trung bình",
    "HIGH": "Cao",
    "DANGER": "Nguy hiểm",
}

# Ten tieng Viet cho ly do bom doi trang thai (truong "source")
PUMP_SOURCE_VI = {
    "BOOT": "Khởi động",
    "AUTO": "Tự động",
    "MANUAL": "Thủ công",
    "SAFETY": "An toàn",
}

# Cac nguong chinh duoc tu web, kem khoang hop le.
# Phai trung voi kiem tra trong RuntimeConfig.cpp ben ESP32.
CONFIG_FIELDS = {
    "pump_start": {"label": "Ngưỡng bật bơm (%)", "min": 1, "max": 100},
    "pump_stop": {"label": "Ngưỡng tắt bơm (%)", "min": 0, "max": 99},
    "output_limit": {"label": "Giới hạn bể xả (%)", "min": 1, "max": 100},
    "max_runtime": {"label": "Thời gian bơm tối đa (giây)", "min": 10, "max": 600},
    "drain_check": {"label": "Chu kỳ kiểm tra thoát nước (giây)", "min": 5, "max": 300},
    "drain_min_drop": {"label": "Mức giảm tối thiểu (%)", "min": 1, "max": 100},
}
