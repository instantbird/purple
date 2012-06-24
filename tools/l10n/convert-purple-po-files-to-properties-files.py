#!/usr/bin/env python
#-*- coding: utf-8 -*-
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.


import polib
import re
import hashlib
import os

# the list of locales that we want to convert
po_files = ['af', 'am', 'ar', 'az', 'bg', 'bn', 'bs', 'ca', 'cs', 'da', 'de', 'dz', 'el', 'eo', 'es', 'et', 'eu', 'fa', 'fi', 'fr', 'ga', 'gl', 'gu', 'he', 'hi', 'hu', 'id', 'it', 'ja', 'ka', 'kn', 'ko', 'ku', 'lo', 'lt', 'mk', 'mn', 'nb', 'ne', 'nl', 'nn', 'oc', 'pa', 'pl', 'ps', 'pt', 'ro', 'ru', 'si', 'sk', 'sl', 'sq', 'sr', 'sv', 'ta', 'te', 'th', 'tr', 'uk', 'ur', 'vi', 'xh', 'be@latin', 'bn_IN', 'ca@valencia', 'en_AU', 'en_CA', 'en_GB', 'hy', 'km', 'mr', 'ms_MY', 'my_MM', 'or', 'pt_BR', 'sr@latin', 'sw', 'zh_CN', 'zh_HK', 'zh_TW']

# md5 hash of translator comments. Libpurple/gettext mixes translator
# comments with code comments, we need to filter them. These are the
# hashes of acceptable translator comments
comment_hashes = ['d83a1890eddfca574851f6d38fd6afe1',
                  '2652b69a34b32776848fcf1b6ad53904',
                  'ee7316c09826f4fd42b98f88749106cf',
                  'f9aa3d449ea6746cbd7c24862dce5a2c',
                  '225f00ea5af32fc3b78e7235b37cfb0d',
                  '479ded39b1ae6a7b3a559f408334cf36',
                  'fc3c9c67d37f2187aed4b528728070a6',
                  'f55fb3b4d70a54e6ccaa7450bd490242',
                  'bf0b84e89243905e59a8cc6269922a8c',
                  '238b348e1c63fba188228630cd482710',
                  'b1a2353732aa2ead4c2c0de5cc2452e2',
                  '3e434ef29c7dd8f8b6595481cb123b2d']

prpl_dirs = ["bonjour", "gg", "jabber", "msn", "myspace", "novell", "oscar",
             "qq", "sametime", "simple", "yahoo"]

# files['purple']['id1'] contains the number of strings in the package purple with the id id1 (if > 1, we have a collision and need to append a hash)
files = {'purple': {}}

# strings[package] list of entry.msgid
strings = {'purple': []}

# allstrings[entry.msgid] = {'id': id, 'packages': entry.packages}
allstrings = {}

fix_ellipsis_exp = re.compile('\.\.\.')

# replace ... with the unicode ellipsis character
def fix_ellipsis(aString):
    return fix_ellipsis_exp.sub('…', aString)

# fix paths of files that are not in the same folder in the
# instantbird source tree and the pidgin source tree
def fix_location(aString):
    if isinstance(aString, tuple):
        return fix_location(aString[0])

    if not isinstance(aString, str):
        raise TypeError

    if aString == "../libpurple/plugins/ssl/ssl-nss.c":
        return "../libpurple/ssl-nss.c"
    return aString

# fix strings for display on a single line
# replace line breaks by (visible) \n
def fix_multiline_string(aString):
    return '\\n'.join(aString.splitlines())

# fix the comment so that they are in a format suitable for .properties files
# Remvove leading '* ' and add '# ' at the beginning of each line
def fix_comment(aString):
    def remove_leading_star(aString):
        if aString.startswith('* '):
            return aString[2:]
        return aString
    return '# ' + '\n# '.join(map(remove_leading_star, aString.splitlines()))

# Return a boolean indicating if the file is used in Instantbird
def is_file_used(aString):
    if not aString.startswith("../libpurple/"):
        return False

    for dir in ["plugins", "gconf", "example", "tests"]:
        if aString.startswith(dir, 13):
            return False

    if aString.startswith("protocols/", 13):
        for dir in prpl_dirs:
            if aString.startswith(dir + "/", 23):
                return True
        return False

    return True

# return the name of the package from the path of a file
def package_from_path(aString):
    if not aString.startswith("../libpurple/"):
        raise ValueError, "not a file in libpurple"

    for dir in ["plugins", "gconf", "example", "tests"]:
        if aString.startswith(dir, 13):
            raise ValueError, "not a file used by instantbird"

    if aString.startswith("protocols/", 13):
        for dir in prpl_dirs:
            if aString.startswith(dir + "/", 23):
                if not dir in files:
                    files[dir] = {}
                    strings[dir] = []
                return dir
        raise ValueError, "not a prpl used by instantbird"

    return "purple"

