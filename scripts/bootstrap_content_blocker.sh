#!/usr/bin/env bash
set -euo pipefail

repository_root="$(cd "$(dirname "$0")/.." && pwd)"
manifest="$repository_root/third_party/content-blocker/Cargo.toml"
target_directory="$repository_root/.cache/content-blocker"

CARGO_TARGET_DIR="$target_directory" cargo build --locked --release --manifest-path "$manifest"

case "$(uname -s)" in
  Darwin) library="$target_directory/release/libomaweb_content_blocker.dylib" ;;
  Linux) library="$target_directory/release/libomaweb_content_blocker.so" ;;
  *) echo "Unsupported content-blocker host: $(uname -s)" >&2; exit 1 ;;
esac

if command -v shasum >/dev/null 2>&1; then
  shasum -a 256 "$library" > "$library.sha256"
else
  sha256sum "$library" > "$library.sha256"
fi

echo "$library"
