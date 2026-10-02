// The introduction's controls: what they show for the film's time, where a press on the scrubber
// seeks to, and what a key does. `drive.js` wires them to the video.

// Minutes and seconds, as a player's clock reads.
const clock = (seconds) => {
  const whole = Math.floor(seconds);
  return `${Math.floor(whole / 60)}:${String(whole % 60).padStart(2, "0")}`;
};

// The film loops, so a seek to its very end would start it again: a seek holds a tenth of a second
// short of it, on the last frame.
const held = (time, duration) => Math.min(Math.max(time, 0), Math.max(duration - 0.1, 0));

// What the controls show for the video's state.
export function filmState({ paused, currentTime, duration }) {
  const total = Number.isFinite(duration) ? duration : 0;
  const time = total ? Math.min(Math.max(currentTime, 0), total) : 0;
  return {
    playing: !paused,
    progress: total ? time / total : 0,
    elapsed: clock(time),
    total: clock(total),
    valueNow: Math.floor(time),
    valueMax: Math.floor(total),
    valueText: `${clock(time)} of ${clock(total)}`,
  };
}

// The time a press at `x` on the scrubber seeks to: its share of the track, held to the film.
export function seekAt(x, { left, width }, duration) {
  if (!Number.isFinite(duration) || !width) return 0;
  return held(((x - left) / width) * duration, duration);
}

// What a key does while the controls have focus: Space and K play or pause, as in most players;
// the arrows step five seconds, Page Up and Page Down a tenth of the film, Home and End go to its
// ends. Null for any other key, which is left to the page.
export function filmKey(key, { currentTime, duration }) {
  if (key === " " || key.toLowerCase() === "k") return { toggle: true };
  if (!Number.isFinite(duration) || !duration) return null;
  const steps = {
    ArrowLeft: -5,
    ArrowDown: -5,
    ArrowRight: 5,
    ArrowUp: 5,
    PageDown: -duration / 10,
    PageUp: duration / 10,
    Home: -duration,
    End: duration,
  };
  if (!(key in steps)) return null;
  return { seek: held(currentTime + steps[key], duration) };
}
