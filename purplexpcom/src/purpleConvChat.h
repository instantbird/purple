/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef PURPLE_CONV_CHAT_H_
# define PURPLE_CONV_CHAT_H_

#include "purpleConversation.h"

// 8a1d900f-aec0-4e93-bce9-29c8d7888d0b
#define PURPLE_CONV_CHAT_CID \
{ 0x8a1d900f, 0xaec0, 0x4e93, \
  { 0xbc, 0xe9, 0x29, 0xc8, 0xd7, 0x88, 0x8d, 0x0b } \
 }

#define PURPLE_CONV_CHAT_CONTRACTID     "@instantbird.org/purple/convchat;1"

class purpleConvChat MOZ_FINAL : public prplIConvChat, public purpleConversation
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_PRPLICONVCHAT

  NS_IMETHOD GetIsChat(bool *aIsChat);
  NS_IMETHOD GetNormalizedName(nsACString& aNormalizedName);

  // Forward most methods to purpleConversation
  NS_IMETHOD GetAccount(imIAccount * *aAccount) {
    return purpleConversation::GetAccount(aAccount);
  }
  NS_IMETHOD GetName(nsACString & aName) {
    return purpleConversation::GetName(aName);
  }
  NS_IMETHOD GetTitle(nsACString & aTitle) {
    return purpleConversation::GetTitle(aTitle);
  }
  NS_IMETHOD GetStartDate(PRTime *aStartDate) {
    return purpleConversation::GetStartDate(aStartDate);
  }
  NS_IMETHOD GetId(PRUint32 *aId) {
    return purpleConversation::GetId(aId);
  }
  NS_IMETHOD SetId(PRUint32 aId) {
    return purpleConversation::SetId(aId);
  }

  NS_IMETHODIMP SendTyping(const nsACString & aString, int32_t *_retval) {
    return purpleConversation::SendTyping(aString, _retval);
  }

  NS_IMETHOD UnInit(void) {
    return purpleConversation::UnInit();
  }
  NS_IMETHOD Close(void) {
    return purpleConversation::Close();
  }
  NS_IMETHOD AddObserver(nsIObserver *aObserver) {
    return purpleConversation::AddObserver(aObserver);
  }
  NS_IMETHOD RemoveObserver(nsIObserver *aObserver) {
    return purpleConversation::RemoveObserver(aObserver);
  }

  // Keep the SendMsg method here
  NS_IMETHOD SendMsg(const nsACString & aMsg);

  purpleConvChat();

private:
  ~purpleConvChat();

protected:
};

#endif /* !PURPLE_CONV_CHAT_H_ */
