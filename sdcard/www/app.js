function log(x){ document.getElementById('log').textContent = x; }

async function capLocal(){
  const framesize = document.getElementById('framesize').value;
  const pixformat = document.getElementById('pixformat').value;
  const id = "local_" + Date.now();
  const r = await fetch("/api/capture_local", {
    method:"POST",
    headers:{"Content-Type":"application/json"},
    body: JSON.stringify({id, framesize, pixformat})
  });
  log(await r.text());
}

async function capSync(){
  const framesize = document.getElementById('framesize').value;
  const pixformat = document.getElementById('pixformat').value;
  const r = await fetch("/api/capture_sync", {
    method:"POST",
    headers:{"Content-Type":"application/json"},
    body: JSON.stringify({framesize, pixformat})
  });
  log(await r.text());
}

async function presetSave(){
  const name = document.getElementById('presetName').value.trim();
  const r = await fetch("/api/registers/preset/save", {
    method:"POST", headers:{"Content-Type":"application/json"},
    body: JSON.stringify({name})
  });
  log(await r.text());
}

async function presetLoad(){
  const name = document.getElementById('presetName').value.trim();
  const r = await fetch("/api/registers/preset/load", {
    method:"POST", headers:{"Content-Type":"application/json"},
    body: JSON.stringify({name})
  });
  log(await r.text());
}

async function presetApply(){
  const name = document.getElementById('presetName').value.trim();
  const r = await fetch("/api/registers/apply_preset", {
    method:"POST", headers:{"Content-Type":"application/json"},
    body: JSON.stringify({name})
  });
  log(await r.text());
}
