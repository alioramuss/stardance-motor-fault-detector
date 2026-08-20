// Tiny static file server for the live dashboard.
// Run with:  node serve.js      then open http://localhost:8000/live.html
const http = require('http');
const fs   = require('fs');
const path = require('path');

const PORT = 8000;
const ROOT = __dirname;

const TYPES = {
  '.html': 'text/html; charset=utf-8',
  '.js'  : 'text/javascript; charset=utf-8',
  '.wasm': 'application/wasm',
  '.css' : 'text/css; charset=utf-8',
  '.json': 'application/json',
  '.md'  : 'text/plain; charset=utf-8',
};

http.createServer((req, res) => {
  let rel = decodeURIComponent(req.url.split('?')[0]);
  if (rel === '/') rel = '/live.html';

  const file = path.join(ROOT, path.normalize(rel).replace(/^([/\\])+/, ''));
  if (!file.startsWith(ROOT)) { res.writeHead(403).end('forbidden'); return; }

  fs.readFile(file, (err, data) => {
    if (err) { res.writeHead(404).end('not found: ' + rel); return; }
    res.writeHead(200, {
      'Content-Type': TYPES[path.extname(file).toLowerCase()] || 'application/octet-stream',
      'Cache-Control': 'no-store',
    });
    res.end(data);
  });
}).listen(PORT, () => {
  console.log('');
  console.log('  Dashboard running.  Open this in Chrome:');
  console.log('');
  console.log('      http://localhost:' + PORT + '/live.html');
  console.log('');
  console.log('  Press Ctrl+C here to stop it.');
  console.log('');
});
