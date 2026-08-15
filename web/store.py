"""
store.py - Luu trang thai hien tai, lich su va nhat ky su kien.

Co hai backend cung mot giao dien:

  FirebaseStore  dung khi da dat FIREBASE_DB_URL trong .env
  SqliteStore    dung khi chua cau hinh Firebase

Nho vay website chay duoc ngay tu bay gio, va khi nhom tao xong Firebase thi
chi can dien mot dong vao .env, khong phai sua code.

Cau truc du lieu tren Firebase (dung nhu muc 4.9 bao cao):

  smartdrain/current      trang thai moi nhat
  smartdrain/history      cac ban ghi muc nuoc theo thoi gian
  smartdrain/events       nhat ky bat/tat bom, doi che do, canh bao
"""

import json
import os
import sqlite3
import threading
import time

import config

try:
    import requests
except ImportError:  # pragma: no cover
    requests = None


def now_ms() -> int:
    """Moc thoi gian tinh bang mili giay. ESP32 khong co dong ho that nen moc
    thoi gian cua moi ban ghi deu lay tu may chay website."""
    return int(time.time() * 1000)


# ---------------------------------------------------------------------------
# SQLite
# ---------------------------------------------------------------------------


class SqliteStore:
    """Luu tru cuc bo. Dung khi chua cau hinh Firebase, va cung lam ban sao du
    phong de trang lich su van xem duoc khi mat mang."""

    name = "SQLite"

    def __init__(self, path):
        self.path = path
        self.lock = threading.Lock()

        folder = os.path.dirname(path)
        if folder:
            os.makedirs(folder, exist_ok=True)

        self._init_schema()

    def _connect(self):
        conn = sqlite3.connect(self.path, timeout=10)
        conn.row_factory = sqlite3.Row
        return conn

    def _init_schema(self):
        with self.lock, self._connect() as conn:
            conn.executescript(
                """
                CREATE TABLE IF NOT EXISTS history (
                    ts            INTEGER NOT NULL,
                    input_percent INTEGER,
                    output_percent INTEGER,
                    pump_state    TEXT,
                    mode          TEXT,
                    alert_code    TEXT
                );
                CREATE INDEX IF NOT EXISTS idx_history_ts ON history(ts);

                CREATE TABLE IF NOT EXISTS events (
                    ts       INTEGER NOT NULL,
                    type     TEXT NOT NULL,
                    message  TEXT,
                    detail   TEXT,
                    acked    INTEGER NOT NULL DEFAULT 0,
                    acked_ts INTEGER
                );
                CREATE INDEX IF NOT EXISTS idx_events_ts ON events(ts);

                CREATE TABLE IF NOT EXISTS current (
                    id      INTEGER PRIMARY KEY CHECK (id = 1),
                    payload TEXT NOT NULL
                );
                """
            )

    # -- ghi ---------------------------------------------------------------

    def save_current(self, state):
        with self.lock, self._connect() as conn:
            conn.execute(
                "INSERT INTO current (id, payload) VALUES (1, ?) "
                "ON CONFLICT(id) DO UPDATE SET payload = excluded.payload",
                (json.dumps(state, ensure_ascii=False),),
            )

    def save_history(self, sample):
        with self.lock, self._connect() as conn:
            conn.execute(
                "INSERT INTO history (ts, input_percent, output_percent, "
                "pump_state, mode, alert_code) VALUES (?, ?, ?, ?, ?, ?)",
                (
                    sample["ts"],
                    sample.get("input_percent"),
                    sample.get("output_percent"),
                    sample.get("pump_state"),
                    sample.get("mode"),
                    sample.get("alert_code"),
                ),
            )

    def save_event(self, event):
        with self.lock, self._connect() as conn:
            cursor = conn.execute(
                "INSERT INTO events (ts, type, message, detail) VALUES (?, ?, ?, ?)",
                (
                    event["ts"],
                    event["type"],
                    event.get("message", ""),
                    json.dumps(event.get("detail", {}), ensure_ascii=False),
                ),
            )
            return str(cursor.lastrowid)

    def ack_event(self, event_id):
        with self.lock, self._connect() as conn:
            cursor = conn.execute(
                "UPDATE events SET acked = 1, acked_ts = ? WHERE rowid = ? AND acked = 0",
                (now_ms(), event_id),
            )
            return cursor.rowcount > 0

    # -- doc ---------------------------------------------------------------

    def read_history(self, start_ms, end_ms, limit):
        with self.lock, self._connect() as conn:
            rows = conn.execute(
                "SELECT * FROM history WHERE ts BETWEEN ? AND ? "
                "ORDER BY ts DESC LIMIT ?",
                (start_ms, end_ms, limit),
            ).fetchall()

        return [dict(row) for row in reversed(rows)]

    def read_events(self, start_ms, end_ms, limit):
        with self.lock, self._connect() as conn:
            rows = conn.execute(
                "SELECT rowid AS id, * FROM events WHERE ts BETWEEN ? AND ? "
                "ORDER BY ts DESC LIMIT ?",
                (start_ms, end_ms, limit),
            ).fetchall()

        result = []
        for row in rows:
            item = dict(row)
            item["id"] = str(item["id"])
            item["acked"] = bool(item["acked"])
            try:
                item["detail"] = json.loads(item.get("detail") or "{}")
            except (ValueError, TypeError):
                item["detail"] = {}
            result.append(item)
        return result

    def purge_older_than(self, cutoff_ms):
        with self.lock, self._connect() as conn:
            conn.execute("DELETE FROM history WHERE ts < ?", (cutoff_ms,))


