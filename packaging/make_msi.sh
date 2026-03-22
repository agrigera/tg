#!/bin/bash

set -e

if test ! -d "$1"
then
	echo "Usage: $0 resources-dir"
	echo
	echo "resources-dir must contain runtime files to be bundled (DLLs, data files, etc.)."
	exit 1
fi

DIR=`dirname "${BASH_SOURCE[0]}"`
ABSDIR=`cd "$DIR"; pwd`

if ! command -v heat >/dev/null 2>&1 || ! command -v candle >/dev/null 2>&1 || ! command -v light >/dev/null 2>&1
then
	echo "Error: WiX Toolset v3 commands not found in PATH (heat, candle, light)."
	echo "Install WiX Toolset and ensure its bin directory is available in the shell PATH."
	exit 1
fi

if ! command -v uuidgen >/dev/null 2>&1
then
	echo "Error: uuidgen not found in PATH; required by packaging/make_wxs.sh."
	exit 1
fi

RESOURCES=`cd "$1"; pwd`
TARGET="$ABSDIR/../msi"

cd "$ABSDIR"/..

VERSION=`cat version`

"$ABSDIR/make_wxs.sh"

./configure
make

rm -rf "$TARGET"
mkdir -p "$TARGET"

cp "$ABSDIR/../tg-timer.exe" "$TARGET/tg.exe"
cd "$TARGET"
cp "$ABSDIR/tg-timer.wxs" "$TARGET"
cp "$ABSDIR/LICENSE.rtf" "$TARGET"
cp "$ABSDIR/../README.md" "$TARGET"
cp "$ABSDIR/../LICENSE" "$TARGET"
cp "$ABSDIR/../icons/tg-document.ico" "$TARGET"
cp -r "$RESOURCES"/* "$TARGET"
heat dir "$RESOURCES" -srd -gg -sreg -dr INSTALLDIR -cg Resources -out "$TARGET/Resources.wxs"

candle tg-timer.wxs Resources.wxs
light -out tg-timer_${VERSION}.msi -ext WixUIExtension tg-timer.wixobj Resources.wixobj

echo "MSI generated at: $TARGET/tg-timer_${VERSION}.msi"
