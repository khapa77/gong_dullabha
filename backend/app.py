import os
import json
import sqlite3
from datetime import datetime

from flask import Flask, jsonify, request, send_from_directory
from flask_cors import CORS
import requests


APP_DIR = os.path.dirname(os.path.abspath(__file__))
REPO_ROOT = os.path.abspath(os.path.join(APP_DIR, os.pardir))
FRONTEND_DIR = os.path.join(REPO_ROOT, "frontend")
DB_PATH = os.path.join(REPO_ROOT, "alarms.db")

# Optionally proxy audio commands to ESP32 REST API (running on the device)
# Set ESP32_API_BASE env var to like "http://esp32.local" or "http://192.168.1.50"
ESP32_API_BASE = os.environ.get("ESP32_API_BASE", "")


def get_db_connection() -> sqlite3.Connection:
    conn = sqlite3.connect(DB_PATH)
    conn.row_factory = sqlite3.Row
    return conn


def init_db_if_needed() -> None:
    conn = get_db_connection()
    try:
        conn.execute(
            """
            CREATE TABLE IF NOT EXISTS alarms (
              id INTEGER PRIMARY KEY AUTOINCREMENT,
              time TEXT NOT NULL,               -- HH:MM
              days TEXT NOT NULL,               -- JSON array of ints [0..6] (0=Mon)
              duration INTEGER NOT NULL,        -- seconds
              active INTEGER NOT NULL DEFAULT 1 -- 0/1
            )
            """
        )
        conn.commit()
    finally:
        conn.close()


def serialize_alarm_row(row: sqlite3.Row) -> dict:
    return {
        "id": row["id"],
        "time": row["time"],
        "days": json.loads(row["days"]) if row["days"] else [],
        "duration": row["duration"],
        "active": bool(row["active"]),
    }


app = Flask(__name__, static_folder=None)
CORS(app)


@app.route("/")
def index_root():
    return send_from_directory(FRONTEND_DIR, "index.html")


@app.route("/<path:path>")
def serve_frontend(path: str):
    # Serve frontend assets like main.js
    return send_from_directory(FRONTEND_DIR, path)


@app.route("/api/time", methods=["GET"])
def api_time():
    now = datetime.now()
    return jsonify({
        "time": now.strftime("%H:%M:%S"),
        "iso": now.isoformat(),
    })


@app.route("/api/alarms", methods=["GET"])
def list_alarms():
    conn = get_db_connection()
    try:
        rows = conn.execute("SELECT * FROM alarms ORDER BY time ASC").fetchall()
        return jsonify({"alarms": [serialize_alarm_row(r) for r in rows]})
    finally:
        conn.close()


@app.route("/api/alarms", methods=["POST"])
def create_alarm():
    payload = request.get_json(silent=True) or {}
    time_str = (payload.get("time") or "").strip()
    days = payload.get("days") or []
    duration = int(payload.get("duration") or 0)
    active = 1 if payload.get("active") in (True, 1, "1", "true", "True") else 0

    if not time_str or duration <= 0:
        return jsonify({"error": "time and positive duration are required"}), 400

    days_json = json.dumps([int(d) for d in days])

    conn = get_db_connection()
    try:
        cur = conn.execute(
            "INSERT INTO alarms (time, days, duration, active) VALUES (?, ?, ?, ?)",
            (time_str, days_json, duration, active),
        )
        conn.commit()
        alarm_id = cur.lastrowid
        row = conn.execute("SELECT * FROM alarms WHERE id = ?", (alarm_id,)).fetchone()
        return jsonify(serialize_alarm_row(row)), 201
    finally:
        conn.close()


@app.route("/api/alarms/<int:alarm_id>", methods=["PUT", "PATCH"])
def update_alarm(alarm_id: int):
    payload = request.get_json(silent=True) or {}

    fields = []
    values = []

    if "time" in payload:
        fields.append("time = ?")
        values.append((payload.get("time") or "").strip())
    if "days" in payload:
        fields.append("days = ?")
        values.append(json.dumps([int(d) for d in (payload.get("days") or [])]))
    if "duration" in payload:
        fields.append("duration = ?")
        values.append(int(payload.get("duration") or 0))
    if "active" in payload:
        fields.append("active = ?")
        values.append(1 if payload.get("active") in (True, 1, "1", "true", "True") else 0)

    if not fields:
        return jsonify({"error": "no fields to update"}), 400

    values.append(alarm_id)

    conn = get_db_connection()
    try:
        conn.execute(f"UPDATE alarms SET {', '.join(fields)} WHERE id = ?", values)
        conn.commit()
        row = conn.execute("SELECT * FROM alarms WHERE id = ?", (alarm_id,)).fetchone()
        if not row:
            return jsonify({"error": "not found"}), 404
        return jsonify(serialize_alarm_row(row))
    finally:
        conn.close()


@app.route("/api/alarms/<int:alarm_id>", methods=["DELETE"])
def delete_alarm(alarm_id: int):
    conn = get_db_connection()
    try:
        cur = conn.execute("DELETE FROM alarms WHERE id = ?", (alarm_id,))
        conn.commit()
        if cur.rowcount == 0:
            return jsonify({"error": "not found"}), 404
        return jsonify({"status": "deleted", "id": alarm_id})
    finally:
        conn.close()


def proxy_to_esp32(path: str):
    if not ESP32_API_BASE:
        return jsonify({"error": "ESP32_API_BASE is not configured on server"}), 503

    target = ESP32_API_BASE.rstrip("/") + path
    try:
        # Forward as POST without body (frontend uses query params)
        resp = requests.post(target, timeout=5)
        return (resp.content, resp.status_code, {"Content-Type": resp.headers.get("Content-Type", "application/json")})
    except requests.RequestException as exc:
        return jsonify({"error": f"proxy failed: {exc}"}), 502


@app.route("/api/audio/play", methods=["POST"])  # proxy to ESP32
def audio_play():
    return proxy_to_esp32("/api/audio/play")


@app.route("/api/audio/stop", methods=["POST"])  # proxy to ESP32
def audio_stop():
    return proxy_to_esp32("/api/audio/stop")


@app.route("/api/audio/track", methods=["POST"])  # proxy to ESP32
def audio_track():
    # preserve query string
    qs = request.query_string.decode("utf-8")
    path = "/api/audio/track" + ("?" + qs if qs else "")
    return proxy_to_esp32(path)


@app.route("/api/audio/volume", methods=["POST"])  # proxy to ESP32
def audio_volume():
    qs = request.query_string.decode("utf-8")
    path = "/api/audio/volume" + ("?" + qs if qs else "")
    return proxy_to_esp32(path)


def main():
    init_db_if_needed()
    host = os.environ.get("HOST", "0.0.0.0")
    port = int(os.environ.get("PORT", "5001"))
    debug = bool(os.environ.get("DEBUG", "").lower() in ("1", "true", "yes"))
    app.run(host=host, port=port, debug=debug)


if __name__ == "__main__":
    main()

 