# ---------------------------------------------------------------------------
# Firebase Realtime Database (qua REST, khong can thu vien firebase-admin)
# ---------------------------------------------------------------------------


class FirebaseStore:
    """Ghi va doc Firebase Realtime Database bang REST API.

    Co y dung REST thay vi firebase-admin: khong can file service account JSON,
    chi can URL database, nen cai dat nhanh hon nhieu cho do an.

    Moi loi mang deu duoc nuot va ghi log. Website khong duoc chet chi vi
    Firebase khong voi toi - dieu khien bom quan trong hon luu lich su.
    """

    name = "Firebase"

    def __init__(self, db_url, auth_token=""):
        if requests is None:
            raise RuntimeError("Thieu thu vien requests, chay: pip install requests")

        self.base = db_url.rstrip("/")
        self.auth = auth_token
        self.root = "smartdrain"
        self.last_error = None

    def _url(self, path):
        url = f"{self.base}/{self.root}/{path}.json"
        if self.auth:
            url += f"?auth={self.auth}"
        return url

    def _request(self, method, path, payload=None, params=None):
        url = self._url(path)
        if params:
            separator = "&" if "?" in url else "?"
            url += separator + "&".join(f"{k}={v}" for k, v in params.items())

        try:
            response = requests.request(method, url, json=payload, timeout=8)
            response.raise_for_status()
            self.last_error = None
            return response.json()
        except Exception as exc:  # pragma: no cover - phu thuoc mang
            self.last_error = str(exc)
            print(f"[FIREBASE] Loi {method} {path}: {exc}", flush=True)
            return None

    # -- ghi ---------------------------------------------------------------

    def save_current(self, state):
        self._request("PUT", "current", state)

    def save_history(self, sample):
        self._request("POST", "history", sample)

    def save_event(self, event):
        result = self._request("POST", "events", event)
        return (result or {}).get("name")

    def ack_event(self, event_id):
        payload = {"acked": True, "acked_ts": now_ms()}
        return self._request("PATCH", f"events/{event_id}", payload) is not None

    # -- doc ---------------------------------------------------------------

    def read_history(self, start_ms, end_ms, limit):
        # Sap xep theo $key: khoa do Firebase sinh ra da tang dan theo thoi gian
        # nen khong can khai bao .indexOn trong rule.
        data = self._request(
            "GET", "history", params={"orderBy": '"$key"', "limitToLast": limit}
        )
        return self._filter_by_time(data, start_ms, end_ms)

    def read_events(self, start_ms, end_ms, limit):
        data = self._request(
            "GET", "events", params={"orderBy": '"$key"', "limitToLast": limit}
        )
        events = self._filter_by_time(data, start_ms, end_ms)
        for event in events:
            event["acked"] = bool(event.get("acked"))
        return list(reversed(events))

    @staticmethod
    def _filter_by_time(data, start_ms, end_ms):
        if not isinstance(data, dict):
            return []

        items = []
        for key, value in data.items():
            if not isinstance(value, dict):
                continue
            ts = value.get("ts", 0)
            if start_ms <= ts <= end_ms:
                item = dict(value)
                item["id"] = key
                items.append(item)

        items.sort(key=lambda item: item.get("ts", 0))
        return items

    def purge_older_than(self, cutoff_ms):
        # Firebase mien phi du cho do an, khong can tu don dep.
        pass


# ---------------------------------------------------------------------------

def create_store():
    """Chon backend theo cau hinh. Uu tien Firebase neu da khai bao."""
    if config.FIREBASE_DB_URL:
        try:
            store = FirebaseStore(config.FIREBASE_DB_URL, config.FIREBASE_AUTH)
            print(f"[STORE] Dung Firebase: {config.FIREBASE_DB_URL}", flush=True)
            return store
        except Exception as exc:
            print(f"[STORE] Khong dung duoc Firebase ({exc}), chuyen sang SQLite", flush=True)

    print(f"[STORE] Dung SQLite: {config.SQLITE_PATH}", flush=True)
    return SqliteStore(config.SQLITE_PATH)
