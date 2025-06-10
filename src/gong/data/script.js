// Global variables
let alarms = [];
let templates = [];

// Update current time
function updateTime() {
    fetch('/api/time')
        .then(response => response.json())
        .then(data => {
            document.getElementById('current-time').textContent = data.time;
            document.getElementById('current-date').textContent = data.date;
        });
}

// Load alarms
function loadAlarms() {
    fetch('/api/alarms')
        .then(response => response.json())
        .then(data => {
            alarms = data;
            updateAlarmList();
        });
}

// Update alarm list in UI
function updateAlarmList() {
    const alarmList = document.getElementById('alarm-list');
    alarmList.innerHTML = '';
    
    alarms.forEach((alarm, index) => {
        const item = document.createElement('div');
        item.className = 'list-group-item';
        item.innerHTML = `
            <div>
                <div class="alarm-time">${alarm.time}</div>
                <div class="alarm-days">${formatDays(alarm.days)}</div>
            </div>
            <div class="btn-group">
                <button class="btn btn-sm btn-outline-primary" onclick="editAlarm(${index})">Редактировать</button>
                <button class="btn btn-sm btn-outline-danger" onclick="deleteAlarm(${index})">Удалить</button>
            </div>
        `;
        alarmList.appendChild(item);
    });
}

// Format days array to string
function formatDays(days) {
    const dayNames = ['Пн', 'Вт', 'Ср', 'Чт', 'Пт', 'Сб', 'Вс'];
    return days.map(day => dayNames[day - 1]).join(', ');
}

// Save alarm
function saveAlarm(alarm) {
    fetch('/api/alarms', {
        method: 'POST',
        headers: {
            'Content-Type': 'application/json'
        },
        body: JSON.stringify(alarm)
    })
    .then(response => response.json())
    .then(data => {
        loadAlarms();
        $('#alarmModal').modal('hide');
    });
}

// Delete alarm
function deleteAlarm(index) {
    if (confirm('Вы уверены, что хотите удалить этот будильник?')) {
        fetch(`/api/alarms/${index}`, {
            method: 'DELETE'
        })
        .then(() => loadAlarms());
    }
}

// Manual trigger
document.getElementById('manual-trigger').addEventListener('click', () => {
    const duration = document.getElementById('duration').value;
    fetch('/api/trigger', {
        method: 'POST',
        headers: {
            'Content-Type': 'application/json'
        },
        body: JSON.stringify({ duration })
    });
});

// Save duration
document.getElementById('duration').addEventListener('change', (e) => {
    fetch('/api/settings', {
        method: 'POST',
        headers: {
            'Content-Type': 'application/json'
        },
        body: JSON.stringify({ duration: e.target.value })
    });
});

// Initialize
document.addEventListener('DOMContentLoaded', () => {
    // Update time every second
    updateTime();
    setInterval(updateTime, 1000);
    
    // Load initial data
    loadAlarms();
    
    // Setup modal events
    document.getElementById('add-alarm').addEventListener('click', () => {
        document.getElementById('alarm-form').reset();
        $('#alarmModal').modal('show');
    });
    
    document.getElementById('save-alarm').addEventListener('click', () => {
        const time = document.getElementById('alarm-time').value;
        const days = Array.from(document.querySelectorAll('input[type="checkbox"]:checked'))
            .map(cb => parseInt(cb.value));
        
        if (time && days.length > 0) {
            saveAlarm({ time, days });
        }
    });
}); 