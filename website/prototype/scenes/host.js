// PROTOTYPE (#440). The Scene host: owns the canvas, the clock and the rules
// for when a Scene may draw. A Scene only draws; see ../README.md for the
// contract. Classic script, so a Scene file is one more <script> tag.

(function () {
  "use strict";

  var registry = [];
  var hosts = [];
  var reduceQuery = matchMedia("(prefers-reduced-motion: reduce)");
  // A phone holds the road still too: motion there costs battery for a page
  // read in passing. The Scene is told as it is for reduced motion.
  var phoneQuery = matchMedia("(max-width: 860px)");

  function still() {
    return reduceQuery.matches || phoneQuery.matches;
  }

  function register(scene) {
    registry = registry.filter(function (s) {
      return s.id !== scene.id;
    });
    registry.push(scene);
    hosts.forEach(function (host) {
      if (host.sceneId === scene.id) host.setScene(scene.id);
    });
  }

  function find(id) {
    for (var i = 0; i < registry.length; i++) if (registry[i].id === id) return registry[i];
    return registry[0];
  }

  function rgb(value) {
    var v = value.trim();
    if (v[0] === "#") {
      if (v.length === 4) v = "#" + v[1] + v[1] + v[2] + v[2] + v[3] + v[3];
      return [
        parseInt(v.substr(1, 2), 16),
        parseInt(v.substr(3, 2), 16),
        parseInt(v.substr(5, 2), 16),
      ];
    }
    var m = v.match(/[\d.]+/g);
    return m ? [+m[0], +m[1], +m[2]] : [0, 0, 0];
  }

  function lightness(c) {
    var max = Math.max(c[0], c[1], c[2]) / 255;
    var min = Math.min(c[0], c[1], c[2]) / 255;
    return (max + min) / 2;
  }

  // The palette a Scene receives: the theme's roles, read off the element the
  // canvas sits in, so whatever carries data-theme decides it.
  function readPalette(element) {
    var style = getComputedStyle(element);
    function role(name, fallback) {
      var value = style.getPropertyValue(name);
      return rgb(value && value.trim() ? value : fallback);
    }
    var palette = {
      ground: role("--bg", "#141210"),
      text: role("--fg", "#ece3cf"),
      accent: role("--accent", "#ff5a2c"),
      muted: role("--muted", "#a59c8a"),
    };
    return { palette: palette, dark: lightness(palette.ground) <= 0.6 };
  }

  // One canvas, one Scene. `options.clock` is "time" (the default) or
  // "manual", where the page sets the time itself, as a scroll-driven page
  // does; the Scene cannot tell the difference.
  function Host(canvas, options) {
    options = options || {};
    this.canvas = canvas;
    this.ctx = canvas.getContext("2d", { willReadFrequently: true });
    this.clock = options.clock || "time";
    this.time = options.time || 0;
    this.navigating = 0;
    this.chosen = options.chosen || {};
    this.visible = false;
    this.frame = 0;
    this.last = 0;
    this.sceneId = options.scene;
    this.scene = find(options.scene);
    this.state = {};
    var self = this;
    canvas.classList.add("scene-canvas");
    this.observer = new IntersectionObserver(function (entries) {
      self.visible = entries[entries.length - 1].isIntersecting;
      self.update();
    });
    this.observer.observe(canvas);
    this.resizer = new ResizeObserver(function () {
      self.layout();
    });
    this.resizer.observe(canvas);
    hosts.push(this);
    this.layout();
  }

  Host.prototype.setScene = function (id) {
    this.sceneId = id;
    this.scene = find(id);
    this.state = {};
    this.layout();
  };

  // A Scene's pitch: logical pixels per display pixel, or "device" for one
  // display pixel per device pixel, which keeps thin lines crisp.
  function pitchOf(scene) {
    return scene.pitch === "device" ? 1 / (window.devicePixelRatio || 1) : scene.pitch || 1;
  }

  Host.prototype.layout = function () {
    if (!this.scene) return;
    var pitch = pitchOf(this.scene);
    var rect = this.canvas.getBoundingClientRect();
    var w = Math.max(1, Math.ceil(rect.width / pitch));
    var h = Math.max(1, Math.ceil(rect.height / pitch));
    if (this.canvas.width !== w || this.canvas.height !== h) {
      this.canvas.width = w;
      this.canvas.height = h;
      this.state = {};
    }
    this.canvas.style.setProperty("--pitch", pitch + "px");
    this.canvas.parentElement.style.setProperty("--pitch", pitch + "px");
    this.canvas.parentElement.classList.toggle("scene-seams", !!this.scene.seams);
    this.glass();
    this.themed = readPalette(this.canvas);
    this.draw();
    this.update();
  };

  // A Scene that declares `glass: "crt"` is shown through a CRT the host
  // composites over the canvas: scanlines, a bloom, a rolling band, a faint
  // flicker and darker corners, as CSS layers the browser draws on the GPU.
  // Only the bloom changes with the picture, and it is a tiny copy of it.
  Host.prototype.glass = function () {
    var frame = this.canvas.parentElement;
    var want = this.scene && this.scene.glass === "crt";
    frame.classList.toggle("scene-crt", !!want);
    if (!want) {
      if (this.bloom) {
        frame.querySelectorAll(".crt-layer").forEach(function (layer) {
          layer.remove();
        });
        this.bloom = null;
      }
      return;
    }
    if (!this.bloom) {
      this.bloom = document.createElement("canvas");
      this.bloom.className = "crt-layer crt-bloom";
      frame.appendChild(this.bloom);
      var made = {};
      ["crt-scan", "crt-vignette"].forEach(function (name) {
        var layer = document.createElement("div");
        layer.className = "crt-layer " + name;
        frame.appendChild(layer);
        made[name] = layer;
      });
    }
    this.bloom.width = Math.max(1, Math.ceil(this.canvas.width / 3));
    this.bloom.height = Math.max(1, Math.ceil(this.canvas.height / 3));
  };

  // The CRT's refresh band rolling down every 7 s and its flicker, a few
  // percent darker at most, drawn over the Scene's picture.
  Host.prototype.crtMotion = function () {
    var ctx = this.ctx;
    var w = this.canvas.width;
    var h = this.canvas.height;
    var t = this.time;
    var text = this.themed.palette.text;
    var y = ((t / 7) % 1) * h * 1.4 - h * 0.2;
    var reach = Math.max(4, h * 0.07);
    var band = ctx.createLinearGradient(0, y - reach, 0, y + reach);
    band.addColorStop(0, "rgba(255,255,255,0)");
    band.addColorStop(0.5, "rgba(" + text.join(",") + ",0.05)");
    band.addColorStop(1, "rgba(255,255,255,0)");
    ctx.fillStyle = band;
    ctx.fillRect(0, y - reach, w, reach * 2);
    var f = 0.02 + 0.025 * Math.abs(Math.sin(t * 37.1) * Math.sin(t * 11.3));
    ctx.fillStyle = "rgba(0,0,0," + f.toFixed(3) + ")";
    ctx.fillRect(0, 0, w, h);
  };

  // A new theme: the palette is read again and the Scene redraws, even when
  // it is not moving.
  Host.prototype.retheme = function () {
    this.state = {};
    this.layout();
  };

  // The reader's choice for one of the Scene's declared options.
  Host.prototype.setOption = function (name, value) {
    this.chosen[name] = value;
    this.state = {};
    this.draw();
  };

  // Every option the Scene declares, at the reader's choice or its first value.
  Host.prototype.optionValues = function () {
    var declared = (this.scene && this.scene.options) || {};
    var values = {};
    for (var name in declared) {
      var choice = this.chosen[name];
      values[name] = declared[name].indexOf(choice) >= 0 ? choice : declared[name][0];
    }
    return values;
  };

  // How hard the reader is navigating, 0 to 1: the page sets it from the
  // scroll's speed, the browser from a commit until its page paints.
  Host.prototype.setNavigating = function (value) {
    this.navigating = Math.max(0, Math.min(1, value));
  };

  Host.prototype.setTime = function (time) {
    this.time = time;
    if (this.visible) this.draw();
  };

  // PROTOTYPE measurement: main-thread time spent in draw, and draws made.
  Host.prototype.resetStats = function () {
    this.stats = { draws: 0, drawMs: 0, maxMs: 0, since: performance.now() };
  };

  Host.prototype.draw = function () {
    if (!this.scene || !this.themed) return;
    if (!this.stats) this.resetStats();
    var started = performance.now();
    this.drawScene();
    if (this.bloom) {
      // The band and the flicker are drawn into the small picture itself, on
      // the Scene's clock: two more full-screen layers would be composited
      // every frame for what a few pixels here show as well.
      if (!still()) this.crtMotion();
      this.bloom.getContext("2d").drawImage(this.canvas, 0, 0, this.bloom.width, this.bloom.height);
    }
    var spent = performance.now() - started;
    this.stats.draws += 1;
    this.stats.drawMs += spent;
    this.stats.maxMs = Math.max(this.stats.maxMs, spent);
  };

  Host.prototype.drawScene = function () {
    this.scene.draw(this.ctx, {
      width: this.canvas.width,
      height: this.canvas.height,
      pitch: pitchOf(this.scene),
      time: this.time,
      navigating: still() ? 0 : this.navigating,
      palette: this.themed.palette,
      dark: this.themed.dark,
      reducedMotion: still(),
      options: this.optionValues(),
      state: this.state,
    });
  };

  // Frames only while the canvas is on screen, the tab is shown, the window
  // has focus and the reader has not asked for less motion: the Start page's
  // own rule.
  Host.prototype.running = function () {
    return (
      this.clock === "time" &&
      this.visible &&
      !document.hidden &&
      (document.hasFocus() || window.OmawebScenes.ignoreFocus) &&
      !still() &&
      this.scene &&
      this.scene.animated !== false
    );
  };

  Host.prototype.update = function () {
    var self = this;
    // The glass's own motion stops with the Scene's.
    this.canvas.parentElement.classList.toggle("scene-still", !this.running());
    if (this.running()) {
      if (this.frame) return;
      this.last = performance.now();
      // A Scene may cap its frame rate at what its motion needs.
      var gap = self.scene.fps ? 1000 / self.scene.fps - 2 : 0;
      var tick = function (now) {
        if (now - self.last >= gap) {
          self.time += Math.min(0.1, (now - self.last) / 1000);
          self.last = now;
          self.draw();
        }
        self.frame = self.running() ? requestAnimationFrame(tick) : 0;
      };
      this.frame = requestAnimationFrame(tick);
    } else if (this.frame) {
      cancelAnimationFrame(this.frame);
      this.frame = 0;
    }
  };

  function updateAll() {
    hosts.forEach(function (host) {
      host.update();
    });
  }
  document.addEventListener("visibilitychange", updateAll);
  addEventListener("focus", updateAll);
  addEventListener("blur", updateAll);
  [reduceQuery, phoneQuery].forEach(function (query) {
    query.addEventListener("change", function () {
      hosts.forEach(function (host) {
        host.state = {};
        host.draw();
        host.update();
      });
    });
  });

  window.OmawebScenes = {
    register: register,
    list: function () {
      return registry.slice();
    },
    Host: Host,
    hosts: hosts,
    retheme: function () {
      hosts.forEach(function (host) {
        host.retheme();
      });
    },
    use: function (id) {
      hosts.forEach(function (host) {
        host.setScene(id);
      });
    },
  };
})();
