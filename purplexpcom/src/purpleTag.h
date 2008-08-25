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
 * 2007.
 *
 * The Initial Developer of the Original Code is
 * Florian QUEZE <florian@instantbird.org>.
 * Portions created by the Initial Developer are Copyright (C) 2007
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

#ifndef PURPLE_TAG_H_
# define PURPLE_TAG_H_

#include "purpleITag.h"

#pragma GCC visibility push(default)
#include <libpurple/account.h>
#pragma GCC visibility pop

// 0012b173-0404-46e7-bd54-e04e22e3dd69
#define PURPLE_TAG_CID                                          \
  { 0x0012b173, 0x0404, 0x46e7,                                 \
      { 0xbd, 0x54, 0xe0, 0x4e, 0x22, 0xe3, 0xdd, 0x69 }        \
  }

#define PURPLE_TAG_CONTRACTID                   \
  "@instantbird.org/purple/tag;1"

class purpleTag : public purpleITag
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_PURPLEITAG

  purpleTag();
  void Init(PRUint32 aId, PurpleGroup *aGroup = NULL);
  void addBuddy(PurpleBuddy *aBuddy);

private:
  ~purpleTag();

protected:
  /* additional members */
  PRUint32 mId;
  PurpleGroup *mGroup;
};

#endif /* !PURPLE_TAG_H _ */
