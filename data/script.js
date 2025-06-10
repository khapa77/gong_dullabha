// Global variables
let currentSettings = {
    duration: 30,
    volume: 20,
    autoSync: true,
    timezone: 'UTC+3'
};

let alarms = [];
let templates = [];

// Initialize the application
$(document).ready(function() {
    loadAlarms();
    updateTime();
    setInterval(updateTime, 1000);

    // Event listeners
    $('#manual-trigger').click(triggerAlarm);
    $('#add-alarm').click(showAddAlarmModal);
    $('#add-template').click(showAddTemplateModal);
    $('#save-alarm').click(saveAlarm);
    $('#save-template').click(saveTemplate);
    $('#add-template-alarm').click(addTemplateAlarm);

    // Settings change handlers
    $('#duration, #volume').change(updateSettings);
    $('#autoSync').change(updateSettings);
    $('#timezone').change(updateSettings);

    // Открыть модальное окно добавления
    $('#add-alarm').click(function() {
        $('#alarm-form')[0].reset();
        $('#alarm-active').prop('checked', true);
        $('#alarmModal').modal('show');
    });

    // Сохранить будильник
    $('#save-alarm').click(function() {
        const time = $('#alarm-time').val();
        const label = $('#alarm-label').val();
        const active = $('#alarm-active').is(':checked') ? 1 : 0;
        if (!time || !label) {
            alert('Заполните все поля!');
            return;
        }
        fetch('http://localhost:5000/api/alarms', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ time, label, active })
        })
        .then(res => res.json())
        .then(() => {
            $('#alarmModal').modal('hide');
            loadAlarms();
        });
    });

    // Удаление будильника (делегирование)
    $('#today-list').on('click', '.delete-alarm', function() {
        const id = $(this).data('id');
        if (confirm('Удалить будильник?')) {
            fetch(`http://localhost:5000/api/alarms/${id}`, { method: 'DELETE' })
                .then(() => loadAlarms());
        }
    });
});

function loadAlarms() {
    fetch('http://localhost:5000/api/alarms')
        .then(res => res.json())
        .then(alarms => {
            const $list = $('#today-list');
            $list.empty();
            if (!alarms.length) {
                $list.append('<li class="text-muted">Нет будильников</li>');
                return;
            }
            alarms.forEach(alarm => {
                $list.append(`
                    <li class="${alarm.active ? 'active' : 'inactive'}">
                        <span class="status-dot"></span>
                        <span>${alarm.time} — ${alarm.label}</span>
                        <button class="btn btn-sm btn-outline-danger ms-2 delete-alarm" data-id="${alarm.id}">✕</button>
                    </li>
                `);
            });
        });
}

function updateTime() {
    fetch('http://localhost:5000/api/time')
        .then(res => res.json())
        .then(data => {
            $('#current-time').text(data.time);
            $('#current-date').text(data.date);
        });
}

// Trigger alarm manually
function triggerAlarm() {
    $.post('/api/trigger', {
        duration: currentSettings.duration,
        volume: currentSettings.volume
    });
}

// Show add alarm modal
function showAddAlarmModal() {
    $('#alarm-form')[0].reset();
    $('#alarm-active').prop('checked', true);
    $('#alarmModal').modal('show');
}

// Show add template modal
function showAddTemplateModal() {
    $('#template-form')[0].reset();
    $('#template-alarms').empty();
    $('#templateModal').modal('show');
}

// Save new alarm
function saveAlarm() {
    const time = $('#alarm-time').val();
    const label = $('#alarm-label').val();
    const active = $('#alarm-active').is(':checked') ? 1 : 0;
    if (!time || !label) {
        alert('Заполните все поля!');
        return;
    }
    fetch('http://localhost:5000/api/alarms', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ time, label, active })
    })
    .then(res => res.json())
    .then(() => {
        $('#alarmModal').modal('hide');
        loadAlarms();
    });
}

