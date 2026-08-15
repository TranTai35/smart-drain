"""
app.py - Website quan ly Smart Drain (Python Flask).

Ba trang theo dung muc 4.8 bao cao:
  /          Dashboard  - muc nuoc hai be, trang thai bom, che do, ESP32, canh bao
  /control   Dieu khien - chuyen Auto/Manual, bat/tat bom, tat coi, chinh nguong
  /history   Lich su    - bieu do muc nuoc va nhat ky su kien, co bo loc thoi gian

Giao dien tu cap nhat bang JavaScript, khong phai tai lai trang.

Chay:
    pip install -r requirements.txt
    python app.py
"""

import time
from datetime import datetime, timedelta

from flask import Flask, jsonify, render_template, request

import config
from mqtt_client import SmartDrainClient
from notifier import EmailNotifier
from store import create_store

app = Flask(__name__)

store = create_store()
notifier = EmailNotifier()
client = SmartDrainClient(store, notifier)


# ---------------------------------------------------------------------------
# Trang
# ---------------------------------------------------------------------------


@app.route("/")
def page_dashboard():
    return render_template("dashboard.html", active_page="dashboard")


@app.route("/control")
def page_control():
    return render_template(
        "control.html",
        active_page="control",
        config_fields=config.CONFIG_FIELDS,
    )


@app.route("/history")
def page_history():
    return render_template("history.html", active_page="history")


# ---------------------------------------------------------------------------
# API - trang thai
# ---------------------------------------------------------------------------


@app.route("/api/state")
def api_state():
    """Dashboard goi moi giay de cap nhat giao dien."""
    state = client.get_state()
    state["store"] = store.name
    state["email_enabled"] = notifier.enabled
    return jsonify(state)


# ---------------------------------------------------------------------------
# API - dieu khien
# ---------------------------------------------------------------------------


def _command_response(ok, message):
    """Tra ve kem trang thai moi nhat de giao dien cap nhat ngay, khong phai doi
    lan poll ke tiep."""
    return jsonify({"ok": ok, "message": message, "state": client.get_state()}), (
        200 if ok else 400
    )


@app.route("/api/command/pump", methods=["POST"])
def api_command_pump():
    data = request.get_json(silent=True) or {}
    ok, message = client.command_pump(data.get("action"))
    return _command_response(ok, message)


@app.route("/api/command/mode", methods=["POST"])
def api_command_mode():
    data = request.get_json(silent=True) or {}
    ok, message = client.command_mode(data.get("mode"))
    return _command_response(ok, message)


@app.route("/api/command/buzzer", methods=["POST"])
def api_command_buzzer():
    data = request.get_json(silent=True) or {}
    ok, message = client.command_buzzer(data.get("action"))
    return _command_response(ok, message)


@app.route("/api/command/config", methods=["POST"])
def api_command_config():
    data = request.get_json(silent=True) or {}
    ok, message = client.command_config(data.get("key"), data.get("value"))
    return _command_response(ok, message)


# ---------------------------------------------------------------------------
# API - xac nhan canh bao
# ---------------------------------------------------------------------------


@app.route("/api/alert/ack", methods=["POST"])
def api_alert_ack():
    """Muc 3.2: cho phep nguoi quan ly danh dau canh bao da duoc xu ly.

    Viec nay hoan toan o phia website - chi ghi mot co vao co so du lieu, khong
    gui gi xuong ESP32. Canh bao chot ben ESP32 duoc go bang lenh tat bom hoac
    doi che do, la hai viec khac han.
    """
    data = request.get_json(silent=True) or {}
    event_id = data.get("id")

    if not event_id:
        return jsonify({"ok": False, "message": "Thiếu id sự kiện"}), 400

    try:
        ok = store.ack_event(event_id)
    except Exception as exc:
        return jsonify({"ok": False, "message": f"Lỗi ghi dữ liệu: {exc}"}), 500

    if not ok:
        return jsonify({"ok": False, "message": "Sự kiện không tồn tại hoặc đã xác nhận"}), 400

    return jsonify({"ok": True, "message": "Đã xác nhận cảnh báo"})


# ---------------------------------------------------------------------------
# API - lich su
# ---------------------------------------------------------------------------


def _parse_range():
    """Doc khoang thoi gian tu query string.

    Nhan 'from' va 'to' dang YYYY-MM-DD (input type=date cua HTML), hoac 'hours'
    de lay N gio gan nhat. Mac dinh la 6 gio gan nhat.
    """
    now = datetime.now()

    hours = request.args.get("hours")
    if hours:
        try:
            start = now - timedelta(hours=float(hours))
            return int(start.timestamp() * 1000), int(now.timestamp() * 1000)
        except ValueError:
            pass

    from_text = request.args.get("from", "").strip()
    to_text = request.args.get("to", "").strip()

    if from_text or to_text:
        try:
            start = (
                datetime.strptime(from_text, "%Y-%m-%d")
                if from_text
                else now - timedelta(days=1)
            )
            # Lay het ngay ket thuc, khong phai 00:00 cua ngay do
            end = (
                datetime.strptime(to_text, "%Y-%m-%d") + timedelta(days=1)
                if to_text
                else now
            )
            return int(start.timestamp() * 1000), int(end.timestamp() * 1000)
        except ValueError:
            pass

    start = now - timedelta(hours=6)
    return int(start.timestamp() * 1000), int(now.timestamp() * 1000)


def _parse_limit(default, maximum):
    try:
        return max(1, min(int(request.args.get("limit", default)), maximum))
    except (TypeError, ValueError):
        return default


@app.route("/api/history")
def api_history():
    start_ms, end_ms = _parse_range()
    limit = _parse_limit(500, 5000)

    try:
        rows = store.read_history(start_ms, end_ms, limit)
    except Exception as exc:
        return jsonify({"ok": False, "message": str(exc), "items": []}), 500

    return jsonify(
        {"ok": True, "items": rows, "from": start_ms, "to": end_ms, "count": len(rows)}
    )


@app.route("/api/events")
def api_events():
    start_ms, end_ms = _parse_range()
    limit = _parse_limit(100, 1000)

    try:
        rows = store.read_events(start_ms, end_ms, limit)
    except Exception as exc:
        return jsonify({"ok": False, "message": str(exc), "items": []}), 500

    event_type = request.args.get("type", "").strip().upper()
    if event_type:
        rows = [row for row in rows if row.get("type") == event_type]

    return jsonify({"ok": True, "items": rows, "count": len(rows)})


# ---------------------------------------------------------------------------

def main():
    client.start()

    # Cho mot chut de nhan cac ban tin retained, giao dien co du lieu ngay khi mo
    time.sleep(1.0)

    print(f"[WEB] Mo trinh duyet: http://127.0.0.1:{config.FLASK_PORT}", flush=True)

    # use_reloader=False: reloader se chay file nay hai lan, tao hai MQTT client
    # trung client id va chung se da nhau ra khoi broker lien tuc
    app.run(
        host=config.FLASK_HOST,
        port=config.FLASK_PORT,
        debug=config.FLASK_DEBUG,
        use_reloader=False,
        threaded=True,
    )


if __name__ == "__main__":
    main()
