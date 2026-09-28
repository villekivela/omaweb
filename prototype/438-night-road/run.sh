#!/usr/bin/env bash
# PROTOTYPE for #438, throwaway. Reads every Omarchy theme's colors.toml into
# themes.js, then opens the scene in Qt's qml tool.
set -euo pipefail
here=$(cd "$(dirname "$0")" && pwd)
python3 - "$here/themes.js" <<'PY'
import json, pathlib, sys, tomllib
roots = [pathlib.Path.home() / ".local/state/omarchy/current/theme",
         *sorted(pathlib.Path("/usr/share/omarchy/themes").glob("*")),
         *sorted((pathlib.Path.home() / ".local/share/omarchy/themes").glob("*"))]
themes, seen = [], set()
for root in roots:
    f = root / "colors.toml"
    if not f.is_file():
        continue
    name = (root.parent / "theme.name").read_text().strip() if root.name == "theme" else root.name
    if name in seen:
        continue
    seen.add(name)
    c = tomllib.loads(f.read_text())
    c["name"] = name
    themes.append(c)
pathlib.Path(sys.argv[1]).write_text(".pragma library\nvar all = " + json.dumps(themes) + ";\n")
print("themes:", ", ".join(t["name"] for t in themes))
PY
exec qml6 "$here/NightRoad.qml" "$@"