// Save new template
function saveTemplate() {
    const name = $('#template-name').val();
    const duration = parseInt($('#template-duration').val());
    const templateAlarms = [];

    $('#template-alarms .template-alarm').each(function() {
        const time = $(this).find('.alarm-time').val();
        const days = [];
        $(this).find('.day-checkbox:checked').each(function() {
            days.push(parseInt($(this).val()));
        });
        if (time && days.length > 0) {
            templateAlarms.push({ time, days });
        }
    });

    if (!name || templateAlarms.length === 0) {
        alert('Пожалуйста, укажите название шаблона и добавьте хотя бы один будильник');
        return;
    }

    $.post('/api/templates', {
        name: name,
        duration: duration,
        alarms: templateAlarms
    }, function() {
        $('#templateModal').modal('hide');
        loadTemplates();
    });
}

// Add alarm to template
function addTemplateAlarm() {
    const alarmHtml = `
        <div class="template-alarm border rounded p-3 mb-3">
            <div class="row">
                <div class="col-md-4">
                    <label class="form-label">Время</label>
                    <input type="time" class="form-control alarm-time" required>
                </div>
                <div class="col-md-7">
                    <label class="form-label">Дни недели</label>
                    <div class="d-flex flex-wrap">
                        ${[1,2,3,4,5,6,7].map(day => `
                            <div class="form-check me-3">
                                <input class="form-check-input day-checkbox" type="checkbox" value="${day}" id="template-day${day}">
                                <label class="form-check-label" for="template-day${day}">
                                    ${['Пн','Вт','Ср','Чт','Пт','Сб','Вс'][day-1]}
                                </label>
                            </div>
                        `).join('')}
                    </div>
                </div>
                <div class="col-md-1">
                    <button type="button" class="btn btn-danger btn-sm mt-4" onclick="$(this).closest('.template-alarm').remove()">
                        <i class="bi bi-trash"></i>
                    </button>
                </div>
            </div>
        </div>
    `;
    $('#template-alarms').append(alarmHtml);
}

// Update settings
function updateSettings() {
    currentSettings = {
        duration: parseInt($('#duration').val()),
        volume: parseInt($('#volume').val()),
        autoSync: $('#autoSync').is(':checked'),
        timezone: $('#timezone').val()
    };

    $.post('/api/settings', currentSettings);
}

// Update template list display
function updateTemplateList() {
    const templateList = $('#template-list');
    templateList.empty();

    templates.forEach((template, index) => {
        const alarmCount = template.alarms.length;
        const templateHtml = `
            <div class="list-group-item">
                <div class="d-flex justify-content-between align-items-center">
                    <div>
                        <h5 class="mb-1">${template.name}</h5>
                        <p class="mb-1 text-muted">
                            Продолжительность: ${template.duration} дней<br>
                            Количество будильников: ${alarmCount}
                        </p>
                    </div>
                    <div>
                        <button class="btn btn-primary btn-sm me-2" onclick="applyTemplate(${index})">
                            Применить
                        </button>
                        <button class="btn btn-danger btn-sm" onclick="deleteTemplate(${index})">
                            <i class="bi bi-trash"></i>
                        </button>
                    </div>
                </div>
            </div>
        `;
        templateList.append(templateHtml);
    });
}

// Delete alarm
function deleteAlarm(index) {
    if (confirm('Вы уверены, что хотите удалить этот будильник?')) {
        $.ajax({
            url: `/api/alarms/${index}`,
            method: 'DELETE',
            success: function() {
                loadAlarms();
            }
        });
    }
}

// Delete template
function deleteTemplate(index) {
    if (confirm('Вы уверены, что хотите удалить этот шаблон?')) {
        $.ajax({
            url: `/api/templates/${index}`,
            method: 'DELETE',
            success: function() {
                loadTemplates();
            }
        });
    }
}

// Apply template
function applyTemplate(index) {
    if (confirm('Применить этот шаблон? Существующие будильники будут удалены.')) {
        $.post(`/api/templates/${index}/apply`, function() {
            loadAlarms();
        });
    }
} 