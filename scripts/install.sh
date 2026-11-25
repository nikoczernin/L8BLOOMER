#!/bin/bash
set -euo pipefail
product="L8BLOOMER"
expected_id="audio.niko.slowbloom"
legacy_name="Slow Bloom.component"
script_dir="$(cd "$(dirname "$0")" && pwd)"
name="$product.component"
# Works in a source checkout and as Install.command next to a release component.
if [[ -d "$script_dir/$name" ]]; then
    source_component="$script_dir/$name"
else
    source_component="$script_dir/../dist/$name"
fi
[[ -d "$source_component" ]] || { echo "Component missing. Download the macOS release or run bash scripts/build.sh." >&2; exit 1; }
identifier="$(/usr/libexec/PlistBuddy -c 'Print CFBundleIdentifier' "$source_component/Contents/Info.plist")"
[[ "$identifier" == "$expected_id" ]] || { echo "Unexpected source component identifier." >&2; exit 1; }
codesign --verify --deep --strict "$source_component"

plugin_root="${AUDIO_PLUGIN_ROOT:-$HOME/Library/Audio/Plug-Ins}"
destination="$plugin_root/Components/$name"
existing_components=("$destination")
if [[ -n "$legacy_name" ]]; then existing_components+=("$plugin_root/Components/$legacy_name"); fi
for existing in "${existing_components[@]}"; do
    if [[ -e "$existing" ]]; then
        identifier="$(/usr/libexec/PlistBuddy -c 'Print CFBundleIdentifier' "$existing/Contents/Info.plist")"
        [[ "$identifier" == "$expected_id" ]] || { echo "Unexpected component at $existing; nothing replaced." >&2; exit 1; }
    fi
done
mkdir -p "$plugin_root/Components"
staging="$(mktemp -d "$plugin_root/$product-stage.XXXXXX")"
trap 'rm -rf "$staging"' EXIT
ditto "$source_component" "$staging/$name"
codesign --verify --deep --strict "$staging/$name"
backup=""
moved_components=()
rollback() {
    if [[ -n "$backup" && ${#moved_components[@]} -gt 0 ]]; then
        for existing in "${moved_components[@]}"; do
            mv "$backup/$(basename "$existing")" "$existing"
        done
    fi
}
for existing in "${existing_components[@]}"; do
    if [[ -e "$existing" ]]; then
        if [[ -z "$backup" ]]; then
            mkdir -p "$plugin_root/$product backups"
            backup="$(mktemp -d "$plugin_root/$product backups/previous.XXXXXX")"
        fi
        if ! mv "$existing" "$backup/"; then rollback; exit 1; fi
        moved_components+=("$existing")
    fi
done
if ! mv "$staging/$name" "$destination"; then rollback; exit 1; fi
touch "$destination"
echo "Installed: $destination"
if [[ -n "$backup" ]]; then echo "Previous version saved in: $backup"; fi
echo "Restart Logic: Audio FX > Audio Units > Niko Audio > $product."
