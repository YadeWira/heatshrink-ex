#!/bin/sh
# Build the release artifacts for heatshrink-ex:
#
#   dist/heatshrink-ex-<version>.tar.gz         source tarball (+ .sha256)
#   dist/heatshrink-ex-<version>-win32.zip      Windows x86 binary (+ .sha256)
#   dist/heatshrink-ex-<version>-win64.zip      Windows x64 binary (+ .sha256)
#
# The Windows binaries are only produced when the MinGW-w64 cross compilers
# are available; the source tarball is always produced. The version is read
# from include/heatshrink_common.h.
#
# Usage: ./make-release.sh
set -eu

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
cd "$ROOT"

MAJOR=$(sed -n 's/^#define HEATSHRINK_VERSION_MAJOR[[:space:]]*\([0-9][0-9]*\).*/\1/p' include/heatshrink_common.h)
MINOR=$(sed -n 's/^#define HEATSHRINK_VERSION_MINOR[[:space:]]*\([0-9][0-9]*\).*/\1/p' include/heatshrink_common.h)
PATCH=$(sed -n 's/^#define HEATSHRINK_VERSION_PATCH[[:space:]]*\([0-9][0-9]*\).*/\1/p' include/heatshrink_common.h)
VERSION=$MAJOR.$MINOR.$PATCH

if [ -n "$(git status --porcelain)" ]; then
    echo "error: the working tree is not clean; commit or stash first" >&2
    exit 1
fi

DIST=${DIST:-dist}
mkdir -p "$DIST"

NAME=heatshrink-ex-$VERSION
TARBALL=$DIST/$NAME.tar.gz
echo "==> source tarball: $TARBALL"
git archive --format=tar.gz --prefix="$NAME/" -o "$TARBALL" HEAD

build_win()
{
    arch=$1
    cc=$2
    label=$3

    if ! command -v "$cc" >/dev/null 2>&1; then
        echo "    (skipping $label: $cc not found)"
        return 0
    fi

    bdir=$DIST/build-$label
    stage=$DIST/$NAME-$label
    rm -rf "$bdir" "$stage"

    echo "==> building $label with $cc"
    cmake -S . -B "$bdir" \
        -DCMAKE_SYSTEM_NAME=Windows \
        -DCMAKE_C_COMPILER="$cc" \
        -DCMAKE_BUILD_TYPE=MinSizeRel \
        -DHEATSHRINK_BUILD_TESTS=OFF >/dev/null
    cmake --build "$bdir" --parallel >/dev/null

    mkdir -p "$stage"
    cp "$bdir/heatshrink.exe" "$stage/"
    cp LICENSE README.md CHANGELOG.md "$stage/"
    (cd "$DIST" && zip -qr "$NAME-$label.zip" "$NAME-$label")
    rm -rf "$stage" "$bdir"
    echo "    $DIST/$NAME-$label.zip"
}

build_win i686 i686-w64-mingw32-gcc win32
build_win x86_64 x86_64-w64-mingw32-gcc win64

cd "$DIST"
sha256sum ./*.tar.gz ./*.zip 2>/dev/null > SHA256SUMS || true
echo
echo "==> artifacts in $DIST:"
cat SHA256SUMS
