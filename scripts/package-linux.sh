#!/usr/bin/env bash
set -euo pipefail
umask 022

repo=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
source_dir="$repo/clients/RemoteAccessApp"
build_dir=${BUILD_DIR:-"$repo/build-release/linux"}
dist="$repo/dist"
qmake=${QT_QMAKE:-qmake6}
jobs=${BUILD_JOBS:-4}

for command in cmake ninja "$qmake" gcc ldd readlink zip unzip; do
    command -v "$command" >/dev/null || { echo "Missing build tool: $command" >&2; exit 1; }
done

mkdir -p "$dist"
temporary=$(mktemp -d "$dist/.linux-package.XXXXXX")
trap 'rm -rf -- "$temporary"' EXIT
stage="$temporary/RemoteAccessLinux"
mkdir -p "$stage/lib" "$stage/plugins"
cmake "-DCLIENT_CONFIG=$source_dir/.env" "-DPACKAGE_DIRECTORY=$stage" -P "$repo/scripts/stage-client-config.cmake"

qt_libs=$("$qmake" -query QT_INSTALL_LIBS)
qt_plugins=$("$qmake" -query QT_INSTALL_PLUGINS)
qt_version=$("$qmake" -query QT_VERSION)
[[ $qt_version == 6.* ]] || { echo 'The Linux release requires Qt 6.' >&2; exit 1; }
cmake -S "$source_dir" -B "$build_dir" -G Ninja \
    -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=OFF -DREMOTEACCESS_REQUIRE_TLS=ON \
    "-DQt6_DIR=$qt_libs/cmake/Qt6"
cmake --build "$build_dir" --target RemoteAccessApp --parallel "$jobs"
install -m 0755 "$build_dir/RemoteAccessApp" "$stage/RemoteAccessApp"

components=("$stage/RemoteAccessApp")
for plugin in platforms/libqxcb.so tls/libqopensslbackend.so \
    imageformats/libqjpeg.so imageformats/libqsvg.so iconengines/libqsvgicon.so; do
    install -D -m 0755 "$qt_plugins/$plugin" "$stage/plugins/$plugin"
    components+=("$stage/plugins/$plugin")
done

# Qt and the X11 input injector open these libraries at runtime.
for name in libssl.so.3 libcrypto.so.3 libXtst.so.6; do
    path=$(gcc -print-file-name="$name")
    [[ $path == /* && -f $path ]] || { echo "Missing runtime library: $name" >&2; exit 1; }
    install -m 0755 "$(readlink -f -- "$path")" "$stage/lib/$name"
    components+=("$path")
done

# The host supplies glibc and its matching graphics stack. Ship the other
# dependencies of the executable, plugins and explicitly loaded libraries.
for component in "${components[@]}"; do
    dependencies=$(ldd "$component")
    if [[ $dependencies == *'not found'* ]]; then
        printf '%s\n%s\n' "Unresolved dependencies for $component" "$dependencies" >&2
        exit 1
    fi
    while read -r name path; do
        case "$name" in
            ld-linux*|libc.so.*|libm.so.*|libpthread.so.*|libdl.so.*|librt.so.*|libresolv.so.*|\
            libGL.so.*|libEGL.so.*|libGLX.so.*|libGLdispatch.so.*|libOpenGL.so.*) continue ;;
        esac
        if [[ ! -e $stage/lib/$name ]]; then
            install -m 0755 "$(readlink -f -- "$path")" "$stage/lib/$name"
        fi
    done < <(awk '$2 == "=>" && $3 ~ /^\// {print $1, $3}' <<<"$dependencies")
done

for component in "$stage/RemoteAccessApp" "$stage"/plugins/*/*.so "$stage/lib/libXtst.so.6" "$stage/lib/libssl.so.3"; do
    dependencies=$(LD_LIBRARY_PATH="$stage/lib" ldd "$component")
    if [[ $dependencies == *'not found'* ]] ||
        ! awk -v bundled="$stage/lib/" '/^[[:space:]]*libQt6[^ ]+ => / && index($3, bundled) != 1 {exit 1}' <<<"$dependencies"; then
        printf '%s\n%s\n' "Failed to resolve packaged dependencies for $component" "$dependencies" >&2
        exit 1
    fi
done

cat > "$stage/qt.conf" <<'QT'
[Paths]
Plugins=plugins
QT
cat > "$stage/run-remoteaccess.sh" <<'LAUNCHER'
#!/bin/sh
set -eu
root=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
export LD_LIBRARY_PATH="$root/lib"
export QT_QPA_PLATFORM="${QT_QPA_PLATFORM:-xcb}"
exec "$root/RemoteAccessApp" "$@"
LAUNCHER
chmod 0755 "$stage/run-remoteaccess.sh"

cat > "$stage/README.txt" <<'README'
Extract the whole ZIP and run ./run-remoteaccess.sh in an X11 desktop session.
Edit .env next to RemoteAccessApp, then restart the app to change the server.
The host supplies glibc, graphics libraries, a desktop, fonts and trusted CAs.
Use Bật chia sẻ (Enable sharing) on the CHILD before starting remote control.
README
printf '\nBuild platform: %s, Qt %s, %s\n' "$(uname -m)" "$qt_version" "$(getconf GNU_LIBC_VERSION)" >> "$stage/README.txt"
cp "$repo/scripts/client-third-party.txt" "$stage/THIRD-PARTY.txt"
printf '\nQt runtime version: %s\n' "$qt_version" >> "$stage/THIRD-PARTY.txt"
cp -r "$repo/scripts/licenses" "$stage/licenses"
(
    cd -- "$temporary"
    zip -q -r RemoteAccessLinux.zip RemoteAccessLinux
    unzip -tq RemoteAccessLinux.zip
)
mv -f -- "$temporary/RemoteAccessLinux.zip" "$dist/RemoteAccessLinux.zip"
printf 'Created %s\n' "$dist/RemoteAccessLinux.zip"
