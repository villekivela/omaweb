// The CRT road: the night drive behind the landing page's Start page, seen through CRT glass. A
// desert under a banded sun, its ground a smooth gradient lit by the sun's glow, layered ridges,
// a wide road with only its dashed centre line, now and then a saguaro, a rock or a lone sign at
// the roadside, and a rare shooting star, drawn in the theme's colours.
//
// What it draws by, every amount, colour recipe and timing, is share/scenes/crt-road.json, the
// file the browser's own road reads too (#496); this is only how it draws. What does not move is
// drawn once per size, theme and option into a cached layer; each frame copies it and draws what
// moves.

const BLACK = [0, 0, 0];
const WHITE = [255, 255, 255];

function mix(a, b, amount) {
  return a.map((channel, index) => channel + (b[index] - channel) * amount);
}

function css(colour, alpha = 1) {
  const [red, green, blue] = colour.map(Math.round);
  return `rgba(${red}, ${green}, ${blue}, ${alpha})`;
}

function fraction(value) {
  return value - Math.floor(value);
}

function wrap(value, span) {
  return ((value % span) + span) % span;
}

// A colour the parameters name: a role, or a mix of two at an amount.
function colourOf(name, c) {
  if (typeof name === "string") return c[name];
  const [from, to, amount] = name.mix;
  return mix(colourOf(from, c), colourOf(to, c), amount);
}

// A gradient's stops, from [position, colour] or [position, colour, alpha], the alphas scaled by
// `scale`.
function stops(gradient, list, c, scale = 1) {
  for (const [position, colour, alpha = 1] of list) {
    gradient.addColorStop(position, css(colourOf(colour, c), alpha * scale));
  }
  return gradient;
}

// A night in the theme. A light theme's night is drawn from its dark text, so the ground stays
// dark and the lit lines stay light.
function colours({ palette, dark }, night) {
  const ground = dark ? palette.ground : mix(palette.text, BLACK, night.lightThemeGround);
  const light = dark ? palette.text : palette.ground;
  const deep = mix(ground, BLACK, dark ? night.deep : night.deepLightTheme);
  const glow = palette.accent;
  return {
    black: BLACK,
    white: WHITE,
    ground,
    light,
    glow,
    skyTop: deep,
    skyLow: mix(ground, glow, night.skyLow),
    groundNear: mix(deep, BLACK, night.groundNear),
    sunTop: mix(light, WHITE, night.sunTop),
    sunLow: mix(glow, light, night.sunLow),
  };
}

function geometry(width, height, road, horizonAt) {
  const horizonY = Math.round(height * horizonAt);
  const depth = height - horizonY;
  const halfWidth = width * road;
  return {
    w: width,
    h: height,
    horizonY,
    depth,
    vx: width / 2,
    halfWidth,
    // A point on the road at depth z, where z = 1 is the bottom edge, and lateral position u,
    // where the road's edges are at -1 and 1.
    screenY: (z) => horizonY + depth / z,
    screenX: (u, z) => width / 2 + (u * halfWidth) / z,
  };
}

