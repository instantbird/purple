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
        'xpcomglue_s',
        'xul',
    ]

LIBRARY_NAME = protocol

