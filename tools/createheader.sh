#!/bin/bash

licence-header()
{
sed s/@YEAR@/$(date +%Y)/ <<EOF
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is the Instantbird messenging client, released
 * @YEAR@.
 *
 * The Initial Developer of the Original Code is
 * Florian QUEZE <florian@instantbird.org>.
 * Portions created by the Initial Developer are Copyright (C) @YEAR@
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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
    local UUID=$(uuidgen)
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