export function createCrtRoad(p) {
  // A repeatable scatter: the same index always lands in the same place.
  const [scale, offset, spread] = p.scatter;
  const hash = (index) => fraction(Math.sin(index * scale + offset) * spread);

  // The drive: travel advances with the clock at a speed that eases toward what `navigating`
  // asks, and the sun brightens with it. Kept in the Scene's state between frames.
  function motion(input) {
    const m = p.motion;
    const s = input.state;
    if (input.reducedMotion) return { travel: m.stillTravel, speed: 1, lit: 0 };
    if (s.last === undefined) Object.assign(s, { last: input.time, travel: 0, speed: 1, lit: 0 });
    const step = Math.max(0, Math.min(m.longestStep, input.time - s.last));
    s.last = input.time;
    s.speed += (1 + m.navigatingSpeed * input.navigating - s.speed) * Math.min(1, step * m.ease);
    s.lit += (input.navigating - s.lit) * Math.min(1, step * m.lightEase);
    s.travel += step * m.speed * m.pace * s.speed;
    return s;
  }

  // A ridge's outline: peaks from folded sines, falling to the horizon in a notch where the road
  // runs out.
  function ridge(g, layer) {
    const r = p.ridges;
    const points = [];
    for (let index = 0; index <= r.points; index++) {
      const u = index / r.points;
      const d = Math.min(1, Math.abs(u - 0.5) / layer.notch);
      const valley = d * d * (3 - 2 * d);
      let height = 0;
      for (const h of r.harmonics) {
        height +=
          h.weight *
          Math.pow(1 - Math.abs(Math.sin(u * h.frequency + layer.seed * h.phase)), h.sharpness);
      }
      height = r.floor + height * (1 - r.floor);
      points.push([u * g.w, g.horizonY - height * layer.height * g.horizonY * valley]);
    }
    return points;
  }

  // The sun: a solid cap, then `bands` cuts that thicken toward the horizon.
  function sun(context, g, c, bands) {
    const radius = g.h * p.sun.radius;
    context.save();
    context.beginPath();
    const solid = radius * p.sun.cap;
    context.rect(g.vx - radius, g.horizonY - radius, radius * 2, solid);
    const unit = (radius - solid) / (bands * 2);
    let y = g.horizonY - (radius - solid);
    for (let band = 0; band < bands; band++) {
      const gap = unit * (p.sun.firstGap + band / bands);
      context.rect(g.vx - radius, y + gap, radius * 2, unit * 2 - gap);
      y += unit * 2;
    }
    context.clip();
    context.fillStyle = stops(
      context.createLinearGradient(0, g.horizonY - radius, 0, g.horizonY),
      p.sun.stops,
      c,
    );
    context.beginPath();
    context.arc(g.vx, g.horizonY, radius, 0, Math.PI * 2);
    context.fill();
    context.restore();
  }

  // What does not move: the sky and its stars, the sunrise glow, the sun, the ridges, the desert
  // and the road on it.
  function still(context, g, c, bands) {
    context.fillStyle = stops(context.createLinearGradient(0, 0, 0, g.horizonY), p.sky, c);
    context.fillRect(0, 0, g.w, g.horizonY + 1);

    // Stars, a few of them brighter, thinning toward the horizon.
    const st = p.stars;
    for (let index = 0; index < st.count; index++) {
      const across = fraction(Math.sin(index * st.across[0]) * st.across[1]);
      const high = Math.pow(
        fraction(Math.sin(index * st.height[0]) * st.height[1]),
        st.heightPower,
      );
      const bright = fraction(Math.sin(index * st.brightness[0]) * st.brightness[1]);
      const brightest = bright > st.brightAbove;
      const size = brightest
        ? st.sizes.bright
        : bright > st.mediumAbove
          ? st.sizes.medium
          : st.sizes.dim;
      const alpha = brightest
        ? 1
        : (st.alpha.base + st.alpha.range * bright) * (1 - high * st.alpha.horizonFade);
      context.fillStyle = css(c.light, alpha);
      context.fillRect(across * g.w, high * g.horizonY * st.reach, size, size);
    }

    const halo = context.createRadialGradient(
      g.vx,
      g.horizonY,
      0,
      g.vx,
      g.horizonY,
      g.w * p.halo.radius,
    );
    context.fillStyle = stops(halo, p.halo.stops, c);
    context.fillRect(0, 0, g.w, g.horizonY);
    sun(context, g, c, bands);

    // The ridges as filled silhouettes, each a tone darker than the one behind it.
    for (const layer of p.ridges.layers) {
      context.beginPath();
      for (const [x, y] of ridge(g, layer)) context.lineTo(x, y);
      context.lineTo(g.w, g.horizonY + 1);
      context.lineTo(0, g.horizonY + 1);
      context.closePath();
      context.fillStyle = css(colourOf(layer.tone, c));
      context.fill();
    }

    // The desert at night, lit near the horizon and where the sun's glow lies on it.
    context.fillStyle = stops(
      context.createLinearGradient(0, g.horizonY, 0, g.h),
      p.desert.stops,
      c,
    );
    context.fillRect(0, g.horizonY, g.w, g.depth);
    const lying = p.desert.glow;
    context.save();
    context.translate(g.vx, g.horizonY);
    context.scale(1, lying.squash);
    context.fillStyle = stops(
      context.createRadialGradient(0, 0, 0, 0, 0, g.w * lying.radius),
      lying.stops,
      c,
    );
    context.fillRect(-g.w / 2, 0, g.w, g.depth / lying.squash);
    context.restore();

    // The road: the sand a step darker at every depth, so it reads as a road through the desert,
    // with wider passes for a soft edge rather than a line, and the sun on the asphalt.
    const wedge = (spread, fill) => {
      context.fillStyle = fill;
      context.beginPath();
      context.moveTo(g.vx - p.road.tip, g.horizonY);
      context.lineTo(g.vx + p.road.tip, g.horizonY);
      context.lineTo(g.screenX(spread, 1), g.h);
      context.lineTo(g.screenX(-spread, 1), g.h);
      context.closePath();
      context.fill();
    };
    for (const edge of p.road.edges) wedge(edge.spread, css(BLACK, edge.shade));
    const reflection = p.road.reflection;
    wedge(
      reflection.spread,
      stops(context.createLinearGradient(0, g.horizonY, 0, g.h), reflection.stops, c),
    );

    const line = p.horizon.lineHeight;
    context.fillStyle = stops(context.createLinearGradient(0, 0, g.w, 0), p.horizon.line, c);
    context.fillRect(0, g.horizonY - line / 2, g.w, line);
  }

  // A roadside silhouette standing on the ground at (x, y), `size` tall.
  function silhouette(context, shape, x, y, size) {
    for (const [left, top, width, height] of shape.rects || []) {
      context.fillRect(x + left * size, y + top * size, width * size, height * size);
    }
    if (shape.polygon) {
      context.beginPath();
      for (const [px, py] of shape.polygon) context.lineTo(x + px * size, y + py * size);
      context.closePath();
      context.fill();
    }
  }

  // What moves: the sun brightening as the reader navigates, a rare shooting star, the centre
  // line and the roadside.
  function moving(context, g, c, m, input) {
    const glow = p.navigatingGlow;
    if (m.lit > glow.from) {
      const halo = context.createRadialGradient(
        g.vx,
        g.horizonY,
        0,
        g.vx,
        g.horizonY,
        g.w * glow.radius,
      );
      context.fillStyle = stops(halo, glow.stops, c, m.lit);
      context.fillRect(0, 0, g.w, g.h);
    }

    // A shooting star in some of its slots, for under a second.
    const star = p.shootingStar;
    const slot = Math.floor(input.time / star.every);
    const into = input.time - slot * star.every;
    if (!input.reducedMotion && into < star.lasts && hash(slot) > star.chance) {
      const length = g.w * star.length;
      const run = into / star.lasts;
      const x = (star.across[0] + star.across[1] * hash(slot + 1)) * g.w + length * run;
      const y =
        (star.height[0] + star.height[1] * hash(slot + 2)) * g.horizonY + length * star.slope * run;
      const tailX = x - length * star.tail;
      const tailY = y - length * star.tail * star.slope;
      const trail = context.createLinearGradient(tailX, tailY, x, y);
      trail.addColorStop(0, css(c.light, 0));
      trail.addColorStop(1, css(colourOf(star.colour, c), 1 - run * star.fade));
      context.strokeStyle = trail;
      context.lineWidth = star.width;
      context.beginPath();
      context.moveTo(tailX, tailY);
      context.lineTo(x, y);
      context.stroke();
    }

    // The dashed centre line, its marks stretching as the road speeds up and widening with it.
    const cl = p.centreLine;
    const length = cl.length + Math.min(cl.stretchMost, (m.speed - 1) * cl.stretch);
    const scale = g.halfWidth / (g.w * cl.widthAtRoad);
    context.fillStyle = css(colourOf(cl.colour, c));
    for (let index = 0; index < cl.marks; index++) {
      const d = 1 + wrap(index * cl.spacing - m.travel, cl.marks * cl.spacing);
      const near = g.screenY(d);
      const far = g.screenY(d + length);
      const width = Math.max(1, cl.width / d) * scale;
      const alpha = Math.min(1, cl.fadeFrom - d / cl.fadeFar) * Math.min(1, (d - 1) * cl.fadeNear);
      context.globalAlpha = Math.max(0, alpha);
      context.fillRect(g.vx - width / 2, far, width, Math.max(1, near - far));
    }

    // The roadside: a few silhouettes on a long loop, far apart.
    const rs = p.roadside;
    context.fillStyle = css(colourOf(rs.colour, c));
    rs.order.forEach((kind, index) => {
      const shape = rs.shapes[kind];
      const place = index * (rs.span / rs.order.length) + hash(index) * rs.scatter - m.travel;
      const d = rs.nearest + wrap(place, rs.span);
      const side = hash(index + rs.seeds.side) > 0.5 ? 1 : -1;
      const out = rs.out[0] + hash(index + rs.seeds.out) * rs.out[1];
      const fade =
        Math.min(1, (rs.span - d) / rs.fadeFar) * Math.min(1, (d - rs.nearest) * rs.fadeNear);
      context.globalAlpha = Math.max(0, fade);
      silhouette(
        context,
        shape,
        g.screenX(side * out, d),
        g.screenY(d),
        (g.depth * shape.height) / d,
      );
    });
    context.globalAlpha = 1;
  }

  return {
    id: p.id,
    name: p.name,
    pitch: p.pitch,
    glass: p.glass,
    fps: p.fps,
    options: p.options,

    draw(context, input) {
      const width = p.roadWidth[input.options.road];
      const g = geometry(input.width * p.pitch, input.height * p.pitch, width, p.horizon.at);
      const c = colours(input, p.night);
      const s = input.state;
      if (!s.still) {
        s.still = document.createElement("canvas");
        s.still.width = input.width;
        s.still.height = input.height;
        const layer = s.still.getContext("2d");
        layer.scale(1 / p.pitch, 1 / p.pitch);
        still(layer, g, c, Number(input.options.bands));
      }
      context.setTransform(1, 0, 0, 1, 0, 0);
      context.drawImage(s.still, 0, 0);
      context.setTransform(1 / p.pitch, 0, 0, 1 / p.pitch, 0, 0);
      moving(context, g, c, motion(input), input);
      context.setTransform(1, 0, 0, 1, 0, 0);
    },
  };
}
