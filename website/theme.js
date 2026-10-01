// The theme the reader picked on the landing page, on every page of the site, applied in the head
// so the page paints in it from the start. A convenience: with storage blocked or empty the page
// wears the theme its markup names.
try {
  const picked = localStorage.getItem("omaweb-theme");
  if (picked && /^[a-z0-9-]+$/.test(picked)) document.documentElement.dataset.theme = picked;
} catch {
  // Storage can be blocked, as in some private windows.
}
