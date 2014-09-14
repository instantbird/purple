/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef PURPLE_CONV_IM_H_
# define PURPLE_CONV_IM_H_

#include "purpleConversation.h"

// 1646fccc-d3cb-488a-81fe-1b2aa60482a5
#define PURPLE_CONV_IM_CID \
{ 0x1646fccc, 0xd3cb, 0x488a, \
  { 0x81, 0xfe, 0x1b, 0x2a, 0xa6, 0x04, 0x82, 0xa5 } \
 }

#define PURPLE_CONV_IM_CONTRACTID     "@instantbird.org/purple/convim;1"

class purpleConvIM MOZ_FINAL : public prplIConvIM, public purpleConversation
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_PRPLICONVIM

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

  // Keep the SendMsg and SendTyping methods here.
  NS_IMETHOD SendMsg(const nsACString & aMsg);
  NS_IMETHODIMP SendTyping(const nsACString & aString, int32_t *_retval);

  purpleConvIM();

private:
  ~purpleConvIM();

protected:
  bool mSentTyping;
};

#endif /* !PURPLE_CONV_IM_H_ */
