#!/bin/bash
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

licence-header()
{
cat <<EOF
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

EOF
}

header-top()
{
sed "s/@MAX@/$1/;s/@MED@/$2/" <<EOF
#ifndef PURPLE_@MAX@_H_
# define PURPLE_@MAX@_H_

#include "purpleI@MED@.h"

EOF
}

header-bottom()
{

maxnospace=`echo $1 |tr -d '_'`
sed "s/@MAX@/$1/;s/@MED@/$2/g;s/@MIN@/$3/;s/@MAXNOSPACE@/$maxnospace/" <<EOF

#define PURPLE_@MAX@_CONTRACTID \
    "@instantbird.org/purple/@MIN@;1"

class purple@MED@ : public purpleI@MED@
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_PURPLEI@MAXNOSPACE@

  purple@MED@();

private:
  ~purple@MED@();

protected:
};

#endif /* !PURPLE_@MAX@_H_ */
EOF
}

uuidgen-c++()
{
    local UUID=$(uuidgen |tr A-Z a-z)
    echo "// $UUID"
    echo "#define PURPLE_"$1"_CID \\"
    echo "{ 0x${UUID:0:8}, 0x${UUID:9:4}, 0x${UUID:14:4}, \\"
    echo -n "  { 0x${UUID:19:2}, 0x${UUID:21:2}, 0x${UUID:24:2}, "
    echo -n "0x${UUID:26:2}, 0x${UUID:28:2}, 0x${UUID:30:2}, "
    echo "0x${UUID:32:2}, 0x${UUID:34:2} } \\"
    echo " }" 
}

create-header()
{
    licence-header

    max=`echo $1 |tr 'a-z ' 'A-Z_'`
    med=`echo $1 |tr -d ' '`
    min=`echo $med |tr A-Z a-z`

    header-top $max $med
    uuidgen-c++ $max
    header-bottom $max $med $min
}

create-header "$1"
