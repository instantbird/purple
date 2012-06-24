#!/bin/sh
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

mkcd () {
  if [ ! -d $1/ ]; then
    mkdir $1
  fi
  cd $1
}

latestversion() {
  wget -q -O - http://pidgin.im | \
  grep class=\"number\" |head -n 1| \
  sed 's/.*">//;s/<\/span>//'
}

getfilename() {
  echo -n pidgin-$1.tar.bz2
}
getversion() {
  if [ $1 = "latest" ]; then
    latest=`latestversion`
    filename=`getfilename $latest`
  else
    filename=`getfilename $1`
  fi

  if [ ! -f cache/$filename ]; then
    mkcd cache
    wget http://downloads.sourceforge.net/pidgin/$filename
    cd -
  fi
}

pruneunused() {
  rm -rf $1/{finch,pidgin,configure,doc}
  rm -rf $1/{ChangeLog*,config*,compile,AUTHORS,COPYRIGHT,NEWS,aclocal.m4,depcomp,install-sh,intltool*,missing,pidgin.*,ltmain.sh}
  find . -name 'Makefile.*' -exec rm -r {} \;
  mv $1/libpurple/plugins/ssl/ssl-nss.c $1/libpurple
  rm -rf $1/libpurple/{plugins,gconf,example,tests}
  rm -rf $1/libpurple/protocols/{irc,msnp9,mxit,silc,silc10,toc,zephyr}
  rm -rf $1/libpurple/protocols/jabber/jingle
  rm -f $1/libpurple/purple-*
  rm -f $1/libpurple/dbus-{a,b,d,p,s,t,u}*
  rm -f $1/libpurple/marshallers.{c,h,list}
  rm -f $1/libpurple/{purple.pc.in,purple.h,purple.h.in,version.h}
  rm -f $1/libpurple/{gaim-compat,media-gst,valgrind}.h
  rm -f $1/libpurple/{log,certificate,desktopitem,pounce,media,mediamanager,theme,theme-loader,theme-manager,savedstatuses,sound,sound-theme,sound-theme-loader}.{c,h}
  rm -f $1/libpurple/win32/{giowin32.c,global.mak,libpurplerc.rc.in,rules.mak,targets.mak}
}

extract() {
  mkcd extract
  filename=`getfilename $1`
  rm -rf pidgin-$1
  echo Extracting $filename...
  tar -xjf ../cache/$filename
  pruneunused pidgin-$1
  cd -
}

diffversions() {
  getversion $1
  getversion $2
  extract $1
  extract $2

  cd extract
  diffname=diff-$1-to-$2.patch
  echo Diffing $1 to $2...
  diff -ru pidgin-$1 pidgin-$2 > ../$diffname
  cd -
  diffstat < $diffname
}

diffcurrent() {
  getversion $1
  extract $1

  diffname=diff-current-to-$1.patch
  echo Diffing current to $1...
  diff -ru extract/pidgin-$1/libpurple libpurple > ./$diffname
  diffstat < $diffname
}

current=`cat config/version.txt`

if [ ! -z "$DIFFCURRENTONLY" ]; then
  diffcurrent $current
  exit
fi

if [ -z "$LATEST" ]; then
  latest=`latestversion`
  if [ -z "$latest" ]; then
    echo "Couldn't fetch latest version number."
    exit 1
  fi
else
  latest=$LATEST
fi

#diffversions 2.4.0 $latest
#diffversions 2.2.1 2.4.0
#diffversions 2.4.0 2.4.2

if [ "$latest" = "$current" ]; then
  echo "Already up to date (version $latest)."
  exit
fi

echo "Attempting to upgrade from $current to $latest"

getversion $latest
diffcurrent $current
extract $latest

cd extract/pidgin-$latest
patch -p0 --dry-run < ../../diff-current-to-$current.patch
cd -

echo "run patch -p0 < ../../diff-current-to-$current.patch and merge changes manually in extract/pidgin-$latest"
echo "then run: diff -ru libpurple extract/pidgin-$latest/libpurple > diff-update-to-$latest.patch "
echo "and finally: patch -p0 <diff-update-to-$latest.patch"

echo "also run: echo $latest > config/version.txt"
echo "don't forget to update purple-prefs.js and the translations"
