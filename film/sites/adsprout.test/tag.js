// The ad a magazine without blocking would show: loud, and in the reader's way.
const slot = document.currentScript.parentElement;
const creative = document.createElement("a");
creative.className = "adsprout";
creative.href = "http://adsprout.test/click";
creative.textContent = "MEGA SALE — 70% OFF EVERYTHING — TODAY ONLY";
creative.style.cssText =
  "display:grid;place-items:center;width:100%;height:100%;background:#ff00aa;color:#ffff00;" +
  "font:900 22px sans-serif;";
slot.replaceChildren(creative);
