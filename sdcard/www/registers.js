function out(x){ document.getElementById('out').textContent = x; }
function bank(){ return parseInt(document.getElementById('bank').value,10); }
function addr(){ return document.getElementById('addr').value; }
function val(){ return document.getElementById('val').value; }
function mask(){ return document.getElementById('mask').value.trim(); }

async function readReg(){
  const r = await fetch(`/api/registers/single?bank=${bank()}&addr=${encodeURIComponent(addr())}`);
  out(await r.text());
}

async function writeReg(){
  const body = { bank: bank(), addr: addr(), value: val() };
  const m = mask();
  if (m) body.mask = m;
  const r = await fetch(`/api/registers/single`, {
    method:"POST", headers:{"Content-Type":"application/json"},
    body: JSON.stringify(body)
  });
  out(await r.text());
}

function parseList(txt){
  return txt.split(",").map(s=>s.trim()).filter(Boolean).map(s=>{
    if (s.startsWith("0x")||s.startsWith("0X")) return parseInt(s,16);
    return parseInt(s,10);
  });
}

async function dumpAll(){
  const r = await fetch("/api/registers/dump");
  out(await r.text());
}

async function writeRangeLocal(){
  const b = bank();
  const start = document.getElementById("rangeStart").value.trim();
  const values = parseList(document.getElementById("rangeValues").value);
  const r = await fetch("/api/registers/range", {
    method:"POST", headers:{"Content-Type":"application/json"},
    body: JSON.stringify({bank:b, start, values})
  });
  out(await r.text());
}

async function writeRangeBoth(){
  const b = bank();
  const start = document.getElementById("rangeStart").value.trim();
  const values = parseList(document.getElementById("rangeValues").value);
  const r = await fetch("/api/registers/apply_range", {
    method:"POST", headers:{"Content-Type":"application/json"},
    body: JSON.stringify({bank:b, start, values})
  });
  out(await r.text());
}