# Create the identifier from a message
# This won't contain the hex hash for duplicate entries.
# The identifier is composed of the 7 first words of the message, in camel case
# words starting with % or 0x are skipped
# non-alphanumeric characters are removed, except underscore (_).
def make_id_from_string(aString):
    # utility functions
    def filter_invalid_words(aString):
        return not (aString == '' or aString.startswith("0x"))
    def capitalize_string(aString):
        return aString.capitalize()

    words = filter(filter_invalid_words,
                   re.split('(?:[^%\w]+|%\w+)+', aString))
    if words == []:
        return ''
    else:
        return words[0].lower() + ''.join(map(capitalize_string, words[1:7]))


## the main program

# read pidgin.pot to get the english version of the strings
po = polib.pofile('po/pidgin.pot')

# put data in our hashtables
for entry in po:
    def uniq(aList, aString):
        if not aString in aList:
            aList.append(aString)
        return aList

    entry.packages =  reduce(uniq,
                             map(package_from_path,
                                 filter(is_file_used,
                                        map(fix_location,
                                            entry.occurrences))),
                             [])
    if entry.packages != []:
        id = make_id_from_string(entry.msgid)
        m = hashlib.md5()
        m.update(entry.comment)
        hash = m.hexdigest()
        if not hash in comment_hashes:
            comment = ''
        else:
            comment = fix_comment(entry.comment)
        allstrings[entry.msgid] = {'id': id, 'packages': entry.packages, 'comment': comment, 'plural': entry.msgid_plural}
        for pack in entry.packages:
            strings[pack].append(entry.msgid)
            if not id in files[pack]:
                files[pack][id] = 1
            else:
                files[pack][id] = files[pack][id] + 1

# output the english language files
for pack in strings:
    dir = 'result/en_US'
    if not os.path.exists(dir):
        os.makedirs(dir)
    file = dir + '/' + pack + '.properties'
    print 'writing ' + file
    fhandle = open(file, 'w')
    for msg in strings[pack]:
        id = allstrings[msg]['id']
        if files[pack][id] > 1 or id == '':
            m = hashlib.md5()
            m.update(msg)
            id += m.hexdigest()[0:8]
        comment = allstrings[msg]['comment']
        if comment != '':
            fhandle.write('\n' + comment + '\n')
        plural = allstrings[msg]['plural']
        if plural != '':
            fhandle.write('\n# LOCALIZATION NOTE (' + id + '): Semi-colon list of plural forms.\n# See: http://developer.mozilla.org/en/docs/Localization_and_Plurals\n')
            msg += ';' + plural
        fhandle.write(id + '=' + fix_multiline_string(fix_ellipsis(msg)) + '\n')
    fhandle.close()

# output the language files for all other languages
for lang in po_files:
    po = polib.pofile('po/' + lang + '.po')
    local_strings = {}
    for entry in po:
        if not entry.translated():
            continue
        if entry.msgstr_plural:
            msgstrs = entry.msgstr_plural
            keys = msgstrs.keys()
            keys.sort()
            msg = ''
            for index in keys:
                if msg != '':
                    msg += ';'
                msg += msgstrs[index]
        else:
            msg = entry.msgstr
        local_strings[entry.msgid] = fix_ellipsis(msg)
    print lang
    for pack in strings:
        fixme = 0
        total = 0
        dir = 'result/' + lang
        if not os.path.exists(dir):
            os.makedirs(dir)
        file = dir + '/' + pack + '.properties'
        fhandle = open(file, 'w')
        for msg in strings[pack]:
            id = allstrings[msg]['id']
            if files[pack][id] > 1 or id == '':
                m = hashlib.md5()
                m.update(msg)
                id += m.hexdigest()[0:8]
            comment = allstrings[msg]['comment']
            if comment != '':
                fhandle.write('\n' + comment + '\n')

            if msg in local_strings and local_strings[msg] != '':
                fhandle.write(id + '=' + fix_multiline_string(local_strings[msg]) + '\n')
            else:
#     When a string is not translated, don't put it in the locale file
#                plural = allstrings[msg]['plural']
#                if plural != '':
#                    msg += ';' + plural
#                fhandle.write(id + '=' + fix_multiline_string(msg) + '\n')
#                fhandle.write(id + '=' + "FIXME" + '\n')
                fixme += 1
            total += 1
        fhandle.close()
        if fixme > 0:
            print 'wrote ' + file + ' (' + str(fixme) + ' untranslated out of ' + str(total) + ')'
        else:
            print 'wrote ' + file
