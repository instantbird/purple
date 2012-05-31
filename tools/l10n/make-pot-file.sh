#!/bin/sh
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.


(cd ../../purple &&
 mkdir -p po &&
 echo '[encoding: UTF-8]' > po/POTFILES.in &&
 find libpurple -name '*.[ch]' >> po/POTFILES.in &&
 cd po &&
 intltool-update -p)

mkdir -p po
mv ../../purple/po/untitled.pot po/pidgin.pot
