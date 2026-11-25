#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."

product="L8BLOOMER"
target="SlowBloom"
targets=(SlowBloom_AU DspTests FrequencyTests PluginTests DspBenchmark)

[[ "$(uname -s)" == Darwin ]] || { echo "Build this Audio Unit on macOS." >&2; exit 1; }
command -v cmake >/dev/null || { echo "Install CMake 3.22+ first (see README.md)." >&2; exit 1; }
# Some Command Line Tools installations keep libc++ headers only in the SDK.
sdk_path="$(xcrun --show-sdk-path)"
if [[ -f "$sdk_path/usr/include/c++/v1/algorithm" ]]; then
    export CPLUS_INCLUDE_PATH="$sdk_path/usr/include/c++/v1${CPLUS_INCLUDE_PATH:+:$CPLUS_INCLUDE_PATH}"
fi

juce_source="${JUCE_DIR:-$PWD/vendor/JUCE-9.0.5}"
if [[ ! -f "$juce_source/CMakeLists.txt" ]]; then
    [[ -z "${JUCE_DIR:-}" ]] || { echo "JUCE_DIR does not contain JUCE sources." >&2; exit 1; }
    mkdir -p vendor
    download_dir="$(mktemp -d "${TMPDIR:-/tmp}/audio-juce.XXXXXX")"
    trap 'rm -rf "$download_dir"' EXIT
    curl --fail --location --retry 3 https://github.com/juce-framework/JUCE/archive/refs/tags/9.0.5.tar.gz -o "$download_dir/juce.tar.gz"
    archive_hash="$(shasum -a 256 "$download_dir/juce.tar.gz" | awk '{print $1}')"
    [[ "$archive_hash" == d92207f51ba728d3d195b6d5e93ecd049717a915fc250ff6844c32ea7272315a ]] || { echo "JUCE checksum mismatch." >&2; exit 1; }
    tar -xzf "$download_dir/juce.tar.gz" -C vendor
fi
export CMAKE_BUILD_PARALLEL_LEVEL="${CMAKE_BUILD_PARALLEL_LEVEL:-6}"
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release '-DCMAKE_OSX_ARCHITECTURES=arm64;x86_64' -DJUCE_DIR="$juce_source"
cmake --build build --config Release --target "${targets[@]}"
ctest --test-dir build -C Release --output-on-failure

mkdir -p dist
ditto "build/${target}_artefacts/Release/AU/${product}.component" "dist/${product}.component"
codesign --force --sign - "dist/${product}.component"
codesign --verify --deep --strict "dist/${product}.component"
cp "build/${product}-preview.png" "dist/${product}-preview.png"
echo "Built: $PWD/dist/${product}.component"
