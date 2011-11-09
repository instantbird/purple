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

#ifndef PURPLE_PROTOCOL_H_
# define PURPLE_PROTOCOL_H_

#include "purpleProxyInfo.h"
#include "prplIProtocol.h"
#include "purpleIPref.h"
#pragma GCC visibility push(default)
#include <libpurple/account.h>
#pragma GCC visibility pop

// 63ee9306-8615-4896-9f1f-6e6fafe17d1e
#define PURPLE_PROTOCOL_CID \
  { 0x63ee9306, 0x8615, 0x4896,                                 \
      { 0x9f, 0x1f, 0x6e, 0x6f, 0xaf, 0xe1, 0x7d, 0x1e }        \
  }

#define PURPLE_PROTOCOL_CONTRACTID \
    "@instantbird.org/purple/protocol;1"

class purpleProtocol : public prplIProtocol
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_PRPLIPROTOCOL

  purpleProtocol();

private:
  ~purpleProtocol();

  PurplePluginInfo *mInfo;

protected:
  /* additional members */
};

#endif /* !PURPLE_PROTOCOL_H_ */
