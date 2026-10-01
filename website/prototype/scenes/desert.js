// PROTOTYPE (#440). Scene "CRT road": the night drive refined for the user and
// shown through the CRT's glass. A desert under a banded sun: the ground a
// smooth gradient lit by the sun's glow, the ridges filled silhouettes in
// layered tones, a dashed centre line on a wide road, now and then a saguaro,
// a rock or a lone sign passing at the roadside, and a rare shooting star.
// Painted in full colour at the Start page's 4 px pitch and scaled up
// nearest-neighbour, so each road pixel is a crisp block under the scanlines.

(function () {
  "use strict";

  var D = window.OmawebDisplay;
  var R = window.OmawebRoad;
  var CRT = window.OmawebCRT;
  var BLACK = [0, 0, 0];
  var WHITE = [255, 255, 255];
  var PIXEL = 4;

  // Half the road's width at the bottom edge, as a share of the width. Today's
  // road is 0.42.
  var ROADS = { wide: 0.62, wider: 0.8, widest: 1 };

  function fraction(v) {
    return v - Math.floor(v);
  }

  function hash(n) {
    return fraction(Math.sin(n * 91.345 + 7.13) * 47453.5453);
  }

  // The sun: a solid cap, then `bands` cuts that thicken toward the horizon.
  function sun(ctx, g, c, r, bands) {
    ctx.save();
    ctx.beginPath();
    var solid = r * 0.4;
    ctx.rect(g.vx - r, g.horizonY - r, r * 2, solid);
    var rest = r - solid;
    var unit = rest / (bands * 2);
    var y = g.horizonY - rest;
    for (var b = 0; b < bands; b++) {
      var gap = unit * (0.5 + b * (1 / bands));
      var stripe = unit * 2 - gap;
      ctx.rect(g.vx - r, y + gap, r * 2, stripe);
      y += unit * 2;
    }
    ctx.clip();
    var disc = ctx.createLinearGradient(0, g.horizonY - r, 0, g.horizonY);
    disc.addColorStop(0, D.css(c.sunTop));
    disc.addColorStop(1, D.css(c.sunLow));
    ctx.fillStyle = disc;
    ctx.beginPath();
    ctx.arc(g.vx, g.horizonY, r, 0, Math.PI * 2);
    ctx.fill();
    ctx.restore();
  }

  // Roadside silhouettes, each one drawn standing on the ground at (x, y) at
  // scale s: about how tall a saguaro is there.
  function saguaro(ctx, x, y, s) {
    var w = s * 0.13;
    ctx.fillRect(x - w / 2, y - s, w, s);
    ctx.fillRect(x - w * 2.2, y - s * 0.62, w * 1.7, w * 0.8);
    ctx.fillRect(x - w * 2.2, y - s * 0.86, w * 0.8, s * 0.3);
    ctx.fillRect(x + w * 0.5, y - s * 0.48, w * 1.5, w * 0.8);
    ctx.fillRect(x + w * 1.2, y - s * 0.74, w * 0.8, s * 0.3);
  }

  function rock(ctx, x, y, s) {
    ctx.beginPath();
    ctx.moveTo(x - s * 0.45, y);
    ctx.lineTo(x - s * 0.32, y - s * 0.22);
    ctx.lineTo(x - s * 0.05, y - s * 0.3);
    ctx.lineTo(x + s * 0.28, y - s * 0.18);
    ctx.lineTo(x + s * 0.42, y);
    ctx.closePath();
    ctx.fill();
  }

  function sign(ctx, x, y, s) {
    ctx.fillRect(x - s * 0.025, y - s * 0.7, s * 0.05, s * 0.7);
    ctx.fillRect(x - s * 0.22, y - s * 0.9, s * 0.44, s * 0.24);
  }

  var KINDS = [saguaro, rock, saguaro, sign, rock, saguaro];

  function draw(ctx, input) {
    var opts = input.options || {};
    var bands = Number(opts.bands) || 3;
    var g = R.geometry(input.width * PIXEL, input.height * PIXEL);
    g.halfWidth = g.w * (ROADS[opts.road] || ROADS.wide);
    var c = R.colours(input);
    var m = R.motion(input, 0.8);

    // The sky, with about half the stars and a few of them brighter.
    var sky = ctx.createLinearGradient(0, 0, 0, g.horizonY);
    sky.addColorStop(0, D.css(c.skyTop));
    sky.addColorStop(0.55, D.css(D.mix(c.skyTop, c.skyLow, 0.35)));
    sky.addColorStop(1, D.css(c.skyLow));
    ctx.fillStyle = sky;
    ctx.fillRect(0, 0, g.w, g.horizonY + 1);
    R.stars(120, function (across, high, bright) {
      var size = bright > 0.93 ? 3 : bright > 0.75 ? 1.8 : 1.2;
      ctx.fillStyle = D.css(c.light, bright > 0.93 ? 1 : (0.25 + 0.6 * bright) * (1 - high * 0.8));
      ctx.fillRect(across * g.w, high * g.horizonY * 0.9, size, size);
    });

    // A rare shooting star: one every fourteen seconds or so, for under a second.
    if (!input.reducedMotion) {
      var slot = Math.floor(input.time / 14);
      var into = input.time - slot * 14;
      if (into < 0.8 && hash(slot) > 0.35) {
        var sx = (0.15 + 0.7 * hash(slot + 1)) * g.w;
        var sy = (0.08 + 0.3 * hash(slot + 2)) * g.horizonY;
        var run = into / 0.8;
        var len = g.w * 0.12;
        var hx = sx + len * run;
        var hy = sy + len * 0.35 * run;
        var trail = ctx.createLinearGradient(hx - len * 0.5, hy - len * 0.175, hx, hy);
        trail.addColorStop(0, D.css(c.light, 0));
        trail.addColorStop(1, D.css(D.mix(c.light, WHITE, 0.5), 1 - run * 0.6));
        ctx.strokeStyle = trail;
        ctx.lineWidth = 3;
        ctx.beginPath();
        ctx.moveTo(hx - len * 0.5, hy - len * 0.175);
        ctx.lineTo(hx, hy);
        ctx.stroke();
      }
    }

    // The sunrise glow, then the sun with its bands.
    var halo = ctx.createRadialGradient(g.vx, g.horizonY, 0, g.vx, g.horizonY, g.w * 0.55);
    halo.addColorStop(0, D.css(c.sunLow, 0.6 + 0.3 * m.lit));
    halo.addColorStop(0.18, D.css(c.sunLow, 0.28));
    halo.addColorStop(0.45, D.css(c.glow, 0.1));
    halo.addColorStop(1, D.css(c.glow, 0));
    ctx.fillStyle = halo;
    ctx.fillRect(0, 0, g.w, g.horizonY);
    sun(ctx, g, c, g.h * 0.15, bands);

    // The ridges as filled silhouettes, far to near, each a tone darker.
    var tones = [
      D.mix(c.skyLow, c.skyTop, 0.2),
      D.mix(c.skyLow, c.skyTop, 0.55),
      D.mix(c.skyTop, BLACK, 0.25),
    ];
    R.RIDGES.forEach(function (spec, i) {
      var pts = R.ridge(g, spec[0], spec[1], spec[2]);
      ctx.beginPath();
      pts.forEach(function (p, n) {
        if (n) ctx.lineTo(p[0], p[1]);
        else ctx.moveTo(p[0], p[1]);
      });
      ctx.lineTo(g.w, g.horizonY + 1);
      ctx.lineTo(0, g.horizonY + 1);
      ctx.closePath();
      ctx.fillStyle = D.css(tones[i]);
      ctx.fill();
    });

    // The desert: a smooth gradient from the lit horizon to the dark
    // foreground, and the sun's glow lying on it.
    var sand = ctx.createLinearGradient(0, g.horizonY, 0, g.h);
    sand.addColorStop(0, D.css(D.mix(c.sunLow, c.ground, 0.55)));
    sand.addColorStop(0.25, D.css(D.mix(c.ground, c.glow, 0.12)));
    sand.addColorStop(1, D.css(D.mix(c.groundNear, BLACK, 0.3)));
    ctx.fillStyle = sand;
    ctx.fillRect(0, g.horizonY, g.w, g.depth);
    ctx.save();
    ctx.translate(g.vx, g.horizonY);
    ctx.scale(1, 0.32);
    var lie = ctx.createRadialGradient(0, 0, 0, 0, 0, g.w * 0.5);
    lie.addColorStop(0, D.css(c.sunLow, 0.45 + 0.2 * m.lit));
    lie.addColorStop(1, D.css(c.sunLow, 0));
    ctx.fillStyle = lie;
    ctx.fillRect(-g.w / 2, 0, g.w, g.depth / 0.32);
    ctx.restore();

    // The road: asphalt from the vanishing point, the sun on it, no edges.
    var asphalt = ctx.createLinearGradient(0, g.horizonY, 0, g.h);
    asphalt.addColorStop(0, D.css(D.mix(c.sunLow, c.ground, 0.5)));
    asphalt.addColorStop(0.15, D.css(D.mix(c.ground, c.groundNear, 0.4)));
    asphalt.addColorStop(1, D.css(D.mix(c.groundNear, BLACK, 0.45)));
    ctx.fillStyle = asphalt;
    ctx.beginPath();
    ctx.moveTo(g.vx - 1, g.horizonY);
    ctx.lineTo(g.vx + 1, g.horizonY);
    ctx.lineTo(g.screenX(1, 1), g.h);
    ctx.lineTo(g.screenX(-1, 1), g.h);
    ctx.closePath();
    ctx.fill();
    var streak = ctx.createLinearGradient(0, g.horizonY, 0, g.h);
    streak.addColorStop(0, D.css(c.sunTop, 0.35));
    streak.addColorStop(0.4, D.css(c.sunLow, 0.08));
    streak.addColorStop(1, D.css(c.sunLow, 0));
    ctx.fillStyle = streak;
    ctx.beginPath();
    ctx.moveTo(g.vx - 2, g.horizonY);
    ctx.lineTo(g.vx + 2, g.horizonY);
    ctx.lineTo(g.screenX(0.25, 1), g.h);
    ctx.lineTo(g.screenX(-0.25, 1), g.h);
    ctx.closePath();
    ctx.fill();

    var line = ctx.createLinearGradient(0, 0, g.w, 0);
    line.addColorStop(0, D.css(c.glow, 0));
    line.addColorStop(0.5, D.css(c.sunTop, 0.9));
    line.addColorStop(1, D.css(c.glow, 0));
    ctx.fillStyle = line;
    ctx.fillRect(0, g.horizonY - 1, g.w, 2);

    // The dashed centre line, and only that, from the shared motion.
    var mark = D.css(D.mix(c.sunTop, WHITE, 0.35));
    R.marks(g, m.travel, m.speed, function (x, y, w, h, alpha, kind) {
      if (kind !== "lane" || alpha <= 0) return;
      ctx.globalAlpha = alpha;
      ctx.fillStyle = mark;
      var wide = w * (g.halfWidth / (g.w * 0.42));
      ctx.fillRect(g.vx - wide / 2, y, wide, h);
    });

    // Silhouettes at the roadside, sparse: six on a long loop, far apart.
    var span = 44;
    var shade = D.css(D.mix(c.groundNear, BLACK, 0.55));
    for (var i = 0; i < KINDS.length; i++) {
      var d = 1.15 + R.wrap(i * (span / KINDS.length) + hash(i) * 3 - m.travel, span);
      var side = hash(i + 10) > 0.5 ? 1 : -1;
      var out = 1.25 + hash(i + 20) * 1.4;
      var x = g.screenX(side * out, d);
      var y = g.screenY(d);
      var size = (g.depth * (KINDS[i] === saguaro ? 0.55 : 0.35)) / d;
      ctx.globalAlpha = Math.max(0, Math.min(1, (span - d) / 10)) * Math.min(1, (d - 1.15) * 3);
      ctx.fillStyle = shade;
      KINDS[i](ctx, x, y, size);
    }
    ctx.globalAlpha = 1;

    var v = ctx.createRadialGradient(
      g.vx,
      g.horizonY,
      0,
      g.vx,
      g.horizonY,
      Math.max(g.w, g.h) * 0.85,
    );
    v.addColorStop(0.45, "rgba(0,0,0,0)");
    v.addColorStop(1, "rgba(0,0,0,0.5)");
    ctx.fillStyle = v;
    ctx.fillRect(0, 0, g.w, g.h);
  }

  window.OmawebScenes.register({
    id: "crt-road",
    name: "CRT road",
    pitch: "device",
    // Pixel blocks move in whole steps; 30 frames a second is all the motion
    // shows, at half the cost.
    fps: 30,
    // Choices the reader makes; the host passes the chosen value in
    // `input.options`. The first value is the default.
    options: {
      bands: ["3", "4"],
      road: ["wide", "wider", "widest"],
    },
    draw: function (ctx, input) {
      var scale = 1 / input.pitch;
      var s = input.state;
      var c = R.colours(input);
      var pic = CRT.canvas(
        s,
        "pic",
        Math.ceil(input.width / scale / PIXEL),
        Math.ceil(input.height / scale / PIXEL),
      );
      var p = pic.getContext("2d");
      p.setTransform(1 / PIXEL, 0, 0, 1 / PIXEL, 0, 0);
      draw(p, {
        width: pic.width,
        height: pic.height,
        palette: input.palette,
        dark: input.dark,
        time: input.time,
        navigating: input.navigating,
        reducedMotion: input.reducedMotion,
        options: input.options,
        state: s,
      });
      p.setTransform(1, 0, 0, 1, 0, 0);
      CRT.glass(ctx, input, s, c, pic, false);
    },
  });
})();
