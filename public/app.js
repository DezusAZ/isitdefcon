(function () {
  const card = document.getElementById('main-card');
  const questionEl = document.getElementById('question');
  const answerText = document.getElementById('answer-text');
  const detailEl = document.getElementById('detail');
  const countdownEl = document.getElementById('countdown');
  const metaEl = document.getElementById('event-meta');
  const selectEl = document.getElementById('event-select');

  // Parse ?event= from URL
  function getQueryEvent() {
    const params = new URLSearchParams(window.location.search);
    return params.get('event') || null;
  }

  function formatDate(iso) {
    const d = new Date(iso + 'T00:00:00Z');
    return d.toLocaleDateString('en-US', { month: 'long', day: 'numeric', year: 'numeric', timeZone: 'UTC' });
  }

  function render(data) {
    questionEl.textContent = data.question;
    document.title = data.question;

    // Remove state classes
    card.classList.remove('yes', 'no', 'passed');
    answerText.classList.remove('yes', 'no', 'passed', 'loading');

    if (data.active) {
      card.classList.add('yes');
      answerText.classList.add('yes');
      answerText.textContent = 'YES.';
      detailEl.textContent = data.yes_message;
      countdownEl.textContent = '';
    } else if (data.passed) {
      card.classList.add('passed');
      answerText.classList.add('passed');
      answerText.textContent = 'NO.';
      detailEl.textContent = data.no_message + ' (It already happened.)';
      countdownEl.textContent = '';
    } else {
      card.classList.add('no');
      answerText.classList.add('no');
      answerText.textContent = 'NO.';
      detailEl.textContent = data.no_message;

      if (data.days_until === 0) {
        countdownEl.textContent = 'It starts TODAY.';
      } else if (data.days_until === 1) {
        countdownEl.textContent = 'Tomorrow. One more day.';
      } else if (data.days_until !== null) {
        countdownEl.textContent = data.days_until + ' days away.';
      } else {
        countdownEl.textContent = '';
      }
    }

    // Event meta
    const startFmt = formatDate(data.start);
    const endFmt = formatDate(data.end);
    metaEl.innerHTML =
      data.event + ' &mdash; ' + startFmt + ' to ' + endFmt +
      '<br>' + data.location +
      (data.url ? ' &mdash; <a href="' + data.url + '" target="_blank">' + data.url.replace('https://', '') + '</a>' : '');
  }

  function setLoading() {
    card.classList.remove('yes', 'no', 'passed');
    answerText.classList.remove('yes', 'no', 'passed');
    answerText.classList.add('loading');
    answerText.textContent = '...';
    detailEl.textContent = '';
    countdownEl.textContent = '';
    metaEl.textContent = '';
  }

  function setError(msg) {
    answerText.classList.add('no');
    answerText.textContent = 'ERR';
    detailEl.textContent = msg || 'Could not load event data.';
    countdownEl.textContent = '';
    metaEl.textContent = '';
  }

  function loadEvent(eventId) {
    setLoading();
    const qs = eventId ? '?event=' + encodeURIComponent(eventId) : '';
    fetch('/api/check' + qs)
      .then(r => r.json())
      .then(data => {
        if (data.error) {
          setError(data.error);
        } else {
          render(data);
        }
      })
      .catch(() => setError('Server unreachable.'));
  }

  function loadEventList() {
    fetch('/api/events')
      .then(r => r.json())
      .then(data => {
        selectEl.innerHTML = '';
        data.events.forEach(ev => {
          const opt = document.createElement('option');
          opt.value = ev.id;
          opt.textContent = ev.name + ' (' + ev.start + ')';
          if (ev.id === data.default_event) opt.selected = true;
          selectEl.appendChild(opt);
        });
        // Override with URL param if present
        const qev = getQueryEvent();
        if (qev) selectEl.value = qev;
      })
      .catch(() => {
        selectEl.innerHTML = '<option value="">Error loading events</option>';
      });
  }

  selectEl.addEventListener('change', () => {
    const val = selectEl.value;
    // Update URL without reload
    const newUrl = val ? '?event=' + encodeURIComponent(val) : window.location.pathname;
    history.replaceState(null, '', newUrl);
    loadEvent(val);
  });

  // Init
  const initialEvent = getQueryEvent();
  loadEventList();
  loadEvent(initialEvent);

  // Auto-refresh every 60 seconds (useful for long running network display)
  setInterval(() => loadEvent(selectEl.value || null), 60000);
})();
