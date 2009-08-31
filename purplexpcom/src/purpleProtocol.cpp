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

#include "purpleProtocol.h"
#include "purplePref.h"
#include "purpleGListEnumerator.h"
#include "purpleClassInfo.h"
#include <nsCOMPtr.h>
#include <nsComponentManagerUtils.h>
#include <nsArrayEnumerator.h>
#include <nsNetUtil.h>
#include <nsIChromeRegistry.h>

#ifdef PR_LOGGING
//
// NSPR_LOG_MODULES=purpleProtocol:5
//
static PRLogModuleInfo *gPurpleProtocolLog = nsnull;
#endif
#define LOG(args) PR_LOG(gPurpleProtocolLog, PR_LOG_DEBUG, args)

class purpleUsernameSplit : public purpleIUsernameSplit,
                            public nsIClassInfo
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSICLASSINFO
  NS_DECL_PURPLEIUSERNAMESPLIT

  purpleUsernameSplit() : mUserSplit(NULL) {}
  void Init(const PurpleAccountUserSplit *aUserSplit)
  {
    mUserSplit = aUserSplit;
  }

private:
  ~purpleUsernameSplit() {}

protected:
  /* additional members */
  const PurpleAccountUserSplit *mUserSplit;
};

PURPLE_IMPL_ISUPPORTS1_CI(purpleUsernameSplit, purpleIUsernameSplit)

#define PURPLE_IMPL_GETSPLITVALUE(aType, aName, aPurpleName, aStar)           \
  NS_IMETHODIMP purpleUsernameSplit::Get##aName(aType a##aName)               \
  {                                                                           \
    NS_ENSURE_TRUE(mUserSplit, NS_ERROR_NOT_INITIALIZED);                     \
                                                                              \
    aStar a##aName = purple_account_user_split_get_##aPurpleName(mUserSplit); \
    return NS_OK;                                                             \
  }

/* readonly attribute AUTF8String label; */
PURPLE_IMPL_GETSPLITVALUE(nsACString &, Label, text, )

/* readonly attribute AUTF8String defaultValue; */
PURPLE_IMPL_GETSPLITVALUE(nsACString &, DefaultValue, default_value, )

/* readonly attribute char separator; */
PURPLE_IMPL_GETSPLITVALUE(char *, Separator, separator, *)

/* readonly attribute PRBool reverse; */
PURPLE_IMPL_GETSPLITVALUE(PRBool *, Reverse, reverse, *)

/* Implementation file */
NS_IMPL_ISUPPORTS1_CI(purpleProtocol, purpleIProtocol)

purpleProtocol::purpleProtocol()
  :mInfo(nsnull)
{
  /* member initializers and constructor code */
#ifdef PR_LOGGING
  if (!gPurpleProtocolLog)
    gPurpleProtocolLog = PR_NewLogModule("purpleProtocol");
#endif
  LOG(("Creating protocol @%x", this));
}

void purpleProtocol::Init(PurplePluginInfo *aInfo)
{
  mInfo = aInfo;
  LOG(("Setting mInfo to @%x (%s, %s)",
       mInfo, mInfo->name, mInfo->id));
}

purpleProtocol::~purpleProtocol()
{
  /* destructor code */
  LOG(("Destructing protocol : %s (@%x)", mInfo ? mInfo->name : "not initialized", this));
}

/* readonly attribute AUTF8String name; */
NS_IMETHODIMP purpleProtocol::GetName(nsACString& aName)
{
  NS_ENSURE_TRUE(mInfo, NS_ERROR_NOT_INITIALIZED);

  aName = mInfo->name;
  return NS_OK;
}

/* readonly attribute AUTF8String id; */
NS_IMETHODIMP purpleProtocol::GetId(nsACString& aId)
{
  NS_ENSURE_TRUE(mInfo, NS_ERROR_NOT_INITIALIZED);

  aId = mInfo->id;
  return NS_OK;
}

/* readonly attribute AUTF8String iconBaseURI; */
NS_IMETHODIMP purpleProtocol::GetIconBaseURI(nsACString& aIconBaseURI)
{
  NS_ENSURE_TRUE(mInfo, NS_ERROR_NOT_INITIALIZED);

  nsCString spec("chrome://");
  spec.Append(mInfo->id);
  spec.Append("/skin/");
  nsCOMPtr<nsIURI> uri;
  nsresult rv = NS_NewURI(getter_AddRefs(uri), spec);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIChromeRegistry> chromeRegistry =
    do_GetService(NS_CHROMEREGISTRY_CONTRACTID);
  NS_ENSURE_TRUE(chromeRegistry, NS_ERROR_FAILURE);

  nsCOMPtr<nsIURI> resolvedURI;
  rv = chromeRegistry->ConvertChromeURL(uri, getter_AddRefs(resolvedURI));
  if (NS_SUCCEEDED(rv))
    aIconBaseURI = spec;
  else {
    // this prpl doesn't provide icons, use the generic ones
    aIconBaseURI = "chrome://instantbird/skin/prpl-generic/";
  }

  return NS_OK;
}

