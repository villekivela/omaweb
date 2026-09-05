# Use a cross-platform theme palette

Omaweb loads its theme palette from a versioned JSON file and watches both the file and its parent
directory for changes on macOS and Linux. This survives theme managers that atomically replace a
generated directory.

On Omarchy Quattro, Omaweb ships a `omaweb.json.tpl` user template and installs it itself: Omarchy
is the platform Omaweb is built for, so following the desktop's theme is the default rather than
three commands the reader has to be told about. The first start on a machine that has Omarchy writes
the template to `~/.config/omarchy/themed/` and asks Omarchy to render the theme that is already
active. A template already there is never overwritten, because it is a customisation of the reader's
or of a theme's; the decision is logged when it differs from the shipped one.
`OMAWEB_NO_OMARCHY_TEMPLATE` stops the write for a configuration directory the reader generates or
tracks themselves. Omarchy renders the active file into
`~/.local/state/omarchy/current/theme/omaweb.json`, which Omaweb detects directly. Normal theme
switching does not depend on a synchronous `theme-set` hook or D-Bus call. An explicit non-blocking
reload command may remain as a fallback. This keeps QML independent of Omarchy and makes the same
palette contract testable on macOS.

A user may also place a `theme.json` in Omaweb's configuration directory, and
`scripts/import_terminal_theme.py` derives one from the colours of the terminal it runs in for
desktops that have no theme manager of their own. ADR 0016 records the lookup order between those
sources.

The palette defines semantic opacity values for Omaweb-owned surfaces such as the vertical sidebar,
address bar, overlays, and empty window background. Those surfaces request native background blur
where the platform or compositor provides it, fall back to alpha transparency, and then to an opaque
theme color. Webpage viewports remain opaque, and transparent surfaces continue to receive pointer
input.

Legibility is Omaweb's rather than the theme's. A palette derived from a terminal's sixteen colours
has no colour for quiet text — Omarchy offers `dark_foreground`, which its own templates spend on
`disabledForeground`, and for a theme naming none of the extended colours that is ANSI bright black,
the value Omaweb also draws borders in — so a theme is free to name muted text that reads fainter
than the disabled rendering of ordinary text, and most do. Omaweb changes only the named colour's
lightness until it clears 4.5:1 against every surface muted text is read on. A theme whose colour
already reads keeps it exactly; one that names none gets the quietest tint of its own text colour
that still reads, so a light theme stays light without being special-cased. Text composited at the
disabled opacity cannot reach 3:1 against its own ground, so one floor both keeps quiet text
readable and keeps it from reading as unavailable. Private surfaces may have the opposite luminance
from their ordinary counterparts, so the palette also carries a `privateMutedText` derived from the
same theme colour and `Main` selects it for a Private window. A Private overlay inherits
`privateSidebar` and keeps the ordinary overlay opacity, placing its text and borders on the same
luminance family as the rest of the Private palette.

Borders are graphical content. Omaweb applies the same rule at the WCAG AA 3:1 non-text threshold
against every surface a border separates. A named border keeps its hue while its lightness moves,
and a Private window receives a separately derived `privateBorder` so neither palette weakens the
other.

A rule drawn inside a surface is not a border around one, and the palette keeps them apart. The
vendored kit draws a panel divider as its foreground colour at a low alpha and a control's edge as
that same colour at a much higher one, so nothing the border role can hold reads as quietly as the
separators in the desktop's own bar. Omaweb's hairlines are those dividers — the seam down the
sidebar, the rule above a browsing identity, the bands across a panel — so the palette derives a
`separator` from the theme's text colour at the kit's own strength, and the browser draws them in
that rather than in the border colour. It is deliberately below every threshold above: a divider
that clears 3:1 is a frame, and a browser drawn in frames is the thing this avoids. A theme that
names `separator` itself keeps exactly what it named. Borders proper — the window's frame, a card's
edge — keep the border role and its floor.

Some palettes make a threshold unreachable: a Private window whose sidebar is the theme's accent and
whose background is the desktop's dark one has no colour that is 3:1 against both, and a theme is
entitled to name exactly that. That is a palette to honour rather than a broken theme, so the floor
is a repair and not a gate. Where no colour clears it against every surface at once, the role takes
whichever candidate reads best on the surface it reads worst on, and every other colour the theme
named stays exactly as it named it. Discarding a whole palette for one unreachable role would stop
the browser following the desktop at all, which costs the reader far more than the role does.

Every theme also defines a distinct Private-window treatment. A theme names one colour for it — the
private accent — and Omaweb derives the grounds that colour is cast over: each private ground is the
ordinary ground it stands in for, mixed towards the accent in OKLab by one small amount the whole
palette shares, keeping a little more chroma than the mix gives so a muted desktop's cast does not
wash out to the grey it was mixed from. A Private window is therefore the reader's own chrome
recognisably tinted rather than a palette of its own, and the private grounds keep the spacing the
theme gave the ordinary ones. Deriving each ground by mixing the window towards the accent instead —
a ladder of its own — was tried and rejected: every ground then climbs towards the accent's
lightness rather than the theme's, and a desktop whose window is nearly black gets a browser several
shades paler than everything around it. Neither `assets/themes/default.json` nor
`scripts/import_terminal_theme.py` names private grounds, so this is the one place they are decided.
A theme that does name one keeps it.

The difference a Private window keeps from an ordinary one is measured in OKLab, so a cast of colour
at the same lightness counts for what the reader sees; measured in RGB it barely counts at all near
a desktop's black, where the only way to move far enough is to make the private chrome paler than
the rest of the theme. The floor is deliberately low, because the ground is not the only thing
saying which window this is — the private accent, the mask on the sidebar and the window's own title
say it too. The tint strengthens where a theme's private accent is so close to its window that no
cast is visible at the shared amount. A named colour too close to its counterpart is replaced by the
tint, and where even that cannot be told apart, by that colour taken towards black or white only as
far as it has to go: a palette with no private hue to offer still has a lightness of its own, and a
white window on a dark desktop is not the reader's theme any more.

Alpha is not part of a colour a theme names. Semantic opacity is the single source of truth for how
much of the desktop shows through a Omaweb-owned surface, and an eight-digit hex colour is read as
`#AARRGGBB`, so a suffix meant as alpha displaces the colour instead of fading it.

System reduced-motion, increased-contrast, and reduced-transparency preferences override the active
palette. Transparent surfaces fall back to opaque colors when accessibility settings require it.
Keyboard commands update state without waiting for interface animations.
