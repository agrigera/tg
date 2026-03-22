#!/bin/bash

DIR=`dirname "${BASH_SOURCE[0]}"`
ABSDIR=`cd "$DIR"; pwd`

cd $DIR

VERSION=`cat ../version`
VERSIONX=`cat ../version | sed "s/\\([0-9]*\\.[0-9]*\\.[0-9]*\\).*/\\1/"`

awk -v version="$VERSION" -v versionx="$VERSIONX" '
function newuuid(  cmd, u) {
	cmd = "uuidgen"
	cmd | getline u
	close(cmd)
	return u
}
{
	line = $0
	gsub(/#VERSION#/, version, line)
	gsub(/#VERSIONX#/, versionx, line)
	while (match(line, /#UUID#/)) {
		u = newuuid()
		line = substr(line, 1, RSTART-1) u substr(line, RSTART+RLENGTH)
	}
	print line
}
' tg-timer.wxs.template > tg-timer.wxs
