#!/bin/sh

# https://github.com/coelckers/prboom-plus/wiki/Building-PrBoom-Plus-on-Windows

set -e

dsda_dir="$PWD"
if [ "$(basename "$dsda_dir")" != "dsda-doom" ]; then
    echo "must be run from dsda_doom/ directory" >&2
    exit 1
fi

if [ "$#" -ne 1 ]; then
    echo "must supply 1 argument: path of mingw-bundledlls/" >&2
    exit 1
fi
bundle_dir="$(realpath "$1")"

build_dir="$dsda_dir/prboom2/build-release"
dist_dir="$dsda_dir/prboom2/release"

echo "=== removing old directories..."
rm -rf "$build_dir"
rm -rf "$dist_dir"

echo "=== configuring cmake..."
mkdir "$build_dir"
cd "$build_dir"
cmake .. -G"MSYS Makefiles" -DCMAKE_BUILD_TYPE=Release
echo "=== building..."
make -j"$(nproc)"
cd "$dsda_dir"

echo "=== creating distribution..."
mkdir "$dist_dir"
cp "$build_dir"/*.exe "$dist_dir"
cp "$build_dir"/*.wad "$dist_dir"

echo "=== bundling dependencies..."
cd "$bundle_dir"
./mingw-bundledlls --copy "$dist_dir"/dsda-doom.exe
cd "$dsda_dir"
mkdir "$dist_dir"/soundfonts
cp "$dsda_dir"/soundfont/* "$dist_dir"/soundfonts/
cp "$dsda_dir"/prboom2/COPYING "$dist_dir"/COPYING.txt

echo "=== stripping binaries..."
strip "$dist_dir"/*.exe

echo "=== zipping files..."
cd "$dist_dir"
zip -r dsda-doom-"$(date +"%Y%m%d_%H%M%S")"-win64.zip *

cd "$dsda_dir"
echo "=== done."
