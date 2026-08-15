"""
mqtt_client.py - Cau noi giua ESP32 va website.

Nhiem vu:
  - Nhan du lieu ESP32 gui len, giai ma va giu trang thai moi nhat trong RAM
  - Ghi nhat ky su kien khi trang thai thay doi
  - Luu lich su theo chu ky
  - Gui email khi co canh bao
  - Gui lenh dieu khien nguoc lai ESP32

Toan bo trang thai duoc bao ve bang mot khoa (RLock) vi luong MQTT va luong
Flask doc ghi dong thoi.

Nguon su that duy nhat la ESP32. Website khong bao gio tu doan trang thai bom:
bam nut xong van phai doi ESP32 publish lai pump/state roi moi doi giao dien.
Nho vay man hinh luon dung voi phan cung, ke ca khi lenh bi tu choi.
"""

import json
import threading
import time
from datetime import datetime

import paho.mqtt.client as mqtt

import config


def now_ms():
    return int(time.time() * 1000)


def format_time(ms):
    return datetime.fromtimestamp(ms / 1000).strftime("%d/%m/%Y %H:%M:%S")


def empty_state():
    return {
        # Ket noi
        "esp32_online": False,
        "mqtt_connected": False,
        "last_seen": None,
        "last_seen_text": "chưa nhận được dữ liệu",
        # Du lieu tu ESP32
        "input": {"percent": None, "adc": None, "level": None},
        "output": {"percent": None, "adc": None, "level": None},
        "pump": {"state": None, "source": None, "runtime": 0},
        "mode": None,
        "alert": {
            "code": "NONE",
            "severity": "INFO",
            "message": "",
            "active": False,
            "muted": False,
        },
        "config": {},
        "uptime": 0,
    }


