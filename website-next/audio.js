// Night radio: a late-night drive theme, synthesized live with the Web Audio API rather than
// played from a file. Nothing here makes a sound or builds a graph until `start()` is called from
// a reader's click or key press, which is also what browsers require before a page may play.
//
// D minor at 88 BPM, on a sixteenth-note clock. Four chords of two bars each carry warm detuned
// pads; under them a restrained sub bass and a soft drum machine; over them a sparse arpeggio
// through a dotted-eighth delay, and now and then a distant lead phrase. Tape hiss, mains hum and
// the odd crackle sit at the bottom. The parts are generated as they play, with a little chance in
// the arpeggio, the hats and the lead, so the loop has no seam and never repeats exactly.

window.OmawebRadio = () => {
  const BPM = 88;
  const STEP = 60 / BPM / 4;
  const STEPS_PER_CHORD = 32;

  // MIDI notes. Each chord names its bass root and the voicing the pads and arpeggio use.
  const CHORDS = [
    { root: 38, notes: [50, 53, 57, 60, 64] }, // Dm9
    { root: 34, notes: [46, 50, 53, 57, 60] }, // Bbmaj9
    { root: 31, notes: [50, 53, 55, 58, 62] }, // Gm7 over D
    { root: 33, notes: [52, 55, 57, 62, 64] }, // A7sus4
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

    // The pads share one filter, which a very slow LFO opens and closes.
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
    pads.connect(padFilter).connect(master);
    const padSend = context.createGain();
    padSend.gain.value = 0.7;
    padFilter.connect(padSend).connect(reverb);

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

    graph = { master, analyser, reverb, delay, pads, noise, wobble };
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
        oscillator.connect(gain).connect(graph.pads);
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
    low.connect(gain).connect(graph.master);
  }

  function kick(time) {
    const oscillator = context.createOscillator();
    oscillator.frequency.setValueAtTime(115, time);
    oscillator.frequency.exponentialRampToValueAtTime(42, time + 0.14);
    const gain = context.createGain();
    gain.gain.setValueAtTime(0.75, time);
    gain.gain.exponentialRampToValueAtTime(0.0001, time + 0.5);
    oscillator.connect(gain).connect(graph.master);
    oscillator.start(time);
    oscillator.stop(time + 0.55);
    // The pads breathe out of the kick's way.
    graph.pads.gain.setTargetAtTime(0.72, time, 0.01);
    graph.pads.gain.setTargetAtTime(1, time + 0.06, 0.18);
  }

  function burst(time, type, hz, q, peak, decay, reverbAmount) {
    const source = context.createBufferSource();
    source.buffer = graph.noise;
    const filter = context.createBiquadFilter();
    filter.type = type;
    filter.frequency.value = hz;
    filter.Q.value = q;
    const gain = context.createGain();
    gain.gain.setValueAtTime(peak, time);
    gain.gain.exponentialRampToValueAtTime(0.0001, time + decay);
    source.connect(filter).connect(gain).connect(graph.master);
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
    tone.connect(gain).connect(graph.master);
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
    send(gain, graph.master, 0.65);
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
      send(gain, graph.master, 0.35);
      send(gain, graph.reverb, 1);
      send(gain, graph.delay, 0.4);
      for (const node of [oscillator, vibrato]) {
        node.start(at);
        node.stop(end);
      }
      at += length + STEP * Math.floor(Math.random() * 3);
    }
  }

  function schedule(index, time) {
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

  function tick() {
    // A hidden tab runs its timers once a second at most, so look further ahead there.
    const ahead = document.hidden ? 1.5 : 0.15;
    while (nextTime < context.currentTime + ahead) {
      schedule(step++, nextTime);
      nextTime += STEP;
    }
  }

  return {
    get playing() {
      return playing;
    },
    start() {
      if (!context) build();
      clearTimeout(stopping);
      context.resume();
      if (!timer) {
        nextTime = context.currentTime + 0.1;
        timer = setInterval(tick, 25);
        tick();
      }
      const now = context.currentTime;
      graph.master.gain.cancelScheduledValues(now);
      graph.master.gain.setTargetAtTime(0.6, now, 0.9);
      playing = true;
    },
    stop() {
      if (!context) return;
      playing = false;
      graph.master.gain.cancelScheduledValues(context.currentTime);
      graph.master.gain.setTargetAtTime(0, context.currentTime, 0.5);
      stopping = setTimeout(() => {
        clearInterval(timer);
        timer = 0;
        context.suspend();
      }, 2500);
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
