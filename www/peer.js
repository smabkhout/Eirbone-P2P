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
