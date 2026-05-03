function fmt(bytes) {
  if (bytes < 1024) return bytes + ' B';
  if (bytes < 1048576) return (bytes / 1024).toFixed(1) + ' KB';
  return (bytes / 1048576).toFixed(1) + ' MB';
}

function renderBitmap(bitmap, totalPieces, gotPieces) {
  if (totalPieces === 0) return '';

  const pct = Math.round(gotPieces / totalPieces * 100);
  const cls = totalPieces > 512 ? 'p-sm' : totalPieces > 128 ? 'p-md' : 'p-lg';

  const squares = bitmap.split('').map(b =>
    `<i class="p ${cls} ${b === '1' ? 'g' : 'm'}"></i>`
  ).join('');

  return `<div class="bm">
    <div class="bm-top">
      <span class="pct">${pct}%</span>
      <span class="bm-info">${gotPieces} / ${totalPieces} pieces downloaded</span>
    </div>
    <div class="prog"><div class="prog-fill" style="width:${pct}%"></div></div>
    <div class="pieces">${squares}</div>
  </div>`;
}

function seedCard(f) {
  const label = f.partial
    ? `${f.pieces} pieces available (partial)`
    : `${f.pieces} pieces`;
  return `<div class="file-card ${f.partial ? 'seed-partial' : 'seed'}">
    <div class="file-name">${f.filename}</div>
    <div class="file-meta">${fmt(f.size)} &bull; ${label} &bull; <code>${f.md5.slice(0, 8)}&hellip;</code></div>
  </div>`;
}

function leechCard(f) {
  return `<div class="file-card leech">
    <div class="file-name">${f.filename}</div>
    <div class="file-meta">${fmt(f.size)} &bull; <code>${f.md5.slice(0, 8)}&hellip;</code></div>
    ${renderBitmap(f.bitmap || '', f.totalPieces || 0, f.gotPieces || 0)}
  </div>`;
}

async function poll() {
  try {
    const r = await fetch('/api/state');
    if (!r.ok) throw new Error('HTTP ' + r.status);
    render(await r.json());
    document.getElementById('ts').textContent = new Date().toLocaleTimeString();
  } catch (e) {
    document.getElementById('ts').textContent = 'error';
  }
}

function render(d) {
  document.getElementById('self-addr').textContent = d.ip + ':' + d.port;
  document.getElementById('seed-count').textContent = d.seeding.length;
  document.getElementById('leech-count').textContent = d.leeching.length;

  document.getElementById('seeding').innerHTML = d.seeding.length
    ? d.seeding.map(seedCard).join('')
    : '<p class="empty">No files being seeded.</p>';

  document.getElementById('leeching').innerHTML = d.leeching.length
    ? d.leeching.map(leechCard).join('')
    : '<p class="empty">No files being downloaded.</p>';
}

poll();
setInterval(poll, 2000);

/* ── file search ─────────────────────────────────────────────────────────── */

const selectedPieces = {};

const seedInput = document.getElementById('seed-input');
const seedBtn = document.getElementById('seed-btn');
const seedStatus = document.getElementById('seed-status');

async function addFileToPeer() {
  const file = seedInput.files && seedInput.files[0];
  if (!file) {
    seedStatus.textContent = 'Choose a file first.';
    return;
  }

  const url = '/api/seed?name=' + encodeURIComponent(file.name);

  seedBtn.disabled = true;
  seedBtn.textContent = 'Uploading…';
  seedStatus.textContent = 'Uploading ' + file.name + '…';

  try {
    const r = await fetch(url, { method: 'POST', body: file });
    const d = await r.json();
    if (d.status === 'started') {
      seedStatus.textContent = 'Added ' + d.filename + ' (' + d.md5.slice(0, 8) + '…)';
      seedInput.value = '';
      await poll();
    } else {
      seedStatus.textContent = 'Upload failed.';
    }
  } catch (e) {
    seedStatus.textContent = 'Upload failed.';
  } finally {
    seedBtn.disabled = false;
    seedBtn.textContent = 'Add File to Peer';
  }
}

async function searchFiles() {
  const q = document.getElementById('search-input').value.trim();
  if (!q) return;
  const btn = document.getElementById('search-btn');
  btn.disabled = true;
  btn.textContent = '…';
  try {
    const r = await fetch('/api/look?q=' + encodeURIComponent(q));
    const d = await r.json();
    renderSearchResults(d.files || []);
  } catch (e) {
    document.getElementById('search-results').innerHTML =
      '<p class="empty">Search failed.</p>';
  } finally {
    btn.disabled = false;
    btn.textContent = 'Search';
  }
}

function renderSearchResults(files) {
  const el = document.getElementById('search-results');
  if (!files.length) {
    el.innerHTML = '<p class="empty">No files found.</p>';
    return;
  }
  el.innerHTML = files.map(f => `
    <div class="result-card">
      <div class="result-top">
        <span class="result-name">${f.filename}</span>
        <span class="result-meta">${fmt(f.size)} &bull; ${f.pieces} pieces &bull; <code>${f.md5.slice(0,8)}&hellip;</code></span>
        <button class="btn" onclick="getPeers('${f.md5}', this)">Get Peers</button>
      </div>
      <div class="peer-list" id="peers-${f.md5}"></div>
    </div>`
  ).join('');
}