class SmartDrainClient:
    def __init__(self, store, notifier):
        self.store = store
        self.notifier = notifier

        self.lock = threading.RLock()
        self.state = empty_state()

        self._last_history_save = 0.0
        self._last_alert_code = "NONE"
        self._running = True

        self.client = self._build_client()

    # -- khoi tao ----------------------------------------------------------

    def _build_client(self):
        # paho-mqtt 2.x doi chu ky Client(); ho tro ca hai phien ban
        try:
            client = mqtt.Client(
                mqtt.CallbackAPIVersion.VERSION1,
                client_id=config.MQTT_CLIENT_ID,
            )
        except AttributeError:  # paho-mqtt 1.x
            client = mqtt.Client(client_id=config.MQTT_CLIENT_ID)

        client.on_connect = self._on_connect
        client.on_disconnect = self._on_disconnect
        client.on_message = self._on_message
        return client

    def start(self):
        self.client.connect_async(
            config.MQTT_HOST, config.MQTT_PORT, keepalive=config.MQTT_KEEP_ALIVE
        )
        self.client.loop_start()

        threading.Thread(target=self._background_loop, daemon=True).start()

        print(
            f"[MQTT] Dang ket noi {config.MQTT_HOST}:{config.MQTT_PORT} "
            f"| topic goc: {config.MQTT_BASE}/",
            flush=True,
        )

    def stop(self):
        self._running = False
        self.client.loop_stop()
        self.client.disconnect()

    # -- callback ----------------------------------------------------------

    def _on_connect(self, client, userdata, flags, rc, properties=None):
        if rc != 0:
            print(f"[MQTT] Ket noi that bai, ma loi rc={rc}", flush=True)
            return

        with self.lock:
            self.state["mqtt_connected"] = True

        for topic in config.SUBSCRIBE_TOPICS:
            client.subscribe(topic, qos=1)

        print(f"[MQTT] Da ket noi, subscribe {len(config.SUBSCRIBE_TOPICS)} topic", flush=True)

    def _on_disconnect(self, client, userdata, rc, properties=None):
        with self.lock:
            self.state["mqtt_connected"] = False
        if self._running:
            print("[MQTT] Mat ket noi broker, dang thu lai...", flush=True)

    def _on_message(self, client, userdata, message):
        topic = message.topic
        raw = message.payload.decode("utf-8", errors="replace").strip()

        try:
            self._handle_message(topic, raw)
        except Exception as exc:  # khong de mot ban tin loi lam chet luong MQTT
            print(f"[MQTT] Loi xu ly {topic}: {exc} | payload={raw!r}", flush=True)

    # -- xu ly tung topic --------------------------------------------------

    def _handle_message(self, topic, raw):
        with self.lock:
            timestamp = now_ms()
            self.state["last_seen"] = timestamp
            self.state["last_seen_text"] = format_time(timestamp)

            if topic == config.TOPIC_STATUS:
                # Chuoi thuan ONLINE / OFFLINE, do ESP32 hoac broker gui (LWT)
                self._handle_status(raw, timestamp)
                return

            data = self._parse_json(topic, raw)
            if data is None:
                return

            if topic == config.TOPIC_TANK_INPUT:
                self.state["input"] = self._tank(data)
                self.state["esp32_online"] = True
            elif topic == config.TOPIC_TANK_OUTPUT:
                self.state["output"] = self._tank(data)
                self.state["esp32_online"] = True
            elif topic == config.TOPIC_PUMP_STATE:
                self._handle_pump(data, timestamp)
            elif topic == config.TOPIC_MODE:
                self._handle_mode(data, timestamp)
            elif topic == config.TOPIC_ALERT:
                self._handle_alert(data, timestamp)
            elif topic == config.TOPIC_CONFIG:
                self.state["config"] = data

            if "uptime" in data:
                self.state["uptime"] = data["uptime"]

            self.store.save_current(self._public_state_locked())

    @staticmethod
    def _parse_json(topic, raw):
        try:
            data = json.loads(raw)
        except ValueError:
            print(f"[MQTT] {topic}: payload khong phai JSON: {raw!r}", flush=True)
            return None

        if not isinstance(data, dict):
            print(f"[MQTT] {topic}: payload khong phai object: {raw!r}", flush=True)
            return None
        return data

    @staticmethod
    def _tank(data):
        return {
            "percent": data.get("percent"),
            "adc": data.get("adc"),
            "level": data.get("level"),
        }

    def _handle_status(self, raw, timestamp):
        online = raw.upper() == "ONLINE"
        was_online = self.state["esp32_online"]
        self.state["esp32_online"] = online

        if online != was_online:
            self._log_event(
                "CONNECTION",
                "ESP32 đã kết nối" if online else "ESP32 mất kết nối",
                {"online": online},
                timestamp,
            )

    def _handle_pump(self, data, timestamp):
        previous = self.state["pump"].get("state")
        new_state = data.get("state")

        self.state["pump"] = {
            "state": new_state,
            "source": data.get("source"),
            "runtime": data.get("runtime", 0),
        }
        self.state["esp32_online"] = True

        if previous is not None and previous != new_state:
            source = config.PUMP_SOURCE_VI.get(
                data.get("source", ""), data.get("source", "")
            )
            self._log_event(
                "PUMP",
                f"Máy bơm {'BẬT' if new_state == 'ON' else 'TẮT'} ({source})",
                {"state": new_state, "source": data.get("source")},
                timestamp,
            )

    def _handle_mode(self, data, timestamp):
        previous = self.state["mode"]
        new_mode = data.get("mode")
        self.state["mode"] = new_mode
        self.state["esp32_online"] = True

        if previous is not None and previous != new_mode:
            self._log_event(
                "MODE",
                f"Chuyển sang chế độ {new_mode}",
                {"mode": new_mode},
                timestamp,
            )

    def _handle_alert(self, data, timestamp):
        code = data.get("code", "NONE")

        self.state["alert"] = {
            "code": code,
            "severity": data.get("severity", "INFO"),
            "message": data.get("message", ""),
            "message_vi": config.ALERT_MESSAGES_VI.get(code, data.get("message", "")),
            "active": bool(data.get("active")),
            "muted": bool(data.get("muted")),
        }
        self.state["esp32_online"] = True

        if code != self._last_alert_code:
            self._last_alert_code = code

            if code != "NONE":
                self._log_event(
                    "ALERT",
                    config.ALERT_MESSAGES_VI.get(code, code),
                    {"code": code, "severity": data.get("severity")},
                    timestamp,
                )
                self.notifier.notify(self.state["alert"], self._public_state_locked())
            else:
                self._log_event("ALERT_CLEAR", "Cảnh báo đã được gỡ bỏ", {}, timestamp)
                self.notifier.reset_on_clear(self.state["alert"])

    # -- nhat ky va lich su ------------------------------------------------

    def _log_event(self, event_type, message, detail, timestamp):
        event = {
            "ts": timestamp,
            "time_text": format_time(timestamp),
            "type": event_type,
            "message": message,
            "detail": detail or {},
            "acked": False,
        }
        try:
            self.store.save_event(event)
        except Exception as exc:
            print(f"[STORE] Khong ghi duoc su kien: {exc}", flush=True)

        print(f"[EVENT] {event_type}: {message}", flush=True)

    def _save_history_sample(self):
        with self.lock:
            if self.state["input"]["percent"] is None:
                return  # chua co du lieu that thi khong luu

            sample = {
                "ts": now_ms(),
                "input_percent": self.state["input"]["percent"],
                "output_percent": self.state["output"]["percent"],
                "pump_state": self.state["pump"]["state"],
                "mode": self.state["mode"],
                "alert_code": self.state["alert"]["code"],
            }

        try:
            self.store.save_history(sample)
        except Exception as exc:
            print(f"[STORE] Khong ghi duoc lich su: {exc}", flush=True)

    def _background_loop(self):
        """Luu lich su theo chu ky va phat hien ESP32 im lang qua lau."""
        while self._running:
            time.sleep(1.0)

            now = time.time()
            if now - self._last_history_save >= config.HISTORY_SAMPLE_INTERVAL:
                self._last_history_save = now
                self._save_history_sample()

            self._check_offline()

    def _check_offline(self):
        """LWT lo truong hop ESP32 tu ngat. Nhung neu ca duong mang giua ESP32 va
        broker dut thi khong ai gui LWT ca, nen van phai tu dem thoi gian im lang."""
        with self.lock:
            last_seen = self.state["last_seen"]
            if not last_seen or not self.state["esp32_online"]:
                return

            silent_seconds = (now_ms() - last_seen) / 1000
            if silent_seconds < config.ESP32_OFFLINE_TIMEOUT:
                return

            self.state["esp32_online"] = False
            timestamp = now_ms()

        self._log_event(
            "CONNECTION",
            f"ESP32 mất kết nối (không có dữ liệu {int(silent_seconds)} giây)",
            {"online": False, "reason": "timeout"},
            timestamp,
        )

    # -- doc trang thai ----------------------------------------------------

    def _public_state_locked(self):
        """Ban sao trang thai kem cac truong da dich sang tieng Viet.
        Chi goi khi dang giu khoa."""
        state = json.loads(json.dumps(self.state))

        for tank in ("input", "output"):
            level = state[tank].get("level")
            state[tank]["level_vi"] = config.LEVEL_NAMES_VI.get(level, level)

        source = state["pump"].get("source")
        state["pump"]["source_vi"] = config.PUMP_SOURCE_VI.get(source, source)

        code = state["alert"].get("code", "NONE")
        state["alert"]["message_vi"] = config.ALERT_MESSAGES_VI.get(
            code, state["alert"].get("message", "")
        )

        state["server_time"] = format_time(now_ms())
        return state

    def get_state(self):
        with self.lock:
            return self._public_state_locked()

    # -- gui lenh ----------------------------------------------------------

    def _publish_command(self, topic, payload):
        if not self.client.is_connected():
            return False, "Website chưa kết nối được MQTT broker"

        # retain = False: neu giu lai, ESP32 se nhan lai lenh cu moi lan khoi
        # dong va tu bat bom - rat nguy hiem
        result = self.client.publish(topic, payload, qos=1, retain=False)
        if result.rc != mqtt.MQTT_ERR_SUCCESS:
            return False, f"Gửi lệnh thất bại (mã {result.rc})"

        print(f"[MQTT] -> {topic} = {payload}", flush=True)
        return True, "Đã gửi lệnh"

    def command_pump(self, action):
        action = (action or "").upper()
        if action not in ("ON", "OFF"):
            return False, "Lệnh bơm không hợp lệ"

        with self.lock:
            mode = self.state["mode"]
            output_percent = self.state["output"].get("percent")
            limit = self.state["config"].get("output_limit", 80)
            online = self.state["esp32_online"]

        if not online:
            return False, "ESP32 đang offline, không gửi được lệnh"

        # Lenh TAT luon duoc phep - day la nut an toan cuoi cung
        if action == "OFF":
            return self._publish_command(config.TOPIC_COMMAND_PUMP, "OFF")

        # Muc 4.8 bao cao: website chi gui lenh bat bom khi dang o MANUAL va be
        # xa chua vuot nguong an toan. ESP32 cung tu kiem tra lai lan nua.
        if mode != "MANUAL":
            return False, "Chỉ bật bơm được ở chế độ Manual"

        if output_percent is not None and output_percent >= limit:
            return False, f"Bể xả đã đạt {output_percent}%, không thể bật bơm"

        return self._publish_command(config.TOPIC_COMMAND_PUMP, "ON")

    def command_mode(self, mode):
        mode = (mode or "").upper()
        if mode not in ("AUTO", "MANUAL"):
            return False, "Chế độ không hợp lệ"

        with self.lock:
            if not self.state["esp32_online"]:
                return False, "ESP32 đang offline, không gửi được lệnh"

        return self._publish_command(config.TOPIC_COMMAND_MODE, mode)

    def command_buzzer(self, action):
        action = (action or "").upper()
        if action not in ("MUTE", "UNMUTE"):
            return False, "Lệnh còi không hợp lệ"

        with self.lock:
            if not self.state["esp32_online"]:
                return False, "ESP32 đang offline, không gửi được lệnh"

        return self._publish_command(config.TOPIC_COMMAND_BUZZER, action)

    def command_config(self, key, value):
        field = config.CONFIG_FIELDS.get(key)
        if field is None:
            return False, f"Không có ngưỡng tên '{key}'"

        try:
            number = int(value)
        except (TypeError, ValueError):
            return False, "Giá trị phải là số nguyên"

        if not field["min"] <= number <= field["max"]:
            return False, (
                f"{field['label']} phải nằm trong khoảng "
                f"{field['min']}–{field['max']}"
            )

        with self.lock:
            if not self.state["esp32_online"]:
                return False, "ESP32 đang offline, không gửi được lệnh"

            current = dict(self.state["config"])

        # Kiem tra truoc o day de bao loi ro rang cho nguoi dung. ESP32 cung tu
        # kiem tra lai va se tu choi neu khong hop le.
        candidate = dict(current)
        candidate[key] = number
        if "pump_start" in candidate and "pump_stop" in candidate:
            if candidate["pump_stop"] >= candidate["pump_start"]:
                return False, "Ngưỡng tắt bơm phải nhỏ hơn ngưỡng bật bơm"

        return self._publish_command(config.TOPIC_COMMAND_CONFIG, f"{key}={number}")
