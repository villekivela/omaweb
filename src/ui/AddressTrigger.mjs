// What the address trigger shows of a page: its host, without `www.` or
// credentials, and its port if it has one. A local file or one of Omaweb's own
// pages has no host and shows the last part of its path, and any other page
// with none, such as `data:`, its scheme (#779).

export function shown(url) {
  const address = String(url);
  const parts = /^([a-z][a-z0-9+.-]*):(?:\/\/([^/?#]*))?([^?#]*)/i.exec(address);
  if (!parts) return address;
  const host = (parts[2] || "").replace(/^[^@]*@/, "").replace(/^www\./i, "");
  if (host.length > 0) return host;
  const scheme = parts[1].toLowerCase();
  if (scheme !== "file" && scheme !== "omaweb") return scheme;
  const segments = parts[3].split("/").filter((segment) => segment.length > 0);
  if (segments.length === 0) return scheme;
  const last = segments[segments.length - 1];
  // A path Qt left with a stray `%` is shown as it stands.
  try {
    return decodeURIComponent(last);
  } catch (error) {
    return last;
  }
}