async function getPeers(key, btn) {
  btn.disabled = true;
  btn.textContent = '…';
  try {
    const r = await fetch('/api/getfile?key=' + key);
    const d = await r.json();
    const el = document.getElementById('peers-' + key);
    if (!d.peers || !d.peers.length) {
      el.innerHTML = '<p class="empty">No peers found for this file.</p>';
    } else {
      el.innerHTML = d.peers.map(p => `
        <div class="peer-row-dl">
          <span class="peer-addr-sm">${p.ip}:${p.port}</span>
          <button class="btn btn-dl" onclick="download('${key}', ${p.port}, this)">Download All</button>
          <button class="btn btn-pick" onclick="getPiecesMode('${key}', ${p.port}, this)">Pick Pieces</button>
        </div>`
      ).join('');
    }
  } catch (e) {
    document.getElementById('peers-' + key).innerHTML =
      '<p class="empty">Could not reach tracker.</p>';
  } finally {
    btn.disabled = false;
    btn.textContent = 'Refresh Peers';
  }
}

async function download(key, port, btn) {
  btn.disabled = true;
  btn.textContent = 'Downloading…';
  try {
    await fetch('/api/download?key=' + key + '&port=' + port, { method: 'POST' });
    btn.textContent = 'Started ✓';
    btn.className = 'btn btn-done';
  } catch (e) {
    btn.disabled = false;
    btn.textContent = 'Retry';
  }
}

document.getElementById('search-input').addEventListener('keydown', e => {
  if (e.key === 'Enter') searchFiles();
});
document.getElementById('search-btn').addEventListener('click', searchFiles);
seedBtn.addEventListener('click', addFileToPeer);

/* ── piece picker ─────────────────────────────────────────────────────────── */

async function getPiecesMode(key, port, btn) {
  const row = btn.closest('.peer-row-dl');
  const existing = row.nextElementSibling;
  if (existing && existing.classList.contains('picker-panel')) {
    existing.remove();
    return;
  }
  btn.disabled = true;
  btn.textContent = '…';
  try {
    const r = await fetch('/api/interested?key=' + key + '&port=' + port);
    const d = await r.json();
    const bitmap = d.bitmap || '';
    if (!bitmap) { btn.textContent = 'No data'; return; }
    const panel = document.createElement('div');
    panel.className = 'picker-panel';
    panel.innerHTML = renderPicker(key, port, bitmap);
    row.insertAdjacentElement('afterend', panel);
    selectedPieces[key + ':' + port] = new Set();
  } catch (e) {
    btn.textContent = 'Error';
  } finally {
    btn.disabled = false;
    btn.textContent = 'Pick Pieces';
  }
}

function renderPicker(key, port, bitmap) {
  const total = bitmap.length;
  const cls = total > 512 ? 'sp-sm' : total > 128 ? 'sp-md' : 'sp-lg';
  const squares = bitmap.split('').map((b, i) =>
    `<i class="sel-p ${cls} ${b === '1' ? 'avail' : 'unavail'}" data-idx="${i}" onclick="togglePiece('${key}',${port},${i},this)"></i>`
  ).join('');
  return `
    <div class="picker-bar">
      <button class="btn btn-xs" onclick="selAll('${key}',${port})">All</button>
      <button class="btn btn-xs" onclick="selNone('${key}',${port})">None</button>
      <span class="picker-info" id="pi-${key}-${port}">0 selected</span>
      <button class="btn btn-xs btn-dl" id="dlbtn-${key}-${port}" onclick="dlSelected('${key}',${port})" disabled>Download 0</button>
    </div>
    <div class="piece-grid" id="pg-${key}-${port}">${squares}</div>`;
}

function togglePiece(key, port, idx, el) {
  const k = key + ':' + port;
  if (!selectedPieces[k]) selectedPieces[k] = new Set();
  if (selectedPieces[k].has(idx)) {
    selectedPieces[k].delete(idx);
    el.classList.remove('selected');
  } else {
    selectedPieces[k].add(idx);
    el.classList.add('selected');
  }
  updateDlBtn(key, port);
}

function selAll(key, port) {
  const k = key + ':' + port;
  if (!selectedPieces[k]) selectedPieces[k] = new Set();
  const grid = document.getElementById('pg-' + key + '-' + port);
  if (!grid) return;
  grid.querySelectorAll('.sel-p.avail').forEach(el => {
    selectedPieces[k].add(parseInt(el.dataset.idx));
    el.classList.add('selected');
  });
  updateDlBtn(key, port);
}

function selNone(key, port) {
  const k = key + ':' + port;
  selectedPieces[k] = new Set();
  const grid = document.getElementById('pg-' + key + '-' + port);
  if (grid) grid.querySelectorAll('.sel-p.selected').forEach(el => el.classList.remove('selected'));
  updateDlBtn(key, port);
}

function updateDlBtn(key, port) {
  const n = (selectedPieces[key + ':' + port] || new Set()).size;
  const info = document.getElementById('pi-' + key + '-' + port);
  const btn  = document.getElementById('dlbtn-' + key + '-' + port);
  if (info) info.textContent = n + ' selected';
  if (btn)  { btn.textContent = 'Download ' + n; btn.disabled = n === 0; }
}

async function dlSelected(key, port) {
  const indexes = [...(selectedPieces[key + ':' + port] || [])].sort((a, b) => a - b);
  if (!indexes.length) return;
  const btn = document.getElementById('dlbtn-' + key + '-' + port);
  if (btn) { btn.disabled = true; btn.textContent = 'Starting…'; }
  try {
    await fetch('/api/getpieces?key=' + key + '&port=' + port + '&indexes=' + indexes.join(','), { method: 'POST' });
    if (btn) { btn.textContent = 'Started ✓'; btn.className = 'btn btn-xs btn-done'; }
  } catch (e) {
    if (btn) { btn.disabled = false; btn.textContent = 'Download ' + indexes.length; }
  }
}
