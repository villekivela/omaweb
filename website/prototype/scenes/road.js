// PROTOTYPE (#440). The night drive every road Scene shares: its geometry, its
// palette mixes and its motion, from src/ui/NightRoad.qml. The Scenes differ
// only in how they draw it. `fill` draws the road in the theme's colours, as
// NightRoad.qml's scene does before its display pass, for the Scenes that map
// it to a display of their own.

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

  // A night in the theme: a light theme's night is drawn from its dark text,
  // so the ground stays dark and the lit lines stay light.
  function colours(input) {
    var p = input.palette;
    var ground = input.dark ? p.ground : D.mix(p.text, BLACK, 0.45);
    var light = input.dark ? p.text : p.ground;
    var deep = D.mix(ground, BLACK, input.dark ? 0.45 : 0.72);
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
    var g = { w: w, h: h, horizonY: horizonY, vx: w / 2, halfWidth: w * 0.42, depth: depth };
    g.railY = h - depth * 0.3;
    g.screenY = function (z) {
      return horizonY + depth / z;
    };
    g.screenX = function (u, z) {
      return g.vx + (u * g.halfWidth) / z;
    };
    return g;
  }

  // The drive: travel advances with time at a speed that eases toward the
  // target `navigating` sets, and the sun lights up with it. Kept in the
  // Scene's state, so a Scene stays a function of its inputs over time.
  function motion(input, base) {
    var s = input.state;
    if (input.reducedMotion) return { travel: 0.7, speed: 1, lit: 0 };
    if (s.last === undefined) {
      s.last = input.time;
      s.travel = 0;
      s.speed = 1;
      s.lit = 0;
    }
    var dt = Math.max(0, Math.min(0.1, input.time - s.last));
    s.last = input.time;
    var target = 1 + 8 * input.navigating;
    s.speed += (target - s.speed) * Math.min(1, dt * 2.2);
    s.lit += (input.navigating - s.lit) * Math.min(1, dt * 3);
    s.travel += dt * 1.6 * (base || 1) * s.speed;
    return { travel: s.travel, speed: s.speed, lit: s.lit };
  }

  function stars(count, each) {
    for (var i = 0; i < count; i++) {
      each(
        fraction(Math.sin(i * 12.9898) * 43758.5453),
        Math.pow(fraction(Math.sin(i * 78.233) * 12543.123), 1.6),
        fraction(Math.sin(i * 39.425) * 9321.77),
      );
    }
  }

  // A ridge's outline: sharp peaks from folded sines, falling to the horizon
  // in a notch where the road runs out.
  function ridge(g, seed, amplitude, notch) {
    var points = [];
    for (var i = 0; i <= 220; i++) {
      var u = i / 220;
      var d = Math.min(1, Math.abs(u - 0.5) / notch);
      var valley = d * d * (3 - 2 * d);
      var height = 0.55 * Math.pow(1 - Math.abs(Math.sin(u * 6.3 + seed)), 1.6);
      height += 0.3 * Math.pow(1 - Math.abs(Math.sin(u * 15.7 + seed * 2.3)), 2);
      height += 0.15 * Math.pow(1 - Math.abs(Math.sin(u * 37.1 + seed * 5.1)), 2);
      height = 0.25 + height * 0.75;
      points.push([u * g.w, g.horizonY - height * amplitude * g.horizonY * valley]);
    }
    return points;
  }

  var RIDGES = [
    [1.3, 0.3, 0.16],
    [4.2, 0.2, 0.22],
    [8.9, 0.11, 0.3],
  ];

  // The road in the theme's colours: what does not move, then what does.
  // `options.weight` thickens its lines, for a Scene that samples it coarsely;
  // `options.soft: false` leaves out the soft light (the sun's halo, its
  // streak on the asphalt and the vignette), for a Scene with few tones.
  function fill(ctx, g, c, m, options) {
    options = options || {};
    var k = options.weight || 1;
    var soft = options.soft !== false;
    var sky = ctx.createLinearGradient(0, 0, 0, g.horizonY);
    sky.addColorStop(0, D.css(c.skyTop));
    sky.addColorStop(0.55, D.css(D.mix(c.skyTop, c.skyLow, 0.35)));
    sky.addColorStop(1, D.css(c.skyLow));
    ctx.fillStyle = sky;
    ctx.fillRect(0, 0, g.w, g.horizonY + 1);

    stars(240, function (across, high, bright) {
      var size = bright > 0.96 ? 2.5 : bright > 0.8 ? 1.6 : 1;
      ctx.fillStyle = D.css(c.light, (0.2 + 0.8 * bright) * (1 - high * 0.85));
      ctx.fillRect(across * g.w, high * g.horizonY * 0.9, size, size);
    });

    if (soft) {
      var halo = ctx.createRadialGradient(g.vx, g.horizonY, 0, g.vx, g.horizonY, g.w * 0.55);
      halo.addColorStop(0, D.css(c.sunLow, 0.6 + 0.3 * m.lit));
      halo.addColorStop(0.18, D.css(c.sunLow, 0.28));
      halo.addColorStop(0.45, D.css(c.glow, 0.1));
      halo.addColorStop(1, D.css(c.glow, 0));
      ctx.fillStyle = halo;
      ctx.fillRect(0, 0, g.w, g.horizonY);
    }

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

    var fills = [
      D.mix(c.skyLow, c.skyTop, 0.3),
      D.mix(c.skyLow, c.skyTop, 0.65),
      D.mix(c.skyTop, c.groundNear, 0.7),
    ];
    var rims = [0.35, 0.55, 0.85];
    RIDGES.forEach(function (spec, i) {
      var pts = ridge(g, spec[0], spec[1], spec[2]);
      ctx.beginPath();
      pts.forEach(function (p, n) {
        if (n) ctx.lineTo(p[0], p[1]);
        else ctx.moveTo(p[0], p[1]);
      });
      ctx.lineTo(g.w, g.horizonY + 1);
      ctx.lineTo(0, g.horizonY + 1);
      ctx.closePath();
      ctx.fillStyle = D.css(fills[i]);
      ctx.fill();
      ctx.strokeStyle = D.css(c.glow, rims[i]);
      ctx.lineWidth = 1.2 * k;
      ctx.stroke();
    });

    var earth = ctx.createLinearGradient(0, g.horizonY, 0, g.h);
    earth.addColorStop(0, D.css(D.mix(c.skyLow, c.groundNear, 0.4)));
    earth.addColorStop(0.1, D.css(c.groundNear));
    earth.addColorStop(1, D.css(D.mix(c.groundNear, BLACK, 0.35)));
    ctx.fillStyle = earth;
    ctx.fillRect(0, g.horizonY, g.w, g.depth);

    function wedge(spread, style) {
      ctx.beginPath();
      ctx.moveTo(g.vx - 1, g.horizonY);
      ctx.lineTo(g.vx + 1, g.horizonY);
      ctx.lineTo(g.screenX(spread, 1), g.h);
      ctx.lineTo(g.screenX(-spread, 1), g.h);
      ctx.closePath();
      ctx.fillStyle = style;
      ctx.fill();
    }
    var asphalt = ctx.createLinearGradient(0, g.horizonY, 0, g.h);
    asphalt.addColorStop(0, D.css(D.mix(c.sunLow, c.ground, 0.35)));
    asphalt.addColorStop(0.12, D.css(D.mix(c.ground, c.groundNear, 0.2)));
    asphalt.addColorStop(1, D.css(D.mix(c.groundNear, BLACK, 0.1)));
    wedge(1.08, asphalt);
    if (soft) {
      var streak = ctx.createLinearGradient(0, g.horizonY, 0, g.h);
      streak.addColorStop(0, D.css(c.sunTop, 0.4));
      streak.addColorStop(0.35, D.css(c.sunLow, 0.12));
      streak.addColorStop(1, D.css(c.sunLow, 0));
      wedge(0.3, streak);
    }

    ctx.lineWidth = 2.5 * k;
    ctx.strokeStyle = D.css(c.glow);
    ctx.beginPath();
    ctx.moveTo(g.vx, g.horizonY);
    ctx.lineTo(g.screenX(-1, 1), g.h);
    ctx.moveTo(g.vx, g.horizonY);
    ctx.lineTo(g.screenX(1, 1), g.h);
    ctx.stroke();
    ctx.lineWidth = 2 * k;
    ctx.strokeStyle = D.css(D.mix(c.light, c.glow, 0.4), 0.55);
    ctx.beginPath();
    ctx.moveTo(g.vx, g.horizonY - 1);
    ctx.lineTo(g.screenX(-1.5, 1), g.railY);
    ctx.moveTo(g.vx, g.horizonY - 1);
    ctx.lineTo(g.screenX(1.5, 1), g.railY);
    ctx.stroke();

    var line = ctx.createLinearGradient(0, 0, g.w, 0);
    line.addColorStop(0, D.css(c.glow, 0));
    line.addColorStop(0.5, D.css(c.sunTop, 1));
    line.addColorStop(1, D.css(c.glow, 0));
    ctx.fillStyle = line;
    ctx.fillRect(0, g.horizonY - k, g.w, 2 * k);

    marks(g, m.travel, m.speed, function (x, y, w, h, alpha, kind) {
      ctx.globalAlpha = alpha;
      if (k > 1 && kind !== "grid") {
        x -= (w * (k - 1)) / 2;
        w *= k;
      }
      ctx.fillStyle =
        kind === "grid"
          ? D.css(c.glow, 0.08)
          : kind === "post"
            ? D.css(D.mix(c.groundNear, c.light, 0.45))
            : D.css(D.mix(c.sunTop, WHITE, 0.35));
      ctx.fillRect(x, y, w, h);
    });
    ctx.globalAlpha = 1;

    if (soft) {
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
  }

  // What moves: the ground grid, the centre line and the rail posts, each as
  // a rectangle with an opacity, placed from the travel as NightRoad.qml does.
  function marks(g, travel, speed, each) {
    for (var i = 0; i < 12; i++) {
      var d = 1 + wrap(i * 2.5 - travel, 30);
      each(0, g.screenY(d), g.w, 1, Math.max(0, Math.min(1, d - 1) * (1 - d / 32)), "grid");
    }
    var length = 0.42 + Math.min(0.7, (speed - 1) * 0.1);
    for (var m = 0; m < 26; m++) {
      var dm = 1 + wrap(m * 1.15 - travel, 26 * 1.15);
      var near = g.screenY(dm);
      var far = g.screenY(dm + length);
      var width = Math.max(1, 12 / dm);
      var a = Math.min(1, 1.3 - dm / 30) * Math.min(1, (dm - 1) * 2.5);
      each(g.vx - width / 2, far, width, Math.max(1, near - far), Math.max(0, a), "lane");
    }
    var span = 12 * 1.6;
    for (var p = 0; p < 24; p++) {
      var side = p % 2 === 0 ? -1.5 : 1.5;
      var dp = 1.2 + wrap(Math.floor(p / 2) * 1.6 - travel, span);
      var pw = Math.max(1, 6 / dp);
      var pa = Math.min(1, 1.2 - dp / span) * Math.min(1, (dp - 1.2) * 2);
      each(
        g.screenX(side, dp) - pw / 2,
        g.horizonY - 1 + (g.railY - g.horizonY + 1) / dp,
        pw,
        Math.max(1, (g.depth * 0.11) / dp),
        Math.max(0, pa),
        "post",
      );
    }
  }

  // Grey, 0 to 1, of an RGBA buffer's pixel.
  function grey(data, i) {
    return (data[i] * 0.299 + data[i + 1] * 0.587 + data[i + 2] * 0.114) / 255;
  }

  window.OmawebRoad = {
    colours: colours,
    geometry: geometry,
    motion: motion,
    stars: stars,
    ridge: ridge,
    RIDGES: RIDGES,
    fill: fill,
    marks: marks,
    grey: grey,
    wrap: wrap,
  };
})();
