/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef PURPLE_CONV_CHAT_BUDDY_H_
# define PURPLE_CONV_CHAT_BUDDY_H_

#include "prplIConversation.h"
#include "purpleAccountBuddy.h"
#include <nsStringAPI.h>
#include <nsIClassInfo.h>

class purpleConvChatBuddy MOZ_FINAL : public prplIConvChatBuddy,
                                      public nsIClassInfo
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSICLASSINFO
  NS_DECL_PRPLICONVCHATBUDDY

  purpleConvChatBuddy();
  void Init(PurpleConvChatBuddy *aBuddy);

private:
  ~purpleConvChatBuddy();

protected:
  nsCString mName;
  nsCString mAlias;
  bool mBuddy;
  PurpleConvChatBuddyFlags mFlags;
};

#endif /* !PURPLE_CONV_CHAT_BUDDY_H_ */
