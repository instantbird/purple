/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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

class purpleMessage MOZ_FINAL : public prplIMessage
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
