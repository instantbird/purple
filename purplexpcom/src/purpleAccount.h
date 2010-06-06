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

#ifndef PURPLE_ACCOUNT_H_
# define PURPLE_ACCOUNT_H_

#include "purpleAccountBase.h"

#define UI_ID                  "instantbird"

class purpleAccount : public purpleAccountBase
{
public:
  //  NS_DECL_PURPLEIACCOUNT
  NS_SCRIPTABLE NS_IMETHOD UnInit(void);
  NS_SCRIPTABLE NS_IMETHOD Connect(void);
  NS_SCRIPTABLE NS_IMETHOD Disconnect(void);
  NS_SCRIPTABLE NS_IMETHOD CreateConversation(const nsACString & aName, purpleIConversation **_retval NS_OUTPARAM);
  NS_SCRIPTABLE NS_IMETHOD AddBuddy(purpleITag *aTag, const nsACString & aName);
  NS_SCRIPTABLE NS_IMETHOD LoadBuddy(purpleIBuddy *aBuddy, const nsACString & aName, const nsACString & aAlias, const nsACString & aServerAlias, purpleITag *aTag);
  NS_SCRIPTABLE NS_IMETHOD GetCanJoinChat(PRBool *aCanJoinChat);
  NS_SCRIPTABLE NS_IMETHOD GetChatRoomFields(nsISimpleEnumerator **_retval NS_OUTPARAM);
  NS_SCRIPTABLE NS_IMETHOD GetChatRoomDefaultFieldValues(const nsACString & aDefaultChatName, purpleIChatRoomFieldValues **_retval NS_OUTPARAM);
  NS_SCRIPTABLE NS_IMETHOD JoinChat(purpleIChatRoomFieldValues *aComponents);
  NS_SCRIPTABLE NS_IMETHOD GetNormalizedName(nsACString & aNormalizedName);
  NS_SCRIPTABLE NS_IMETHOD GetPassword(nsACString & aPassword);
  NS_SCRIPTABLE NS_IMETHOD SetPassword(const nsACString & aPassword);
  NS_SCRIPTABLE NS_IMETHOD GetRememberPassword(PRBool *aRememberPassword);
  NS_SCRIPTABLE NS_IMETHOD SetRememberPassword(PRBool aRememberPassword);
  NS_SCRIPTABLE NS_IMETHOD GetAlias(nsACString & aAlias);
  NS_SCRIPTABLE NS_IMETHOD SetAlias(const nsACString & aAlias);
  NS_SCRIPTABLE NS_IMETHOD GetProxyInfo(purpleIProxyInfo * *aProxyInfo);
  NS_SCRIPTABLE NS_IMETHOD SetProxyInfo(purpleIProxyInfo * aProxyInfo);
  NS_SCRIPTABLE NS_IMETHOD SetBool(const char *aName, PRBool aVal);
  NS_SCRIPTABLE NS_IMETHOD SetInt(const char *aName, PRInt32 aVal);
  NS_SCRIPTABLE NS_IMETHOD SetString(const char *aName, const char *aVal);
  NS_SCRIPTABLE NS_IMETHOD GetHTMLEnabled(PRBool *aHTMLEnabled);
  NS_SCRIPTABLE NS_IMETHOD GetNoBackgroundColors(PRBool *aNoBackgroundColors);
  NS_SCRIPTABLE NS_IMETHOD GetAutoResponses(PRBool *aAutoResponses);
  NS_SCRIPTABLE NS_IMETHOD GetSingleFormatting(PRBool *aSingleFormatting);
  NS_SCRIPTABLE NS_IMETHOD GetNoNewlines(PRBool *aNoNewlines);
  NS_SCRIPTABLE NS_IMETHOD GetNoFontSizes(PRBool *aNoFontSizes);
  NS_SCRIPTABLE NS_IMETHOD GetNoUrlDesc(PRBool *aNoUrlDesc);
  NS_SCRIPTABLE NS_IMETHOD GetNoImages(PRBool *aNoImages);

  // From purpleIAccountBase
  NS_SCRIPTABLE NS_IMETHOD Load(const nsACString & aKey,
                                purpleIProtocol *aPrpl,
                                PRBool aIsValidProtocol);
  NS_SCRIPTABLE NS_IMETHOD Create(const nsACString & aName,
                                  purpleIProtocol *aPrpl,
                                  const nsACString & aKey);
  NS_SCRIPTABLE NS_IMETHOD Disconnecting(PRInt16 aConnectionErrorReason,
                                         const nsACString & aConnectionErrorMessage);

  purpleAccount();
  PurpleAccount *GetPurpleAccount() { return mAccount; }

private:
  ~purpleAccount();
  nsresult SetBoolPref(const char *aName, PRBool aValue);
  nsresult SetIntPref(const char *aName, PRInt32 aValue);
  nsresult SetStringPref(const char *aName, const char *aValue);
  inline void GetBoolPref(const char *aName);
  inline void GetIntPref(const char *aName);
  inline void GetCharPref(const char *aName);

protected:
  /* additional members */
  PurpleAccount *mAccount;
  nsCOMPtr<purpleIProxyInfo> mProxy;
};

#endif /* !PURPLE_ACCOUNT_H_ */