/* boolean accountExists (in AUTF8String aName); */
NS_IMETHODIMP purpleProtocol::AccountExists(const nsACString& aName,
                                            PRBool *aExists)
{
  NS_ENSURE_TRUE(mInfo, NS_ERROR_NOT_INITIALIZED);

  *aExists = !!purple_accounts_find(PromiseFlatCString(aName).get(),
                                    mInfo->id);
  return NS_OK;
}

#define GET_PROTO_INFO(aInfo) ((PurplePluginProtocolInfo *)aInfo->extra_info)

/* nsISimpleEnumerator getOptions (); */
NS_IMETHODIMP purpleProtocol::GetOptions(nsISimpleEnumerator **aResult)
{
  NS_ENSURE_TRUE(mInfo, NS_ERROR_NOT_INITIALIZED);

  purpleGListEnumerator *enumerator = new purpleGListEnumerator();
  enumerator->Init(GET_PROTO_INFO(mInfo)->protocol_options,
                   purpleTypeToInterface<purplePref,
                                         purpleIPref,
                                         PurpleAccountOption>);

  NS_ADDREF(*aResult = enumerator);
  return NS_OK;
}

/* nsISimpleEnumerator getUsernameSplit (); */
NS_IMETHODIMP purpleProtocol::GetUsernameSplit(nsISimpleEnumerator **aResult)
{
  NS_ENSURE_TRUE(mInfo, NS_ERROR_NOT_INITIALIZED);

  purpleGListEnumerator *enumerator = new purpleGListEnumerator();
  enumerator->Init(GET_PROTO_INFO(mInfo)->user_splits,
                   purpleTypeToInterface<purpleUsernameSplit,
                                         purpleIUsernameSplit,
                                         PurpleAccountUserSplit>);

  NS_ADDREF(*aResult = enumerator);
  return NS_OK;
}

/* readonly attribute AUTF8String usernameEmptyText; */
NS_IMETHODIMP purpleProtocol::GetUsernameEmptyText(nsACString &aUsernameEmptyText)
{
  NS_ENSURE_TRUE(mInfo, NS_ERROR_NOT_INITIALIZED);

  aUsernameEmptyText = "";
  if (GET_PROTO_INFO(mInfo)->get_account_text_table) {
    GHashTable *table = GET_PROTO_INFO(mInfo)->get_account_text_table(NULL);
    const char *label =
      (const char *)g_hash_table_lookup(table, "login_label");
    if (label)
      aUsernameEmptyText = label;
    g_hash_table_destroy(table);
  }

  return NS_OK;
}

#define PURPLE_IMPL_GETBOOLOPT(aOpt, aConst)                            \
  NS_IMETHODIMP purpleProtocol::Get##aOpt(PRBool *a##aOpt)              \
  {                                                                     \
    NS_ENSURE_TRUE(mInfo, NS_ERROR_NOT_INITIALIZED);                    \
                                                                        \
    *a##aOpt =                                                          \
      GET_PROTO_INFO(mInfo)->options & OPT_PROTO_##aConst ? PR_TRUE     \
                                                          : PR_FALSE;   \
                                                                        \
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
NS_IMETHODIMP purpleProtocol::GetInfo(PurplePluginProtocolInfo * *aPrplInfo)
{
  NS_ENSURE_TRUE(mInfo, NS_ERROR_NOT_INITIALIZED);
  *aPrplInfo = (PurplePluginProtocolInfo *)mInfo->extra_info;

  return NS_OK;
}

/* [noscript] PurpleNativeAccount createPurpleAccount (in AUTF8String aName,
                                                       in purpleIAccount aAccount); */
NS_IMETHODIMP purpleProtocol::CreatePurpleAccount(const nsACString & aName,
                                                  purpleIAccount *aAccount,
                                                  PurpleAccount * *aResult)
{
  NS_ENSURE_TRUE(mInfo, NS_ERROR_NOT_INITIALIZED);

  PurpleAccount *account =
    purple_account_new(PromiseFlatCString(aName).get(), mInfo->id);
  NS_ENSURE_TRUE(account, NS_ERROR_FAILURE);

  account->ui_data = aAccount;
  purple_accounts_add(account);

  *aResult = account;
  return NS_OK;
}
