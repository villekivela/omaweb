// PROTOTYPE (#440). The night road, ported from src/ui/NightRoad.qml: a night
// drive toward a banded sun, drawn in the palette and shown as a one-colour
// pixel display in the accent. The geometry, the palette mixes and the motion
// are the QML's; only the drawing calls differ.

(function () {
  "use strict";

  var D = window.OmawebDisplay;
  var BLACK = [0, 0, 0];
  var WHITE = [255, 255, 255];

  function fraction(v) {
    return v - Math.floor(v);
  }

  function wrap(value, span) {
    return ((value % span) + span) % span;
  }

  function colours(input) {
    var p = input.palette;
    var lightTheme = !input.dark;
    var ground = lightTheme ? D.mix(p.text, BLACK, 0.45) : p.ground;
    var light = lightTheme ? p.ground : p.text;
    var deep = D.mix(ground, BLACK, lightTheme ? 0.72 : 0.45);
    var glow = p.accent;
    return {
      ground: ground,
      light: light,
      deep: deep,
      glow: glow,
      skyTop: deep,
      skyLow: D.mix(ground, glow, 0.28),
      groundNear: D.mix(deep, BLACK, 0.35),
      sunTop: D.mix(light, WHITE, 0.4),
      sunLow: D.mix(glow, light, 0.25),
    };
  }

  function geometry(w, h) {
    var horizonY = Math.round(h / 2);
    var depth = h - horizonY;
    var g = {
      w: w,
      h: h,
      horizonY: horizonY,
      vx: w / 2,
      halfWidth: w * 0.42,
      depth: depth,
      railY: h - depth * 0.3,
    };
    g.screenY = function (z) {
      return horizonY + depth / z;
    };
    g.screenX = function (u, z) {
      return g.vx + (u * g.halfWidth) / z;
    };
    return g;
  }

  function ridge(ctx, g, seed, amplitude, notch, fill, rim, rimAlpha) {
    var base = g.horizonY;
    var top = base - amplitude * base;
    var gradient = ctx.createLinearGradient(0, top, 0, base);
    gradient.addColorStop(0, D.css(D.mix(fill, rim, 0.22)));
    gradient.addColorStop(0.6, D.css(fill));
    gradient.addColorStop(1, D.css(D.mix(fill, BLACK, 0.25)));
    ctx.beginPath();
    for (var i = 0; i <= 220; i++) {
      var u = i / 220;
      var d = Math.min(1, Math.abs(u - 0.5) / notch);
      var valley = d * d * (3 - 2 * d);
      var height = 0.55 * Math.pow(1 - Math.abs(Math.sin(u * 6.3 + seed)), 1.6);
      height += 0.3 * Math.pow(1 - Math.abs(Math.sin(u * 15.7 + seed * 2.3)), 2);
      height += 0.15 * Math.pow(1 - Math.abs(Math.sin(u * 37.1 + seed * 5.1)), 2);
      height = 0.25 + height * 0.75;
      var x = u * g.w;
      var y = base - height * amplitude * base * valley;
      if (i === 0) ctx.moveTo(x, y);
      else ctx.lineTo(x, y);
    }
    ctx.lineTo(g.w, base + 1);
    ctx.lineTo(0, base + 1);
    ctx.closePath();
    ctx.fillStyle = gradient;
    ctx.fill();
    ctx.strokeStyle = D.css(rim, rimAlpha);
    ctx.lineWidth = 1.2;
    ctx.stroke();
  }

  // The sky, the sun, the mountains, the ground and the road: drawn once per
  // size and palette.
  function still(ctx, g, c) {
    var sky = ctx.createLinearGradient(0, 0, 0, g.horizonY + 1);
    sky.addColorStop(0, D.css(c.skyTop));
    sky.addColorStop(0.55, D.css(D.mix(c.skyTop, c.skyLow, 0.35)));
    sky.addColorStop(1, D.css(c.skyLow));
    ctx.fillStyle = sky;
    ctx.fillRect(0, 0, g.w, g.horizonY + 1);

    for (var i = 0; i < 240; i++) {
      var across = fraction(Math.sin(i * 12.9898) * 43758.5453);
      var high = Math.pow(fraction(Math.sin(i * 78.233) * 12543.123), 1.6);
      var bright = fraction(Math.sin(i * 39.425) * 9321.77);
      var size = bright > 0.96 ? 2.5 : bright > 0.8 ? 1.6 : 1;
      ctx.fillStyle = D.css(c.light, (0.2 + 0.8 * bright) * (1 - high * 0.85));
      ctx.fillRect(across * g.w, high * g.horizonY * 0.9, size, size);
    }

    var glow = ctx.createRadialGradient(g.vx, g.horizonY, 0, g.vx, g.horizonY, g.w * 0.55);
    glow.addColorStop(0, D.css(c.sunLow, 0.6));
    glow.addColorStop(0.18, D.css(c.sunLow, 0.28));
    glow.addColorStop(0.45, D.css(c.glow, 0.1));
    glow.addColorStop(1, D.css(c.glow, 0));
    ctx.fillStyle = glow;
    ctx.fillRect(0, 0, g.w, g.horizonY);

    // The sun: a disc cut by bands that widen toward the horizon.
    var r = g.h * 0.15;
    ctx.save();
    ctx.beginPath();
    ctx.rect(g.vx - r, g.horizonY - r, r * 2, r * 0.42);
    var band = (r * 0.58) / 7;
    for (var b = 0; b < 7; b++) {
      ctx.rect(g.vx - r, g.horizonY - r + r * 0.42 + b * band, r * 2, band * (0.88 - b * 0.1));
    }
    ctx.clip();
    var disc = ctx.createLinearGradient(0, g.horizonY - r, 0, g.horizonY + r);
    disc.addColorStop(0, D.css(c.sunTop));
    disc.addColorStop(0.5, D.css(c.sunLow));
    ctx.fillStyle = disc;
    ctx.beginPath();
    ctx.arc(g.vx, g.horizonY, r, 0, Math.PI * 2);
    ctx.fill();
    ctx.restore();

    ridge(ctx, g, 1.3, 0.3, 0.16, D.mix(c.skyLow, c.skyTop, 0.3), c.glow, 0.35);
    ridge(ctx, g, 4.2, 0.2, 0.22, D.mix(c.skyLow, c.skyTop, 0.65), c.glow, 0.55);
    ridge(ctx, g, 8.9, 0.11, 0.3, D.mix(c.skyTop, c.groundNear, 0.7), c.glow, 0.85);

    var earth = ctx.createLinearGradient(0, g.horizonY, 0, g.h);
    earth.addColorStop(0, D.css(D.mix(c.skyLow, c.groundNear, 0.4)));
    earth.addColorStop(0.1, D.css(c.groundNear));
    earth.addColorStop(1, D.css(D.mix(c.groundNear, BLACK, 0.35)));
    ctx.fillStyle = earth;
    ctx.fillRect(0, g.horizonY, g.w, g.depth);

    function wedge(spread, fill) {
      ctx.beginPath();
      ctx.moveTo(g.vx - (spread < 1 ? 2 : 1), g.horizonY);
      ctx.lineTo(g.vx + (spread < 1 ? 2 : 1), g.horizonY);
      ctx.lineTo(g.screenX(spread, 1), g.h);
      ctx.lineTo(g.screenX(-spread, 1), g.h);
      ctx.closePath();
      ctx.fillStyle = fill;
      ctx.fill();
    }
    var asphalt = ctx.createLinearGradient(0, g.horizonY, 0, g.h);
    asphalt.addColorStop(0, D.css(D.mix(c.sunLow, c.ground, 0.35)));
    asphalt.addColorStop(0.12, D.css(D.mix(c.ground, c.groundNear, 0.2)));
    asphalt.addColorStop(1, D.css(D.mix(c.groundNear, BLACK, 0.1)));
    wedge(1.08, asphalt);
    var streak = ctx.createLinearGradient(0, g.horizonY, 0, g.h);
    streak.addColorStop(0, D.css(c.sunTop, 0.4));
    streak.addColorStop(0.35, D.css(c.sunLow, 0.12));
    streak.addColorStop(1, D.css(c.sunLow, 0));
    wedge(0.3, streak);

    // Edge lines and rails from the vanishing point, glowing.
    ctx.save();
    ctx.shadowColor = D.css(c.glow);
    ctx.shadowBlur = 3;
    ctx.lineWidth = 2.5;
    ctx.strokeStyle = D.css(c.glow);
    ctx.beginPath();
    ctx.moveTo(g.vx, g.horizonY);
    ctx.lineTo(g.screenX(-1, 1), g.h);
    ctx.moveTo(g.vx, g.horizonY);
    ctx.lineTo(g.screenX(1, 1), g.h);
    ctx.stroke();
    ctx.lineWidth = 2;
    ctx.strokeStyle = D.css(D.mix(c.light, c.glow, 0.4), 0.55);
    ctx.beginPath();
    ctx.moveTo(g.vx, g.horizonY - 1);
    ctx.lineTo(g.screenX(-1.5, 1), g.railY);
    ctx.moveTo(g.vx, g.horizonY - 1);
    ctx.lineTo(g.screenX(1.5, 1), g.railY);
    ctx.stroke();
    ctx.restore();

    var line = ctx.createLinearGradient(0, 0, g.w, 0);
    line.addColorStop(0, D.css(c.glow, 0));
    line.addColorStop(0.5, D.css(c.sunTop, 1));
    line.addColorStop(1, D.css(c.glow, 0));
    ctx.fillStyle = line;
    ctx.fillRect(0, g.horizonY - 1, g.w, 2);
  }

  // The grid, the centre line and the posts, placed from the travel.
  function moving(ctx, g, c, travel) {
    for (var i = 0; i < 12; i++) {
      var d = 1 + wrap(i * 2.5 - travel, 30);
      ctx.fillStyle = D.css(c.glow, 0.08 * Math.min(1, d - 1) * (1 - d / 32));
      ctx.fillRect(0, g.screenY(d), g.w, 1);
    }
    var mark = D.css(D.mix(c.sunTop, WHITE, 0.35));
    for (var m = 0; m < 26; m++) {
      var dm = 1 + wrap(m * 1.15 - travel, 26 * 1.15);
      var near = g.screenY(dm);
      var far = g.screenY(dm + 0.42);
      var width = Math.max(1, 12 / dm);
      ctx.globalAlpha = Math.max(0, Math.min(1, 1.3 - dm / 30) * Math.min(1, (dm - 1) * 2.5));
      ctx.fillStyle = mark;
      ctx.fillRect(g.vx - width / 2, far, width, Math.max(1, near - far));
    }
    var post = D.css(D.mix(c.groundNear, c.light, 0.45));
    var span = 12 * 1.6;
    for (var p = 0; p < 24; p++) {
      var side = p % 2 === 0 ? -1.5 : 1.5;
      var dp = 1.2 + wrap(Math.floor(p / 2) * 1.6 - travel, span);
      var pw = Math.max(1, 6 / dp);
      ctx.globalAlpha = Math.max(0, Math.min(1, 1.2 - dp / span) * Math.min(1, (dp - 1.2) * 2));
      ctx.fillStyle = post;
      ctx.fillRect(
        g.screenX(side, dp) - pw / 2,
        g.horizonY - 1 + (g.railY - g.horizonY + 1) / dp,
        pw,
        Math.max(1, (g.depth * 0.11) / dp),
      );
    }
    ctx.globalAlpha = 1;
  }

  function vignette(ctx, g) {
    var v = ctx.createRadialGradient(
      g.vx,
      g.horizonY,
      0,
      g.vx,
      g.horizonY,
      Math.max(g.w, g.h) * 0.85,
    );
    v.addColorStop(0.4, "rgba(0,0,0,0)");
    v.addColorStop(1, "rgba(0,0,0,0.6)");
    ctx.fillStyle = v;
    ctx.fillRect(0, 0, g.w, g.h);
  }

  window.OmawebScenes.register({
    id: "night-road",
    name: "Night road",
    pitch: 4,
    seams: true,
    draw: function (ctx, input) {
      var p = input.pitch;
      var g = geometry(input.width * p, input.height * p);
      var c = colours(input);
      var base = D.layer(input.state, "still", input.width, input.height);
      if (base.fresh) {
        var b = base.getContext("2d");
        b.setTransform(1 / p, 0, 0, 1 / p, 0, 0);
        still(b, g, c);
      }
      ctx.setTransform(1, 0, 0, 1, 0, 0);
      ctx.drawImage(base, 0, 0);
      ctx.setTransform(1 / p, 0, 0, 1 / p, 0, 0);
      // Still under reduced motion: the road as it stands at a fixed moment.
      moving(ctx, g, c, input.reducedMotion ? 0.7 : input.time * 1.6);
      vignette(ctx, g);
      ctx.setTransform(1, 0, 0, 1, 0, 0);
      D.light(ctx, input.width, input.height, c.glow);
    },
  });
})();
