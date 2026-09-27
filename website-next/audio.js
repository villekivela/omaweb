// Night radio: late-night drive music, synthesized live with the Web Audio API rather than played
// from a file. Nothing here makes a sound or builds a graph until `start()` is called from a
// reader's click or key press, which is also what browsers require before a page may play.
//
// It is a radio with two stations, each a generated arrangement on a sixteenth-note clock, and
// `tune()` steps through them, through a burst of static, and then off:
//
// - 102.4, Overpass. A minor at 102 BPM: a rolling sixteenth bass, four on the floor with a gated
//   snare, and a 64-bar arrangement with a build, a drop, a breakdown and a return. It has no
//   lead yet; the groove carries it.
// - 88.0, Night drive. D minor at 88 BPM and quieter under the same sky: warm pads, a restrained
//   sub bass, a soft drum machine, a sparse electric-piano arpeggio and now and then a distant
//   lead phrase.
//
// Tape hiss, mains hum and the odd crackle sit under both. Each station plays on a bus of its own,
// so tuning away fades what the last one still had ringing rather than leaving it under the next.

window.OmawebRadio = () => {
  let STEP = 60 / 88 / 4;
  const STEPS_PER_CHORD = 32;

  // MIDI notes. Each chord names its bass root and the voicing the pads and arpeggio use.
  const NIGHT_DRIVE = [
    { root: 38, notes: [50, 53, 57, 60, 64] }, // Dm9
    { root: 34, notes: [46, 50, 53, 57, 60] }, // Bbmaj9
    { root: 31, notes: [50, 53, 55, 58, 62] }, // Gm7 over D
    { root: 33, notes: [52, 55, 57, 62, 64] }, // A7sus4
  ];
  const OVERPASS = [
    { root: 33, notes: [57, 60, 64, 67, 71] }, // Am9
    { root: 29, notes: [53, 57, 60, 64, 67] }, // Fmaj9
    { root: 38, notes: [50, 53, 57, 60, 64] }, // Dm9
    { root: 28, notes: [52, 57, 59, 62, 64] }, // E7sus4
  ];
  const LEAD_SCALE = [62, 65, 67, 69, 72, 74, 77];
  const ARP_MASK = [1, 0, 0, 1, 0, 0, 1, 0, 1, 0, 0, 1, 0, 1, 0, 0];

  const frequency = (note) => 440 * 2 ** ((note - 69) / 12);

  let context = null;
  let graph = null;
  let timer = 0;
  let step = 0;
  let nextTime = 0;
  let arpIndex = 0;
  let playing = false;
  let station = 0;
  let bus = null;
  let stopping = 0;
  const level = new Float32Array(1024);

  function noiseBuffer(seconds) {
    const buffer = context.createBuffer(1, context.sampleRate * seconds, context.sampleRate);
    const data = buffer.getChannelData(0);
    for (let i = 0; i < data.length; i++) data[i] = Math.random() * 2 - 1;
    return buffer;
  }

  // A dark hall: decaying noise, smoothed so the tail loses its highs as it fades.
  function impulse(seconds) {
    const length = context.sampleRate * seconds;
    const buffer = context.createBuffer(2, length, context.sampleRate);
    for (let channel = 0; channel < 2; channel++) {
      const data = buffer.getChannelData(channel);
      let smooth = 0;
      for (let i = 0; i < length; i++) {
        const fade = (1 - i / length) ** 3;
        smooth += (Math.random() * 2 - 1 - smooth) * (0.5 - 0.42 * (i / length));
        data[i] = smooth * fade;
      }
    }
    return buffer;
  }

  function build() {
    context = new AudioContext();
    const master = context.createGain();
    master.gain.value = 0;
    const compressor = context.createDynamicsCompressor();
    compressor.threshold.value = -20;
    compressor.ratio.value = 3;
    const analyser = context.createAnalyser();
    analyser.fftSize = 1024;
    master.connect(compressor).connect(context.destination);
    master.connect(analyser);

    const reverb = context.createConvolver();
    reverb.buffer = impulse(4.5);
    const reverbReturn = context.createGain();
    reverbReturn.gain.value = 0.8;
    reverb.connect(reverbReturn).connect(master);

    // A dotted-eighth echo, darkened on every repeat.
    const delay = context.createDelay(2);
    delay.delayTime.value = STEP * 3;
    const feedback = context.createGain();
    feedback.gain.value = 0.38;
    const darken = context.createBiquadFilter();
    darken.type = "lowpass";
    darken.frequency.value = 2000;
    delay.connect(darken).connect(feedback).connect(delay);
    const delayReturn = context.createGain();
    delayReturn.gain.value = 0.35;
    darken.connect(delayReturn);
    delayReturn.connect(master);
    delayReturn.connect(reverb);

    const noise = noiseBuffer(2);

    // The room tone: tape hiss, mains hum and its first harmonic.
    const hiss = context.createBufferSource();
    hiss.buffer = noise;
    hiss.loop = true;
    const hissHigh = context.createBiquadFilter();
    hissHigh.type = "highpass";
    hissHigh.frequency.value = 3500;
    const hissGain = context.createGain();
    hissGain.gain.value = 0.007;
    hiss.connect(hissHigh).connect(hissGain).connect(master);
    hiss.start();
    for (const [hz, gain] of [
      [60, 0.004],
      [120, 0.0018],
    ]) {
      const hum = context.createOscillator();
      hum.frequency.value = hz;
      const humGain = context.createGain();
      humGain.gain.value = gain;
      hum.connect(humGain).connect(master);
      hum.start();
    }

    // Tape wobble: two slow, uneven drifts of pitch shared by every pad voice, the way a worn
    // cassette pulls the whole chord flat and sharp together rather than each note on its own.
    const wobble = context.createGain();
    wobble.gain.value = 1;
    for (const [hz, cents] of [
      [0.23, 9],
      [0.07, 6],
    ]) {
      const drift = context.createOscillator();
      drift.frequency.value = hz;
      const depth = context.createGain();
      depth.gain.value = cents;
      drift.connect(depth).connect(wobble);
      drift.start();
    }

    graph = { master, analyser, reverb, delay, noise, wobble, hall: impulse(1.2) };
  }

  // A station's own bus. Everything it plays goes out through `out`; the pads and bass go through
  // `pump` first, which the kick ducks, so they breathe out of its way. The pads share one filter,
  // which a very slow LFO opens and closes.
  function makeBus() {
    const out = context.createGain();
    out.connect(graph.master);
    const pump = context.createGain();
    pump.connect(out);
    const pads = context.createGain();
    const padFilter = context.createBiquadFilter();
    padFilter.type = "lowpass";
    padFilter.frequency.value = 850;
    padFilter.Q.value = 0.4;
    const lfo = context.createOscillator();
    lfo.frequency.value = 0.045;
    const lfoDepth = context.createGain();
    lfoDepth.gain.value = 300;
    lfo.connect(lfoDepth).connect(padFilter.frequency);
    lfo.start();
    pads.connect(padFilter).connect(pump);
    const padSend = context.createGain();
    padSend.gain.value = 0.7;
    padFilter.connect(padSend).connect(graph.reverb);
    return { out, pump, pads, lfo };
  }

  function retire(old, time) {
    old.out.gain.setTargetAtTime(0, time, 0.12);
    setTimeout(() => {
      old.lfo.stop();
      old.out.disconnect();
    }, 1500);
  }

  function envelope(gain, time, peak, attack, hold, release) {
    gain.setValueAtTime(0.0001, time);
    gain.linearRampToValueAtTime(peak, time + attack);
    gain.setValueAtTime(peak, time + attack + hold);
    gain.exponentialRampToValueAtTime(0.0001, time + attack + hold + release);
    return time + attack + hold + release + 0.05;
  }

  function send(node, target, amount) {
    const gain = context.createGain();
    gain.gain.value = amount;
    node.connect(gain).connect(target);
  }

  function pad(chord, time) {
    const length = STEPS_PER_CHORD * STEP;
    // Soft voices: a triangle and a sine a hair apart, so each note is warm and round rather
    // than buzzy, with a slow swell in and a long fade out.
    for (const note of chord.notes) {
      for (const [type, detune, level] of [
        ["triangle", -4, 0.02],
        ["sine", 5, 0.024],
      ]) {
        const oscillator = context.createOscillator();
        oscillator.type = type;
        oscillator.frequency.value = frequency(note);
        oscillator.detune.value = detune + (Math.random() * 3 - 1.5);
        graph.wobble.connect(oscillator.detune);
        oscillator.onended = () => graph.wobble.disconnect(oscillator.detune);
        const gain = context.createGain();
        const end = envelope(gain.gain, time, level, 2.4, length - 1.6, 3.5);
        oscillator.connect(gain).connect(bus.pads);
        oscillator.start(time);
        oscillator.stop(end);
      }
    }
  }

  // A deep, restrained bass in the chord's own low octave, 45 to 75 Hz: a sine for the weight and
  // a sawtooth through a very low filter for a little warmth, so it is felt under the pads rather
  // than heard as a tune. A soft attack keeps it from thumping.
  function bass(chord, time, steps) {
    const hz = frequency(chord.root);
    const low = context.createBiquadFilter();
    low.type = "lowpass";
    low.frequency.setValueAtTime(420, time);
    low.frequency.exponentialRampToValueAtTime(160, time + 0.35);
    low.Q.value = 0.5;
    const gain = context.createGain();
    const end = envelope(gain.gain, time, 0.3, 0.03, steps * STEP * 0.75, 0.35);
    for (const [type, level] of [
      ["sine", 1],
      ["sawtooth", 0.18],
    ]) {
      const oscillator = context.createOscillator();
      oscillator.type = type;
      oscillator.frequency.value = hz;
      const mix = context.createGain();
      mix.gain.value = level;
      oscillator.connect(mix).connect(low);
      oscillator.start(time);
      oscillator.stop(end);
    }
    low.connect(gain).connect(bus.pump);
  }

  function kick(time) {
    const oscillator = context.createOscillator();
    oscillator.frequency.setValueAtTime(115, time);
    oscillator.frequency.exponentialRampToValueAtTime(42, time + 0.14);
    const gain = context.createGain();
    gain.gain.setValueAtTime(0.75, time);
    gain.gain.exponentialRampToValueAtTime(0.0001, time + 0.5);
    oscillator.connect(gain).connect(bus.out);
    oscillator.start(time);
    oscillator.stop(time + 0.55);
    duck(time, 0.72, 0.18);
  }

  function duck(time, depth, recovery) {
    bus.pump.gain.setTargetAtTime(depth, time, 0.01);
    bus.pump.gain.setTargetAtTime(1, time + 0.06, recovery);
  }

  function burst(time, type, hz, q, peak, decay, reverbAmount, target = bus.out) {
    const source = context.createBufferSource();
    source.buffer = graph.noise;
    const filter = context.createBiquadFilter();
    filter.type = type;
    filter.frequency.value = hz;
    filter.Q.value = q;
    const gain = context.createGain();
    gain.gain.setValueAtTime(peak, time);
    gain.gain.exponentialRampToValueAtTime(0.0001, time + decay);
    source.connect(filter).connect(gain).connect(target);
    if (reverbAmount) send(gain, graph.reverb, reverbAmount);
    source.start(time, Math.random() * 1.5);
    source.stop(time + decay + 0.02);
  }

  function snare(time) {
    burst(time, "bandpass", 1700, 0.8, 0.16, 0.2, 0.6);
    const tone = context.createOscillator();
    tone.type = "triangle";
    tone.frequency.value = 185;
    const gain = context.createGain();
    gain.gain.setValueAtTime(0.09, time);
    gain.gain.exponentialRampToValueAtTime(0.0001, time + 0.09);
    tone.connect(gain).connect(bus.out);
    tone.start(time);
    tone.stop(time + 0.1);
  }

  // A muted electric piano, in FM: a sine modulating a sine at the same pitch, its depth falling
  // fast so each note starts with a soft bell and mellows as it rings, a faint high tine for the
  // ping, a low-pass for the felt, and a short decay, like a key damped by the palm.
  function arp(chord, time) {
    const hz = frequency(chord.notes[arpIndex++ % chord.notes.length] + 12);
    const velocity = 0.75 + Math.random() * 0.25;
    const decay = 0.7;

    const carrier = context.createOscillator();
    carrier.frequency.value = hz;
    const modulator = context.createOscillator();
    modulator.frequency.value = hz;
    const depth = context.createGain();
    depth.gain.setValueAtTime(hz * 2.4 * velocity, time);
    depth.gain.exponentialRampToValueAtTime(hz * 0.25, time + 0.3);
    modulator.connect(depth).connect(carrier.frequency);

    const tine = context.createOscillator();
    tine.frequency.value = hz * 7;
    const tineGain = context.createGain();
    tineGain.gain.setValueAtTime(0.012 * velocity, time);
    tineGain.gain.exponentialRampToValueAtTime(0.0001, time + 0.07);

    const felt = context.createBiquadFilter();
    felt.type = "lowpass";
    felt.frequency.value = 1500;
    felt.Q.value = 0.3;
    const gain = context.createGain();
    gain.gain.setValueAtTime(0.0001, time);
    gain.gain.linearRampToValueAtTime(0.07 * velocity, time + 0.004);
    gain.gain.exponentialRampToValueAtTime(0.0001, time + decay);

    carrier.connect(felt);
    tine.connect(tineGain).connect(felt);
    felt.connect(gain);
    send(gain, bus.out, 0.65);
    send(gain, graph.delay, 0.75);
    send(gain, graph.reverb, 0.45);
    for (const node of [carrier, modulator, tine]) {
      node.start(time);
      node.stop(time + decay + 0.05);
    }
  }

  // A few long notes, far away: slow in, vibrato, most of it in the reverb.
  function lead(time) {
    let at = time;
    let index = 2 + Math.floor(Math.random() * 3);
    const count = 3 + Math.floor(Math.random() * 2);
    for (let n = 0; n < count; n++) {
      index = Math.max(
        0,
        Math.min(LEAD_SCALE.length - 1, index + [-2, -1, 1, 2][Math.floor(Math.random() * 4)]),
      );
      const length = STEP * (6 + Math.floor(Math.random() * 4));
      const oscillator = context.createOscillator();
      oscillator.type = "sine";
      oscillator.frequency.value = frequency(LEAD_SCALE[index]);
      const vibrato = context.createOscillator();
      vibrato.frequency.value = 4.6;
      const depth = context.createGain();
      depth.gain.value = 7;
      vibrato.connect(depth).connect(oscillator.detune);
      const filter = context.createBiquadFilter();
      filter.type = "lowpass";
      filter.frequency.value = 1500;
      const gain = context.createGain();
      const end = envelope(gain.gain, at, 0.05, 0.35, length - 0.35, 1.4);
      oscillator.connect(filter).connect(gain);
      send(gain, bus.out, 0.35);
      send(gain, graph.reverb, 1);
      send(gain, graph.delay, 0.4);
      for (const node of [oscillator, vibrato]) {
        node.start(at);
        node.stop(end);
      }
      at += length + STEP * Math.floor(Math.random() * 3);
    }
  }

  function nightDrive(index, time) {
    const CHORDS = NIGHT_DRIVE;
    const inBar = index % 16;
    const bar = Math.floor(index / 16);
    const chord = CHORDS[Math.floor(index / STEPS_PER_CHORD) % CHORDS.length];
    // A little swing on the off sixteenths.
    const swung = inBar % 2 ? time + STEP * 0.12 : time;

    if (index % STEPS_PER_CHORD === 0) pad(chord, time);
    if (inBar === 0) bass(chord, time, 6);
    if (inBar === 7) bass(chord, swung, 2);
    if (inBar === 10) bass(chord, time, 5);

    if (bar >= 2) {
      if (inBar === 0 || inBar === 8 || (inBar === 14 && bar % 4 === 3)) kick(time);
      if (inBar === 4 || inBar === 12) snare(time);
      if (inBar % 4 === 2) burst(swung, "highpass", 7500, 0.5, 0.05, 0.05, 0);
      else if (inBar % 2 && Math.random() < 0.45)
        burst(swung, "highpass", 8000, 0.5, 0.018, 0.03, 0);
      if (inBar === 14 && bar % 8 === 7) burst(swung, "highpass", 6500, 0.5, 0.035, 0.35, 0.3);
    }
    if (bar >= 4 && ARP_MASK[inBar] && Math.random() < 0.85) arp(chord, swung);
    if (bar >= 8 && index % (STEPS_PER_CHORD * 4) === STEPS_PER_CHORD && Math.random() < 0.55) {
      lead(time + STEP * 2);
    }
    // The odd crackle of a radio just off station.
    if (Math.random() < 0.025)
      burst(time + Math.random() * STEP, "bandpass", 2800, 2, 0.03, 0.006, 0);
  }

  // --- Overpass -------------------------------------------------------------------------------

  // A harder kick: a longer pitch drop and a click on top, and a deeper duck.
  function bigKick(time) {
    const body = context.createOscillator();
    body.frequency.setValueAtTime(150, time);
    body.frequency.exponentialRampToValueAtTime(45, time + 0.12);
    const gain = context.createGain();
    gain.gain.setValueAtTime(0.95, time);
    gain.gain.exponentialRampToValueAtTime(0.0001, time + 0.42);
    body.connect(gain).connect(bus.out);
    body.start(time);
    body.stop(time + 0.45);
    burst(time, "highpass", 3000, 0.7, 0.05, 0.012, 0);
    duck(time, 0.45, 0.12);
  }

  // The big snare: a noise crack and a body, most of it sent into the hall and then cut short, the
  // way a gated reverb stops dead.
  function gatedSnare(time, peak = 1) {
    const gate = context.createGain();
    gate.gain.setValueAtTime(1, time);
    gate.gain.setValueAtTime(1, time + 0.2);
    gate.gain.linearRampToValueAtTime(0, time + 0.26);
    const hall = context.createConvolver();
    hall.buffer = graph.hall;
    hall.connect(gate).connect(bus.out);
    const source = context.createBufferSource();
    source.buffer = graph.noise;
    const band = context.createBiquadFilter();
    band.type = "bandpass";
    band.frequency.value = 1900;
    band.Q.value = 0.7;
    const crack = context.createGain();
    crack.gain.setValueAtTime(0.28 * peak, time);
    crack.gain.exponentialRampToValueAtTime(0.0001, time + 0.18);
    source.connect(band).connect(crack);
    crack.connect(bus.out);
    crack.connect(hall);
    source.start(time, Math.random());
    source.stop(time + 0.3);
    const tone = context.createOscillator();
    tone.type = "triangle";
    tone.frequency.setValueAtTime(220, time);
    tone.frequency.exponentialRampToValueAtTime(160, time + 0.08);
    const toneGain = context.createGain();
    toneGain.gain.setValueAtTime(0.14 * peak, time);
    toneGain.gain.exponentialRampToValueAtTime(0.0001, time + 0.12);
    tone.connect(toneGain);
    toneGain.connect(bus.out);
    toneGain.connect(hall);
    tone.start(time);
    tone.stop(time + 0.14);
  }

  // A rolling bass note: sawtooth and square an octave apart, through a filter that snaps shut,
  // short enough that sixteenths of it roll rather than drone.
  function rollingBass(note, time, accent) {
    const hz = frequency(note);
    const low = context.createBiquadFilter();
    low.type = "lowpass";
    low.Q.value = 4;
    low.frequency.setValueAtTime(accent ? 1400 : 900, time);
    low.frequency.exponentialRampToValueAtTime(180, time + 0.11);
    const gain = context.createGain();
    gain.gain.setValueAtTime(0.0001, time);
    gain.gain.linearRampToValueAtTime(accent ? 0.2 : 0.15, time + 0.004);
    gain.gain.exponentialRampToValueAtTime(0.0001, time + STEP * 0.95);
    for (const [type, octave, level] of [
      ["sawtooth", 1, 1],
      ["square", 0.5, 0.5],
    ]) {
      const oscillator = context.createOscillator();
      oscillator.type = type;
      oscillator.frequency.value = hz * octave;
      const mix = context.createGain();
      mix.gain.value = level;
      oscillator.connect(mix).connect(low);
      oscillator.start(time);
      oscillator.stop(time + STEP + 0.02);
    }
    low.connect(gain).connect(bus.pump);
  }

  // A noise sweep up into a section, over one bar.
  function riser(time) {
    const length = STEP * 16;
    const source = context.createBufferSource();
    source.buffer = graph.noise;
    source.loop = true;
    const band = context.createBiquadFilter();
    band.type = "bandpass";
    band.Q.value = 3;
    band.frequency.setValueAtTime(300, time);
    band.frequency.exponentialRampToValueAtTime(7000, time + length);
    const gain = context.createGain();
    gain.gain.setValueAtTime(0.0001, time);
    gain.gain.exponentialRampToValueAtTime(0.09, time + length);
    gain.gain.linearRampToValueAtTime(0, time + length + 0.02);
    source.connect(band).connect(gain).connect(bus.out);
    send(gain, graph.reverb, 0.4);
    source.start(time);
    source.stop(time + length + 0.05);
  }

  function overpass(index, time) {
    const inBar = index % 16;
    const bar = Math.floor(index / 16) % 64;
    const chord = OVERPASS[Math.floor(index / STEPS_PER_CHORD) % OVERPASS.length];
    const swung = inBar % 2 ? time + STEP * 0.06 : time;
    // Sections of the 64-bar loop: intro, build, drop, breakdown, return, cool-down.
    const intro = bar < 8;
    const build = bar >= 8 && bar < 16;
    const drop = (bar >= 16 && bar < 32) || (bar >= 40 && bar < 56);
    const breakdown = bar >= 32 && bar < 40;
    const drums = build || drop || bar >= 56;

    if (index % STEPS_PER_CHORD === 0) pad(chord, time);

    // The bass rolls in sixteenths once the drums are in, with the octave on every off-beat
    // eighth; in the breakdown it holds long notes instead.
    if (drums) {
      const octave = inBar % 4 === 2;
      rollingBass(chord.root + 12 + (octave ? 12 : 0), swung, inBar % 4 === 0);
    } else if (breakdown && inBar === 0) {
      bass(chord, time, 14);
    }

    if (drums) {
      if (inBar % 4 === 0) bigKick(time);
      if ((inBar === 4 || inBar === 12) && (bar >= 12 || drop)) gatedSnare(time);
      if (inBar % 4 === 2) burst(swung, "highpass", 7000, 0.6, 0.07, 0.11, 0.1);
      else if (inBar % 2) burst(swung, "highpass", 9000, 0.5, 0.025, 0.03, 0);
    }
    // A snare roll into the drop and the return, growing to the downbeat.
    if ((bar === 15 || bar === 39) && inBar >= 8) gatedSnare(time, 0.35 + (inBar - 8) * 0.08);
    if ((bar === 15 || bar === 39) && inBar === 0) riser(time);
    if ((bar === 16 || bar === 40) && inBar === 0) {
      burst(time, "highpass", 5000, 0.5, 0.16, 1.8, 0.9);
    }

    if ((intro || breakdown || drop) && ARP_MASK[inBar]) arp(chord, swung);

    if (Math.random() < 0.015)
      burst(time + Math.random() * STEP, "bandpass", 2800, 2, 0.03, 0.006, 0);
  }

  // In the order the button tunes through them: the first is what a first press plays.
  const STATIONS = [
    { name: "102.4 Overpass", bpm: 102, play: overpass },
    { name: "88.0 Night drive", bpm: 88, play: nightDrive },
  ];

  // Between stations: a sweep of static, and the new one starts as it clears.
  function staticBurst(time) {
    const source = context.createBufferSource();
    source.buffer = graph.noise;
    const band = context.createBiquadFilter();
    band.type = "bandpass";
    band.Q.value = 1.2;
    band.frequency.setValueAtTime(900, time);
    band.frequency.exponentialRampToValueAtTime(4200, time + 0.25);
    band.frequency.exponentialRampToValueAtTime(1600, time + 0.5);
    const gain = context.createGain();
    gain.gain.setValueAtTime(0.0001, time);
    gain.gain.linearRampToValueAtTime(0.12, time + 0.05);
    gain.gain.exponentialRampToValueAtTime(0.0001, time + 0.55);
    source.connect(band).connect(gain).connect(graph.master);
    source.start(time, Math.random());
    source.stop(time + 0.6);
  }

  function tuneTo(index) {
    const now = context.currentTime;
    if (bus) {
      retire(bus, now);
      staticBurst(now);
    }
    station = index;
    STEP = 60 / STATIONS[index].bpm / 4;
    graph.delay.delayTime.setValueAtTime(STEP * 3, now);
    bus = makeBus();
    step = 0;
    arpIndex = 0;
    nextTime = now + (playing ? 0.45 : 0.1);
  }

  function tick() {
    // A hidden tab runs its timers once a second at most, so look further ahead there.
    const ahead = document.hidden ? 1.5 : 0.15;
    while (nextTime < context.currentTime + ahead) {
      STATIONS[station].play(step++, nextTime);
      nextTime += STEP;
    }
  }

  function start(index = 0) {
    if (!context) build();
    clearTimeout(stopping);
    context.resume();
    if (!bus || station !== index || !timer) tuneTo(index);
    if (!timer) timer = setInterval(tick, 25);
    tick();
    const now = context.currentTime;
    graph.master.gain.cancelScheduledValues(now);
    graph.master.gain.setTargetAtTime(0.6, now, 0.9);
    playing = true;
  }

  function stop() {
    if (!context) return;
    playing = false;
    graph.master.gain.cancelScheduledValues(context.currentTime);
    graph.master.gain.setTargetAtTime(0, context.currentTime, 0.5);
    stopping = setTimeout(() => {
      clearInterval(timer);
      timer = 0;
      if (bus) retire(bus, context.currentTime);
      bus = null;
      context.suspend();
    }, 2500);
  }

  return {
    get playing() {
      return playing;
    },
    get station() {
      return playing ? STATIONS[station].name : "";
    },
    start,
    stop,
    // Off, then each station in turn, then off again: the one button a car radio has.
    tune() {
      if (!playing) start(0);
      else if (station < STATIONS.length - 1) tuneTo(station + 1);
      else stop();
      return this.station;
    },
    // How loud it is right now, from 0 to about 1, for the screen to glow with.
    level() {
      if (!graph) return 0;
      graph.analyser.getFloatTimeDomainData(level);
      let sum = 0;
      for (const sample of level) sum += sample * sample;
      return Math.min(1, Math.sqrt(sum / level.length) * 3);
    },
  };
};
