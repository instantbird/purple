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
 * 2009.
 *
 * The Initial Developer of the Original Code is
 * Florian QUEZE <florian@instantbird.org>.
 * Portions created by the Initial Developer are Copyright (C) 2009
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

#include "purpleUnknownProtocol.h"
#include <nsArrayEnumerator.h>

#ifdef PR_LOGGING
//
// NSPR_LOG_MODULES=purpleUnknownProtocol:5
//
static PRLogModuleInfo *gPurpleUnknownProtocolLog = nsnull;
#endif
#define LOG(args) PR_LOG(gPurpleUnknownProtocolLog, PR_LOG_DEBUG, args)

/* Implementation file */
NS_IMPL_ISUPPORTS1(purpleUnknownProtocol, purpleIProtocol)

purpleUnknownProtocol::purpleUnknownProtocol()
{
  /* member initializers and constructor code */
#ifdef PR_LOGGING
  if (!gPurpleUnknownProtocolLog)
    gPurpleUnknownProtocolLog = PR_NewLogModule("purpleUnknownProtocol");
#endif
  LOG(("Creating unknown protocol @%x", this));
}

purpleUnknownProtocol::~purpleUnknownProtocol()
{
  /* destructor code */
  LOG(("Destructing unknown protocol : %s (@%x)",
       !mPrplId.IsEmpty() ? mPrplId.get() : "not initialized", this));
}

/* readonly attribute AUTF8String name; */
NS_IMETHODIMP purpleUnknownProtocol::GetName(nsACString& aName)
{
  aName = EmptyCString();
  return NS_OK;
}

/* readonly attribute AUTF8String id; */
NS_IMETHODIMP purpleUnknownProtocol::GetId(nsACString& aId)
{
  NS_ENSURE_TRUE(!mPrplId.IsEmpty(), NS_ERROR_NOT_INITIALIZED);

  aId = mPrplId;
  return NS_OK;
}

/* readonly attribute AUTF8String iconBaseURI; */
NS_IMETHODIMP purpleUnknownProtocol::GetIconBaseURI(nsACString& aIconBaseURI)
{
  aIconBaseURI = "chrome://instantbird/skin/prpl-unknown/";

  return NS_OK;
}

/* boolean accountExists (in AUTF8String aName); */
NS_IMETHODIMP purpleUnknownProtocol::AccountExists(const nsACString& aName,
                                            PRBool *aExists)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* nsISimpleEnumerator getOptions (); */
NS_IMETHODIMP purpleUnknownProtocol::GetOptions(nsISimpleEnumerator **aResult)
{
  return NS_NewArrayEnumerator(aResult, nsnull);
}

/* nsISimpleEnumerator getUsernameSplit (); */
NS_IMETHODIMP purpleUnknownProtocol::GetUsernameSplit(nsISimpleEnumerator **aResult)
{
  return NS_NewArrayEnumerator(aResult, nsnull);
}

#define PURPLE_IMPL_GETBOOLOPT(aOpt, aConst)                            \
  NS_IMETHODIMP purpleUnknownProtocol::Get##aOpt(PRBool *a##aOpt)       \
  {                                                                     \
    /* FALSE seems acceptable for all the options */                    \
    *a##aOpt = PR_FALSE;                                                \
    return NS_OK;                                                       \
  }

/* readonly attribute boolean uniqueChatName; */
PURPLE_IMPL_GETBOOLOPT(UniqueChatName, UNIQUE_CHATNAME)

/* readonly attribute boolean chatHasTopic; */
PURPLE_IMPL_GETBOOLOPT(ChatHasTopic, CHAT_TOPIC)

/* readonly attribute boolean noPassword; */
PURPLE_IMPL_GETBOOLOPT(NoPassword, NO_PASSWORD)

/* readonly attribute boolean newMailNotification; */
PURPLE_IMPL_GETBOOLOPT(NewMailNotification, MAIL_CHECK)

/* readonly attribute boolean imagesInIM; */
PURPLE_IMPL_GETBOOLOPT(ImagesInIM, IM_IMAGE)

/* readonly attribute boolean passwordOptional; */
PURPLE_IMPL_GETBOOLOPT(PasswordOptional, PASSWORD_OPTIONAL)

/* readonly attribute boolean usePointSize; */
PURPLE_IMPL_GETBOOLOPT(UsePointSize, USE_POINTSIZE)

/* readonly attribute boolean registerNoScreenName; */
PURPLE_IMPL_GETBOOLOPT(RegisterNoScreenName, REGISTER_NOSCREENNAME)

/* readonly attribute boolean slashCommandsNative; */
PURPLE_IMPL_GETBOOLOPT(SlashCommandsNative, SLASH_COMMANDS_NATIVE)

/* [noscript] PurpleNativePluginProtocolInfo getInfo (); */
NS_IMETHODIMP purpleUnknownProtocol::GetInfo(PurplePluginProtocolInfo * *aPrplInfo)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* [noscript] PurpleNativeAccount createPurpleAccount (in AUTF8String aName,
                                                       in purpleIAccount aAccount); */
NS_IMETHODIMP purpleUnknownProtocol::CreatePurpleAccount(const nsACString & aName,
                                                         purpleIAccount *aAccount,
                                                         PurpleAccount * *aResult)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}
