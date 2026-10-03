#!/bin/sh
set -eu
project_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
qt_prefix=${QT_PREFIX:-/opt/homebrew/opt/qt}
cmake -S "$project_dir" -B "$project_dir/build-package" -DCMAKE_BUILD_TYPE=MinSizeRel -DCMAKE_PREFIX_PATH="$qt_prefix"
cmake --build "$project_dir/build-package" --parallel 4
cmake --install "$project_dir/build-package" --prefix "$project_dir/dist/macos" --strip
python3 "$project_dir/scripts/thin-macos.py" "$project_dir/dist/macos/ChurchProjection.app" arm64
python3 "$project_dir/scripts/fix-macos-dependencies.py" "$project_dir/dist/macos/ChurchProjection.app"
codesign --force --deep --sign - "$project_dir/dist/macos/ChurchProjection.app"
codesign --verify --deep --strict "$project_dir/dist/macos/ChurchProjection.app"
COPYFILE_DISABLE=1 tar -c -C "$project_dir/dist/macos" ChurchProjection.app | xz -9e -T2 > "$project_dir/dist/ChurchProjection-macOS-arm64.tar.xz"
xz -t "$project_dir/dist/ChurchProjection-macOS-arm64.tar.xz"
