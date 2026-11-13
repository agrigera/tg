#!/bin/bash

set -euo pipefail

DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")"; pwd)
ROOT=$(cd "$DIR/.."; pwd)
LIST="$ROOT/packaging/tg-timer_resources"
SOURCE=$(cygpath -u 'C:\msys64\mingw64')
TARGET="$ROOT/msi-resources"

if test ! -f "$LIST"
then
	echo "Error: file list not found: $LIST" >&2
	exit 1
fi

if test -L "$TARGET" || { test -e "$TARGET" && test ! -d "$TARGET"; }
then
	echo "Error: resources target must be a directory, not a symbolic link: $TARGET" >&2
	exit 1
fi

files=()
while IFS= read -r file || test -n "$file"
do
	file=${file%$'\r'}
	file=${file//\\//}
	test -n "$file" || continue

	case "$file" in
		/*|*:*|.|..|./*|../*|*/./*|*/../*|*/.|*/..)
			echo "Error: expected a relative file path without traversal: $file" >&2
			exit 1
			;;
	esac

	if test ! -f "$SOURCE/$file"
	then
		echo "Error: source file not found: $SOURCE/$file" >&2
		exit 1
	fi
	files+=("$file")
done < "$LIST"

if test "${#files[@]}" -eq 0
then
	echo "Error: file list is empty: $LIST" >&2
	exit 1
fi

# Validate the whole list before removing the previous resources.
rm -rf -- "$TARGET"
mkdir -p -- "$TARGET"

for file in "${files[@]}"
do
	destination="$file"
	case "$file" in
		bin/*) destination=${file#bin/} ;;
	esac
	mkdir -p -- "$(dirname "$TARGET/$destination")"
	cp -- "$SOURCE/$file" "$TARGET/$destination"
done

echo "Copied ${#files[@]} files to: $TARGET"
