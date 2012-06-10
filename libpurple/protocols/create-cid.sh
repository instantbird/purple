#!/bin/bash
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

uuidgen-c++()
{
    local UUID=$(uuidgen)
    echo "// $UUID"
    echo "#define PRPL_CID \\"
    echo "{ 0x${UUID:0:8}, 0x${UUID:9:4}, 0x${UUID:14:4}, \\"
    echo -n "  { 0x${UUID:19:2}, 0x${UUID:21:2}, 0x${UUID:24:2}, "
    echo -n "0x${UUID:26:2}, 0x${UUID:28:2}, 0x${UUID:30:2}, "
    echo "0x${UUID:32:2}, 0x${UUID:34:2} } \\"
    echo " }" 
}

uuidgen-c++
