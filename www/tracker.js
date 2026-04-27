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

function colorTag(text) {
  return text
    .replace(/(\[CONNECT\])/g,    '<span class="tag-connect">$1</span>')
    .replace(/(\[DISCONNECT\])/g, '<span class="tag-disconnect">$1</span>')
    .replace(/(\[ERROR\])/g,      '<span class="tag-error">$1</span>')
    .replace(/(\[TRACKER\])/g,    '<span class="tag-tracker">$1</span>');
}

function renderLog(entries) {
  const box = document.getElementById('log-box');
  if (!entries || !entries.length) {
    box.innerHTML = '<p class="empty">No events yet.</p>';
    return;
  }
  /* reverse so newest entry is at the top */
  box.innerHTML = [...entries].reverse()
    .map(e => `<div class="log-line">${colorTag(e)}</div>`)
    .join('');
}

function render(d) {
  document.getElementById('peer-count').textContent = d.peers.length;

  const table = document.getElementById('peer-table');
  const empty = document.getElementById('empty-msg');

  if (!d.peers.length) {
    table.hidden = true;
    empty.hidden = false;
    return;
  }

  table.hidden = false;
  empty.hidden = true;

  renderLog(d.log);
  document.getElementById('peer-rows').innerHTML = d.peers.map(p => {
    const cls = p.status === 'both'     ? 'both'
              : p.status === 'seeding'  ? 'seed'
              : p.status === 'leeching' ? 'leech'
              : 'standby';
    return `<tr>
      <td class="addr">${p.ip}:${p.port}</td>
      <td><span class="badge ${cls}">${p.status}</span></td>
      <td>${p.seeding}</td>
      <td>${p.leeching}</td>
    </tr>`;
  }).join('');
}

poll();
setInterval(poll, 2000);
