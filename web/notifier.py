"""
notifier.py - Gui email canh bao khi he thong gap tinh trang nguy hiem.

Muc 3.2 va muc 4.9 bao cao: khi xuat hien canh bao thi gui thong bao gom loai
canh bao, muc nuoc hai be, trang thai may bom va thoi diem xay ra.

Email duoc gui trong luong rieng de khong lam nghen luong MQTT: neu may chu SMTP
phan hoi cham vai giay ma chan luong MQTT thi lenh dieu khien bom cung bi cham
theo, dieu do nguy hiem hon nhieu so voi viec email den tre.
"""

import smtplib
import threading
import time
from email.message import EmailMessage
from email.utils import formatdate

import config


class EmailNotifier:
    def __init__(self):
        self.lock = threading.Lock()
        self.last_sent_at = 0.0
        self.last_code = None
        self.enabled = bool(
            config.EMAIL_ENABLED
            and config.SMTP_USER
            and config.SMTP_PASSWORD
            and config.EMAIL_TO
        )

        if config.EMAIL_ENABLED and not self.enabled:
            print(
                "[EMAIL] Da bat EMAIL_ENABLED nhung thieu SMTP_USER / "
                "SMTP_PASSWORD / EMAIL_TO, tam thoi khong gui",
                flush=True,
            )

    # -- quyet dinh co gui hay khong ---------------------------------------

    def should_send(self, alert):
        """Chi gui khi that su can. Ba dieu kien phai cung dung."""
        if not self.enabled:
            return False

        code = alert.get("code", "NONE")
        severity = alert.get("severity", "INFO")

        # 1. Phai la canh bao dang hieu luc va du muc nghiem trong
        if code == "NONE" or not alert.get("active"):
            return False
        if severity not in config.EMAIL_SEVERITIES:
            return False

        with self.lock:
            # 2. Cung mot ma canh bao thi chi gui mot lan
            if code == self.last_code:
                return False

            # 3. Khong gui qua day, tranh spam khi canh bao chop tat lien tuc
            if time.time() - self.last_sent_at < config.EMAIL_MIN_INTERVAL:
                return False

        return True

    # -- gui ---------------------------------------------------------------

    def notify(self, alert, state):
        if not self.should_send(alert):
            return

        with self.lock:
            self.last_code = alert.get("code")
            self.last_sent_at = time.time()

        thread = threading.Thread(
            target=self._send_safely,
            args=(dict(alert), dict(state)),
            daemon=True,
        )
        thread.start()

    def reset_on_clear(self, alert):
        """Khi het canh bao thi xoa ma da gui, de lan sau canh bao cung loai
        quay lai van duoc bao."""
        if alert.get("code") == "NONE" or not alert.get("active"):
            with self.lock:
                self.last_code = None

    def _send_safely(self, alert, state):
        try:
            self._send(alert, state)
        except Exception as exc:  # pragma: no cover - phu thuoc mang
            print(f"[EMAIL] Gui that bai: {exc}", flush=True)

    def _send(self, alert, state):
        code = alert.get("code", "NONE")
        title = config.ALERT_MESSAGES_VI.get(code, code)

        message = EmailMessage()
        message["Subject"] = f"[Smart Drain] {title}"
        message["From"] = config.EMAIL_FROM or config.SMTP_USER
        message["To"] = ", ".join(config.EMAIL_TO)
        message["Date"] = formatdate(localtime=True)
        message.set_content(self._build_body(alert, state, title))

        with smtplib.SMTP(config.SMTP_HOST, config.SMTP_PORT, timeout=20) as smtp:
            smtp.ehlo()
            smtp.starttls()
            smtp.login(config.SMTP_USER, config.SMTP_PASSWORD)
            smtp.send_message(message)

        print(f"[EMAIL] Da gui canh bao {code} toi {config.EMAIL_TO}", flush=True)

    @staticmethod
    def _build_body(alert, state, title):
        tank_input = state.get("input") or {}
        tank_output = state.get("output") or {}
        pump = state.get("pump") or {}

        pump_state = pump.get("state", "?")
        pump_source = config.PUMP_SOURCE_VI.get(
            pump.get("source", ""), pump.get("source", "?")
        )

        return (
            "HE THONG SMART DRAIN - CANH BAO\n"
            "===============================\n\n"
            f"Loai canh bao : {title}\n"
            f"Ma canh bao   : {alert.get('code')}\n"
            f"Muc do        : {alert.get('severity')}\n"
            f"Thoi diem     : {state.get('last_seen_text', 'khong ro')}\n\n"
            "TRANG THAI HE THONG\n"
            "-------------------\n"
            f"Be thu        : {tank_input.get('percent', '?')}% "
            f"({tank_input.get('level', '?')})\n"
            f"Be xa         : {tank_output.get('percent', '?')}% "
            f"({tank_output.get('level', '?')})\n"
            f"May bom       : {pump_state} (nguyen nhan: {pump_source})\n"
            f"Che do        : {state.get('mode', '?')}\n"
            "Email nay duoc gui tu dong boi he thong Smart Drain.\n"
        )
