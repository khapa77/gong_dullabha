import sqlite3
from flask import Flask, request, jsonify
from datetime import datetime
from flask_cors import CORS

app = Flask(__name__)
CORS(app)

DB = 'alarms.db'

def init_db():
    conn = sqlite3.connect(DB)
    c = conn.cursor()
    c.execute('''CREATE TABLE IF NOT EXISTS alarms (
        id INTEGER PRIMARY KEY AUTOINCREMENT,
        time TEXT NOT NULL,
        label TEXT,
        active INTEGER DEFAULT 1
    )''')
    c.execute('''CREATE TABLE IF NOT EXISTS settings (
        key TEXT PRIMARY KEY,
        value TEXT
    )''')
    conn.commit()
    conn.close()

@app.route('/api/alarms', methods=['GET'])
def get_alarms():
    conn = sqlite3.connect(DB)
    c = conn.cursor()
    c.execute('SELECT id, time, label, active FROM alarms')
    alarms = [
        {'id': row[0], 'time': row[1], 'label': row[2], 'active': bool(row[3])}
        for row in c.fetchall()
    ]
    conn.close()
    return jsonify(alarms)

@app.route('/api/alarms', methods=['POST'])
def add_alarm():
    data = request.json
    time = data.get('time')
    label = data.get('label', '')
    active = int(data.get('active', 1))
    conn = sqlite3.connect(DB)
    c = conn.cursor()
    c.execute('INSERT INTO alarms (time, label, active) VALUES (?, ?, ?)', (time, label, active))
    conn.commit()
    alarm_id = c.lastrowid
    conn.close()
    return jsonify({'id': alarm_id}), 201

@app.route('/api/alarms/<int:alarm_id>', methods=['PUT'])
def update_alarm(alarm_id):
    data = request.json
    time = data.get('time')
    label = data.get('label', '')
    active = int(data.get('active', 1))
    conn = sqlite3.connect(DB)
    c = conn.cursor()
    c.execute('UPDATE alarms SET time=?, label=?, active=? WHERE id=?', (time, label, active, alarm_id))
    conn.commit()
    conn.close()
    return jsonify({'status': 'ok'})

@app.route('/api/alarms/<int:alarm_id>', methods=['DELETE'])
def delete_alarm(alarm_id):
    conn = sqlite3.connect(DB)
    c = conn.cursor()
    c.execute('DELETE FROM alarms WHERE id=?', (alarm_id,))
    conn.commit()
    conn.close()
    return jsonify({'status': 'ok'})

@app.route('/api/settings', methods=['GET'])
def get_settings():
    conn = sqlite3.connect(DB)
    c = conn.cursor()
    c.execute('SELECT key, value FROM settings')
    settings = {row[0]: row[1] for row in c.fetchall()}
    conn.close()
    return jsonify(settings)

@app.route('/api/settings', methods=['POST'])
def set_settings():
    data = request.json
    conn = sqlite3.connect(DB)
    c = conn.cursor()
    for key, value in data.items():
        c.execute('REPLACE INTO settings (key, value) VALUES (?, ?)', (key, str(value)))
    conn.commit()
    conn.close()
    return jsonify({'status': 'ok'})

@app.route('/api/schedule')
def get_schedule():
    # Для простоты: возвращаем все будильники (можно доработать под дату)
    return get_alarms()

@app.route('/api/calendar')
def get_calendar():
    # Для простоты: возвращаем все будильники (можно доработать под месяц)
    return get_alarms()

@app.route('/api/time')
def get_time():
    now = datetime.now()
    return {
        'time': now.strftime('%H:%M:%S'),
        'date': now.strftime('%d.%m.%Y')
    }

if __name__ == '__main__':
    init_db()
    app.run(debug=True) 