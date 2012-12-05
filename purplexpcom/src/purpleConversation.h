/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef PURPLE_CONVERSATION_H_
#define PURPLE_CONVERSATION_H_

#include "prplIConversation.h"
#include "nsCOMArray.h"

#pragma GCC visibility push(default)
#include <libpurple/conversation.h>
#pragma GCC visibility pop

class purpleAccount;

class purpleConversation : public prplIConversation
{
public:
  NS_DECL_PRPLICONVERSATION
  purpleConversation();

  void SetConv(PurpleConversation *aConv);
  PurpleConversation *GetConv() { return mConv; };
  void NotifyObservers(nsISupports *aSubject,
                       const char *aTopic,
                       const PRUnichar *aData);
  static inline purpleConversation *fromPurpleConv(PurpleConversation *aConv) {
    return static_cast<purpleConversation *>(aConv->ui_data);
  }

private:
  nsCOMArray<nsIObserver> mObservers;

protected:
  ~purpleConversation();
  purpleAccount *GetAccount();
  PurpleConversation *mConv;
  PRUint32 mId;
  PRTime mStartDate;
  bool mUninitialized;
};

#endif /* !PURPLE_CONVERSATION_H_ */
