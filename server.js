const http = require('http');
const fs = require('fs');
const path = require('path');
const url = require('url');

const PORT = process.env.PORT || 5000;
const EVENTS_FILE = path.join(__dirname, 'events.json');

function loadEvents() {
  return JSON.parse(fs.readFileSync(EVENTS_FILE, 'utf8'));
}

function isEventActive(event) {
  const now = new Date();
  // Compare using local date in YYYY-MM-DD
  const today = now.toISOString().slice(0, 10);
  return today >= event.start && today <= event.end;
}

function daysUntil(event) {
  const now = new Date();
  const today = now.toISOString().slice(0, 10);
  if (today > event.end) return null; // already passed
  const start = new Date(event.start + 'T00:00:00Z');
  const todayDate = new Date(today + 'T00:00:00Z');
  const diff = Math.ceil((start - todayDate) / (1000 * 60 * 60 * 24));
  return diff <= 0 ? 0 : diff;
}

function serveFile(res, filePath, contentType) {
  fs.readFile(filePath, (err, data) => {
    if (err) {
      res.writeHead(404);
      res.end('Not found');
      return;
    }
    res.writeHead(200, { 'Content-Type': contentType });
    res.end(data);
  });
}

const server = http.createServer((req, res) => {
  const parsed = url.parse(req.url, true);
  const pathname = parsed.pathname;

  // CORS headers for local network use
  res.setHeader('Access-Control-Allow-Origin', '*');

  // API endpoint
  if (pathname === '/api/check' || pathname.startsWith('/api/check')) {
    const data = loadEvents();
    const eventId = parsed.query.event || data.default_event;
    const event = data.events.find(e => e.id === eventId || e.slug === eventId);

    if (!event) {
      res.writeHead(404, { 'Content-Type': 'application/json' });
      res.end(JSON.stringify({ error: 'Event not found' }));
      return;
    }

    const active = isEventActive(event);
    const days = daysUntil(event);
    const today = new Date().toISOString().slice(0, 10);
    const passed = today > event.end;

    res.writeHead(200, { 'Content-Type': 'application/json' });
    res.end(JSON.stringify({
      event: event.name,
      active,
      days_until: days,
      passed,
      start: event.start,
      end: event.end,
      location: event.location,
      url: event.url,
      question: event.question,
      yes_message: event.yes_message,
      no_message: event.no_message
    }));
    return;
  }

  // API: list all events
  if (pathname === '/api/events') {
    const data = loadEvents();
    res.writeHead(200, { 'Content-Type': 'application/json' });
    res.end(JSON.stringify({ events: data.events, default_event: data.default_event }));
    return;
  }

  // Serve static files
  if (pathname === '/' || pathname === '/index.html') {
    serveFile(res, path.join(__dirname, 'public', 'index.html'), 'text/html; charset=utf-8');
    return;
  }

  if (pathname === '/style.css') {
    serveFile(res, path.join(__dirname, 'public', 'style.css'), 'text/css');
    return;
  }

  if (pathname === '/app.js') {
    serveFile(res, path.join(__dirname, 'public', 'app.js'), 'application/javascript');
    return;
  }

  res.writeHead(404);
  res.end('Not found');
});

server.listen(PORT, '0.0.0.0', () => {
  console.log(`isitdefcon server running on http://0.0.0.0:${PORT}`);
});
