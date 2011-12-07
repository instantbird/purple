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

#ifndef PURPLE_MESSAGE_H_
#define PURPLE_MESSAGE_H_

#include "prplIMessage.h"
#pragma GCC visibility push(default)
#include <libpurple/conversation.h>
#pragma GCC visibility pop
#include "nsStringAPI.h"

// 0ca0b0a3-ab52-434a-9176-a7d7daa74bad
#define PURPLE_MESSAGE_CID                                      \
  { 0x0ca0b0a3, 0xab52, 0x434a,                                 \
      { 0x91, 0x76, 0xa7, 0xd7, 0xda, 0xa7, 0x4b, 0xad }        \
  }

#define PURPLE_MESSAGE_CONTRACTID \
  "@instantbird.org/purple/message;1"

class purpleMessage : public prplIMessage
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_PRPLIMESSAGE

  purpleMessage();
  nsresult Init(PurpleConversation *conv, const char *who, const char *alias,
                const char *message, PurpleMessageFlags flags, time_t mtime);

private:
  ~purpleMessage();

protected:
  /* additional members */
  PRUint32 mId;
  static PRUint32 sLastId;
  PurpleConversation *mConv;
  nsCString mOriginalMessage;
  nsCString mMessage;
  nsCString mWho, mAlias;
  nsCString mColor;
  PurpleMessageFlags mFlags;
  time_t mTime;
};

#endif /* !PURPLE_MESSAGE_H_ */
