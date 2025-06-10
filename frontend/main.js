$(document).ready(function() {
  // Отображение текущего времени
  function updateTime() {
    $.get('/api/time', function(data) {
      $('#current-time').text('Текущее время: ' + (data.time || '—'));
    });
  }
  setInterval(updateTime, 1000);
  updateTime();

  // Загрузка и отображение списка будильников
  function loadAlarms() {
    $.get('/api/alarms', function(data) {
      let html = '<ul class="list-group">';
      (data.alarms || []).forEach(function(alarm) {
        html += `<li class="list-group-item d-flex justify-content-between align-items-center">
          <span>${alarm.time} (${alarm.days.join(', ')}) — ${alarm.duration}s ${alarm.active ? '' : '<span class=\'text-danger\'>(выкл)</span>'}</span>
          <div>
            <button class="btn btn-sm btn-secondary me-2 edit-alarm" data-id="${alarm.id}">✏️</button>
            <button class="btn btn-sm btn-danger delete-alarm" data-id="${alarm.id}">🗑️</button>
          </div>
        </li>`;
      });
      html += '</ul>';
      $('#alarms-list').html(html);
    });
  }
  loadAlarms();

  // Добавление/редактирование будильника (заглушка)
  $('#alarm-form').on('submit', function(e) {
    e.preventDefault();
    // TODO: реализовать отправку данных на сервер
    $('#alarmModal').modal('hide');
    loadAlarms();
  });

  // Кнопки редактирования/удаления (заглушка)
  $('#alarms-list').on('click', '.edit-alarm', function() {
    // TODO: реализовать редактирование
    alert('Редактирование будильника пока не реализовано');
  });
  $('#alarms-list').on('click', '.delete-alarm', function() {
    // TODO: реализовать удаление
    alert('Удаление будильника пока не реализовано');
  });

  // Дни недели
  const days = ['Пн','Вт','Ср','Чт','Пт','Сб','Вс'];
  days.forEach((d, i) => {
    $('#alarm-days').append(`<label class="form-check-label me-2"><input type="checkbox" class="form-check-input" value="${i}"> ${d}</label>`);
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