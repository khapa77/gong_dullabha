$(document).ready(function() {
  // --- Состояние ---
  let currentAlarms = [];
  let editingAlarmId = null;

  // --- Время ---
  function updateTime() {
    $.get('/api/time', function(data) {
      $('#current-time').text('Текущее время: ' + (data.time || '—'));
    });
  }
  setInterval(updateTime, 1000);
  updateTime();

  // --- Утилиты ---
  const dayLabels = ['Пн','Вт','Ср','Чт','Пт','Сб','Вс'];
  function formatDays(days) {
    if (!Array.isArray(days)) return '';
    return days.map(d => dayLabels[d] || d).join(', ');
  }

  function getFormData() {
    const time = $('#alarm-time').val();
    const duration = parseInt($('#alarm-duration').val(), 10) || 0;
    const active = $('#alarm-active').is(':checked');
    const days = [];
    $('#alarm-days input[type="checkbox"]').each(function() {
      if ($(this).is(':checked')) days.push(parseInt($(this).val(), 10));
    });
    return { time, duration, active, days };
  }

  function resetForm() {
    editingAlarmId = null;
    $('#alarm-form')[0].reset();
    $('#alarm-days input[type="checkbox"]').prop('checked', false);
    $('#alarmModalLabel').text('Будильник');
  }

  // --- Список будильников ---
  function loadAlarms() {
    $.get('/api/alarms', function(data) {
      currentAlarms = data.alarms || [];
      let html = '<ul class="list-group">';
      currentAlarms.forEach(function(alarm) {
        html += `<li class="list-group-item d-flex justify-content-between align-items-center">
          <span>${alarm.time} (${formatDays(alarm.days)}) — ${alarm.duration}s ${alarm.active ? '' : '<span class=\'text-danger\'>(выкл)</span>'}</span>
          <div>
            <button class="btn btn-sm btn-secondary me-2 edit-alarm" data-id="${alarm.id}" data-bs-toggle="modal" data-bs-target="#alarmModal">✏️</button>
            <button class="btn btn-sm btn-danger delete-alarm" data-id="${alarm.id}">🗑️</button>
          </div>
        </li>`;
      });
      html += '</ul>';
      $('#alarms-list').html(html);
    });
  }
  loadAlarms();

  // --- Форма добавления/редактирования ---
  // Инициализация чекбоксов дней
  dayLabels.forEach((d, i) => {
    $('#alarm-days').append(`<label class="form-check-label me-2"><input type="checkbox" class="form-check-input" value="${i}"> ${d}</label>`);
  });

  $('#alarmModal').on('hidden.bs.modal', resetForm);

  $('#alarm-form').on('submit', function(e) {
    e.preventDefault();
    const payload = getFormData();
    if (!payload.time || !payload.duration) return;

    const ajaxOpts = editingAlarmId
      ? { url: `/api/alarms/${editingAlarmId}`, method: 'PUT' }
      : { url: '/api/alarms', method: 'POST' };

    $.ajax({
      ...ajaxOpts,
      data: JSON.stringify(payload),
      contentType: 'application/json',
      success: function() {
        $('#alarmModal').modal('hide');
        loadAlarms();
      },
      error: function(xhr) {
        alert('Ошибка сохранения: ' + (xhr.responseJSON && xhr.responseJSON.error || xhr.status));
      }
    });
  });

  $('#alarms-list').on('click', '.edit-alarm', function() {
    const id = parseInt($(this).data('id'), 10);
    const alarm = currentAlarms.find(a => a.id === id);
    if (!alarm) return;
    editingAlarmId = id;
    $('#alarmModalLabel').text('Редактировать будильник');
    $('#alarm-time').val(alarm.time);
    $('#alarm-duration').val(alarm.duration);
    $('#alarm-active').prop('checked', !!alarm.active);
    $('#alarm-days input[type="checkbox"]').each(function() {
      const val = parseInt($(this).val(), 10);
      $(this).prop('checked', Array.isArray(alarm.days) && alarm.days.includes(val));
    });
  });

  $('#alarms-list').on('click', '.delete-alarm', function() {
    const id = parseInt($(this).data('id'), 10);
    if (!confirm('Удалить будильник #' + id + '?')) return;
    $.ajax({
      url: `/api/alarms/${id}`,
      method: 'DELETE',
      success: function() {
        loadAlarms();
      },
      error: function(xhr) {
        alert('Ошибка удаления: ' + (xhr.responseJSON && xhr.responseJSON.error || xhr.status));
      }
    });
  });

  // --- Управление DFPlayer Mini ---
  $('#audio-play').on('click', function() {
    $.post('/api/audio/play').done(function() {
      $('#audio-status').text('Воспроизведение запущено');
    }).fail(function() {
      $('#audio-status').text('Ошибка запуска воспроизведения');
    });
  });
  $('#audio-stop').on('click', function() {
    $.post('/api/audio/stop').done(function() {
      $('#audio-status').text('Воспроизведение остановлено');
    }).fail(function() {
      $('#audio-status').text('Ошибка остановки');
    });
  });
  $('#audio-track-form').on('submit', function(e) {
    e.preventDefault();
    const num = $('#audio-track-num').val();
    if (!num) return;
    $.post('/api/audio/track?num=' + num).done(function() {
      $('#audio-status').text('Воспроизведение трека #' + num);
    }).fail(function() {
      $('#audio-status').text('Ошибка воспроизведения трека');
    });
  });
  $('#audio-volume-form').on('submit', function(e) {
    e.preventDefault();
    const val = $('#audio-volume-value').val();
    if (val === '') return;
    $.post('/api/audio/volume?value=' + val).done(function() {
      $('#audio-status').text('Громкость установлена: ' + val);
    }).fail(function() {
      $('#audio-status').text('Ошибка установки громкости');
    });
  });
});