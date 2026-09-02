# Website design variants — PROTOTYPE

Throwaway. Directions for `website/index.html`, on one route.

    scripts/serve_website.sh
    open http://localhost:8000/prototype/

`?v=tanto | keys | bar | spaces | manifest`, or the switcher pinned to the
bottom. `?v=tanto` is the default and the only one still being worked on.

## The question

Which direction should the Tanto site take, in the Omarchy design language?

## Where it landed

**`?v=tanto` — the live direction.** Variant 2's look (`keys`) carrying variant
1's copy. Variants `keys`, `bar`, `spaces` and `manifest` are frozen as
reference; they still hold the older copy and its three factual errors, so do
not lift text out of them.

Its signature: **the product UI is HTML, drawn from the same palette tokens as
the page.** Picking a theme restyles the site and the browser together, which
is what `omarchy theme set` does to the real thing. There is no product
screenshot left to go stale.

The chrome follows `src/ui/SpaceOutline.qml` rather than approximating it:

- A controls row — `left_panel_close`, `search`, then back / forward / reload.
- The address field at `radius: 2`, with the lock in the same 18px slot a tab
  row gives its site chip so the address and every tab title start on one
  line, and the blocked-request `shield` plus its count on the right.
- Pinned tabs run 3–5 across as equal cells, each showing the two-letter site
  code the application derives from the host. This is what the YO / SP / DU / X
  row in the old screenshot actually was — not Spaces.
- Ordinary tabs as rows beneath, with the site chip.
- The footer carries the Space switcher as **one letter per Space**
  (`spaceName.charAt(0)`), then `more_horiz` and `settings`.
- Hiding the sidebar floats the navigation cluster over the page — back,
  forward, reload, a 1px rule, `left_panel_open`, `search` — at 0.55 opacity
  rising to 1 on hover, exactly as `NavigationCluster.qml` does. That cluster
  is how the sidebar comes back.

Other details worth keeping:

- The desktop spans the full viewport width and parallaxes against the window
  on scroll in **two layers at different rates** — the orb layer travels
  ~320px against the window, the stripe field ~135px, leaving ~185px between
  them. One layer is not enough: a repeating diagonal stripe field shifted
  along its own axis is indistinguishable from itself, so the first attempt
  measured correctly and looked completely static. The orbs are the landmark
  you can actually track. Skipped under `prefers-reduced-motion`.
- Both layers are generated from the theme's own `accent` / `urgent` / `fg`
  roles, so they re-theme for free, and are oversized with the scroll
  progress clamped to ±1 — the raw ratio overshoots at both ends and pushed a
  layer past its headroom, exposing a strip of bare background.
- The sidebar is translucent at the theme's `opacity.sidebar` (0.85) and
  blurred over that wallpaper. The page viewport stays opaque, because a
  webpage viewport is not a Transparent surface.
- The viewport shows the Start page answering over a live page, so it is
  opaque and closeable — which is why the address bar holds a real URL and a
  blocked count rather than the at-rest placeholder.
- Every binding shown comes from `assets/keybindings/default.json`, rendered
  with `Ctrl` rather than `⌘`.
- Hiding the sidebar is bound to `Ctrl B` as well as to both icons.

## Where the tokens come from

Every size, colour alpha and border width derives from
`third_party/omarchy-shell/qs/Commons/Style.qml` — `cornerRadius: 0`,
`fontFamily: "monospace"` system-wide, the type scale as multiples of one
`base-size` rem root, the kit's spacing steps, fills at fg·0.04 / 0.08 / 0.18,
borders at fg·0.40 (hover drops to 0.25), `gapsOut`, and a 26–28px bar. Per
[ADR 0017](../../docs/adr/0017-vendor-the-omarchy-component-kit.md) the kit
wins, so there is no filled button here either: emphasis is bold + tint +
accent border.

One documented extension: `--fs-poster`, at 2x the kit's `display-large`,
because a scale that tops out at 28px for a 12px bar has no hero step.

## Icons

`website/assets/fonts/material-symbols-subset.woff2` (1.4 kB) is built from the
vendored 15 MB variable font by:

    scripts/build_website_icon_font.py

It resolves each codepoint out of the font's own ligature table rather than
hard-coding them, because the PUA assignments are not guessable — `lock` is
U+E88D and `shield` is U+E75B. The icon names match what the application asks
for in `src/ui/*.qml`, so the site and the browser cannot drift. Add a name to
`ICONS` and re-run.

## Open questions for the next pass

- **Theme palettes are approximations.** The six presets were written by hand.
  Regenerate them from the real `colors.toml` each Omarchy theme ships, and
  from `integrations/omarchy/tanto.json.tpl`, before this goes live.
- **Platform copy.** macOS appears nowhere. The status section keeps its three
  columns and its third one is now "Linux is coming". Note this is a
  positioning decision, not what the repo's own `README.md` says today — that
  still describes the Qt build as running on macOS first.
- **"Space" versus "profile".** `CONTEXT.md` makes Space the domain term and
  tells us to avoid "profile". The copy keeps Space as the noun and explains it
  as one person's isolated accounts, never as different people. Worth a second
  read.
- The `keys` variant's `f` link hints and `⌘K` Omnibar were dropped from the
  live direction; the theme switcher and the product UI are its signature.
  They still work under `?v=keys` if you want them back.
- `lock_open` and `domino_mask` are in the application but not on the page
  yet. Add them to `ICONS` in the build script if a not-secure state or a
  Private window ever needs drawing.

## When this is done

Fold the chosen direction into `website/`, then delete this directory. Do not
grow it into the real site.
