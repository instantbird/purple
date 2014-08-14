# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

if protocol in CONFIG['STATIC_PRPLS'].split(" "):
    FINAL_LIBRARY = 'purplexpcom'
else:
    GENERATED_SOURCES += ['xpcomModule.cpp']
    IS_COMPONENT = True
    USE_LIBS += [
        'mozalloc',
        'nspr',
        'xpcomglue_s',
        'xul',
    ]

    OS_LIBS += [
        '-L ../../../purplexpcom/src/',
        'purplexpcom',
    ]
    if CONFIG['OS_ARCH'] != 'WINNT':
        OS_LIBS += [
            'z',
        ]

    if CONFIG['OS_ARCH'] != 'WINNT' and CONFIG['OS_ARCH'] != 'Darwin':
        OS_LIBS += CONFIG['GLIB_LIBS']
        OS_LIBS += CONFIG['GLIB_GMODULE_LIBS']
        OS_LIBS += CONFIG['LIBXML2_LIBS'].split()

LIBRARY_NAME = protocol

