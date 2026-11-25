#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
product="L8BLOOMER"
expected_id="audio.niko.slowbloom"
component="$PWD/dist/$product.component"
[[ -d "$component" ]] || { echo "Run bash scripts/build.sh first." >&2; exit 1; }
codesign --verify --deep --strict "$component"
identifier="$(/usr/libexec/PlistBuddy -c 'Print CFBundleIdentifier' "$component/Contents/Info.plist")"
[[ "$identifier" == "$expected_id" ]] || { echo "Unexpected component identity." >&2; exit 1; }
version="$(/usr/libexec/PlistBuddy -c 'Print CFBundleShortVersionString' "$component/Contents/Info.plist")"
revision="$(git rev-parse HEAD)"
git diff --quiet HEAD -- || { echo "Commit source changes before packaging." >&2; exit 1; }
[[ -z "$(git ls-files --others --exclude-standard)" ]] || { echo "Commit or ignore untracked files before packaging." >&2; exit 1; }
juce_source="${JUCE_DIR:-$PWD/vendor/JUCE-9.0.5}"
[[ -f "$juce_source/CMakeLists.txt" ]] || { echo "JUCE source missing; set JUCE_DIR or run the build first." >&2; exit 1; }

staging="$(mktemp -d "${TMPDIR:-/tmp}/audio-release.XXXXXX")"
trap 'rm -rf "$staging"' EXIT
binary_name="$product-$version-macOS-universal"
source_name="$product-$version-source-with-JUCE"
binary_dir="$staging/$binary_name"
source_dir="$staging/$source_name"
mkdir -p "$binary_dir" "$source_dir" dist/releases
output_dir="$PWD/dist/releases"

ditto "$component" "$binary_dir/$product.component"
cp scripts/install.sh "$binary_dir/Install.command"
chmod +x "$binary_dir/Install.command"
cp README.md LICENSE.md "$binary_dir/"
ditto docs "$binary_dir/docs"
ditto ThirdParty "$binary_dir/ThirdParty"
printf 'Source repository: https://github.com/nikoczernin/%s\nVersion: %s\nCommit: %s\nMatching complete source: %s.tar.gz\n' "$product" "$version" "$revision" "$source_name" > "$binary_dir/SOURCE.txt"

export COPYFILE_DISABLE=1
git archive --format=tar HEAD | tar -xf - -C "$source_dir"
mkdir -p "$source_dir/vendor/JUCE-9.0.5"
tar -C "$juce_source" --exclude=.git --exclude=build --exclude=.DS_Store -cf - . | tar -xf - -C "$source_dir/vendor/JUCE-9.0.5"
cp "$binary_dir/SOURCE.txt" "$source_dir/SOURCE.txt"
ditto --norsrc --noextattr -c -k --keepParent "$binary_dir" "$output_dir/$binary_name.zip"
tar -czf "$output_dir/$source_name.tar.gz" -C "$staging" "$source_name"
cd "$output_dir"
shasum -a 256 "$binary_name.zip" "$source_name.tar.gz" > SHA256SUMS.txt
echo "Packaged releases in: $output_dir"
