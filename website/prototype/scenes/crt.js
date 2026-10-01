// PROTOTYPE (#440). Scene "CRT": the road on a cathode-ray tube. Drawn soft at
// a low resolution and scaled up, with scanlines, a phosphor bloom, darker
// corners, a refresh band rolling down the glass and a faint flicker. The
// flicker moves the brightness by a few percent at most, never a flash, and
// both it and the band hold still for reduced motion.

(function () {
  "use strict";

  var D = window.OmawebDisplay;
  var R = window.OmawebRoad;
  var DOWN = 3; // logical pixels per texel of the picture
  var BLOOM = 12; // logical pixels per texel of the bloom

  function canvas(state, key, w, h) {
    var c = state[key];
    if (!c || c.width !== w || c.height !== h) {
      c = state[key] = document.createElement("canvas");
      c.width = w;
      c.height = h;
    }
    return c;
  }

  // One scanline period, as a pattern: a dark line every third device row.
  function scanlines(ctx, state, scale) {
    if (!state.lines || state.linesScale !== scale) {
      var period = Math.max(2, Math.round(3 * scale));
      var tile = document.createElement("canvas");
      tile.width = 1;
      tile.height = period;
      var t = tile.getContext("2d");
      t.fillStyle = "rgba(0,0,0,0.38)";
      t.fillRect(0, period - Math.max(1, Math.round(scale)), 1, Math.max(1, Math.round(scale)));
      state.lines = ctx.createPattern(tile, "repeat");
      state.linesScale = scale;
    }
    return state.lines;
  }

  // The glass over a picture: phosphor bloom, scanlines, the refresh band,
  // the flicker and the darker corners. `pic` is drawn to fill the display,
  // smoothed or, for a pixel picture, nearest-neighbour.
  function glass(ctx, input, s, c, pic, smooth) {
    var scale = 1 / input.pitch;
    var glow = canvas(
      s,
      "glow",
      Math.max(1, Math.ceil(input.width / scale / BLOOM)),
      Math.max(1, Math.ceil(input.height / scale / BLOOM)),
    );
    var gctx = glow.getContext("2d");
    gctx.imageSmoothingEnabled = true;
    gctx.drawImage(pic, 0, 0, glow.width, glow.height);

    ctx.setTransform(1, 0, 0, 1, 0, 0);
    ctx.globalCompositeOperation = "source-over";
    ctx.globalAlpha = 1;
    ctx.imageSmoothingEnabled = smooth;
    ctx.drawImage(pic, 0, 0, input.width, input.height);
    ctx.imageSmoothingEnabled = true;

    ctx.globalCompositeOperation = "lighter";
    ctx.globalAlpha = 0.35;
    ctx.drawImage(glow, 0, 0, input.width, input.height);
    ctx.globalAlpha = 1;
    ctx.globalCompositeOperation = "source-over";

    ctx.fillStyle = scanlines(ctx, s, scale);
    ctx.fillRect(0, 0, input.width, input.height);

    if (!input.reducedMotion) {
      var y = ((input.time / 7) % 1) * (input.height * 1.4) - input.height * 0.2;
      var band = ctx.createLinearGradient(0, y - 60 * scale, 0, y + 60 * scale);
      band.addColorStop(0, "rgba(255,255,255,0)");
      band.addColorStop(0.5, D.css(c.light, 0.045));
      band.addColorStop(1, "rgba(255,255,255,0)");
      ctx.fillStyle = band;
      ctx.fillRect(0, y - 60 * scale, input.width, 120 * scale);
      var f = 0.02 + 0.025 * Math.abs(Math.sin(input.time * 37.1) * Math.sin(input.time * 11.3));
      ctx.fillStyle = "rgba(0,0,0," + f.toFixed(3) + ")";
      ctx.fillRect(0, 0, input.width, input.height);
    }

    var cx = input.width / 2;
    var cy = input.height / 2;
    var v = ctx.createRadialGradient(cx, cy, Math.min(cx, cy) * 0.6, cx, cy, Math.hypot(cx, cy));
    v.addColorStop(0, "rgba(0,0,0,0)");
    v.addColorStop(1, "rgba(0,0,0,0.55)");
    ctx.fillStyle = v;
    ctx.fillRect(0, 0, input.width, input.height);
  }

  window.OmawebScenes.register({
    id: "crt",
    name: "CRT",
    pitch: "device",
    draw: function (ctx, input) {
      var scale = 1 / input.pitch;
      var w = input.width / scale;
      var h = input.height / scale;
      var c = R.colours(input);
      var m = R.motion(input);
      var s = input.state;
      var pic = canvas(s, "pic", Math.ceil(w / DOWN), Math.ceil(h / DOWN));
      var p = pic.getContext("2d");
      p.setTransform(1 / DOWN, 0, 0, 1 / DOWN, 0, 0);
      R.fill(p, R.geometry(pic.width * DOWN, pic.height * DOWN), c, m, { weight: 1.4 });
      glass(ctx, input, s, c, pic, true);
    },
  });

  // The glass, for the Scenes that show their own picture through it.
  window.OmawebCRT = { glass: glass, canvas: canvas };
})();
