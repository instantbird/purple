#!/bin/sh

(cd ../../purple &&
 mkdir -p po &&
 echo '[encoding: UTF-8]' > po/POTFILES.in &&
 find libpurple -name '*.[ch]' >> po/POTFILES.in &&
 cd po &&
 intltool-update -p)

mkdir -p po
mv ../../purple/po/untitled.pot po/pidgin.pot
