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

#include "purpleAccountBuddy.h"
#include "purpleCoreService.h"
#include "purpleAccount.h"
#include "purpleStorage.h"
#include "purpleUtils.h"
#include <nsServiceManagerUtils.h>
#include <prprf.h>

NS_IMPL_ISUPPORTS1(purpleAccountBuddy, purpleIAccountBuddy)

purpleAccountBuddy::purpleAccountBuddy()
{
  /* member initializers and constructor code */
}

purpleAccountBuddy::~purpleAccountBuddy()
{
  /* destructor code */
}

void purpleAccountBuddy::Init(purpleIAccount *aAccount,
                              PurpleBuddy *aBuddy,
                              purpleTag *aTag)
{
  mAccount = aAccount;
  mBuddy = aBuddy;
  mTag = aTag;
}

void purpleAccountBuddy::Uninit()
{
  mBuddy = NULL;
}

nsresult purpleAccountBuddy::Store(PRInt32 aBuddyId)
{
  nsCString key;
  nsresult rv = mAccount->GetId(key);
  NS_ENSURE_SUCCESS(rv, rv);

  PRInt32 accountId;
  if (PR_sscanf(key.get(), ACCOUNT_KEY "%u", &accountId) != 1) {
    return NS_ERROR_UNEXPECTED;
  }

  PRInt32 groupId;
  rv = mTag->GetId(&groupId);
  NS_ENSURE_SUCCESS(rv, rv);

  purpleStorage *storageInstance = purpleStorage::GetInstance();
  mozIStorageStatement *statement = storageInstance->mInsertAccountBuddy;
  mozStorageStatementScoper scoper(statement);
  rv = statement->BindInt32Parameter(0, accountId);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = statement->BindInt32Parameter(1, aBuddyId);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = statement->BindInt32Parameter(2, groupId);
  NS_ENSURE_SUCCESS(rv, rv);

#ifdef DEBUG_PURPLE_BUDDY
  printf("purpleAccountBuddy::Store(buddyId = %i, groupId = %i)\n",
         aBuddyId, groupId);
#endif
  return statement->Execute();
}

/* readonly attribute purpleIBuddy buddy; */
NS_IMETHODIMP purpleAccountBuddy::GetBuddy(purpleIBuddy * *aBuddy)
{
  if (!mBuddy) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  nsCOMPtr<purpleCoreService> pcs = do_GetService(PURPLE_CORE_SERVICE_CONTRACTID);
  return pcs->GetBuddyByPurpleBuddy(mBuddy, aBuddy);
}

/* readonly attribute purpleIAccount account; */
NS_IMETHODIMP purpleAccountBuddy::GetAccount(purpleIAccount * *aAccount)
{
  if (!mAccount) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  *aAccount = mAccount;
  NS_ADDREF(*aAccount);
  return NS_OK;
}

/* readonly attribute purpleITag tag; */
NS_IMETHODIMP purpleAccountBuddy::GetTag(purpleITag * *aTag)
{
  if (!mTag) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  *aTag = mTag;
  NS_ADDREF(*aTag);
  return NS_OK;
}

/* readonly attribute AUTF8String buddyName; */
NS_IMETHODIMP purpleAccountBuddy::GetBuddyName(nsACString& aBuddyName)
{
  if (!mBuddy) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  aBuddyName = purple_buddy_get_name(mBuddy);
  return NS_OK;
}

/* purpleIConversation createConversation (); */
NS_IMETHODIMP purpleAccountBuddy::CreateConversation(purpleIConversation **aResult)
{
  if (!mAccount || !mBuddy) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  nsCString buddyName(purple_buddy_get_name(mBuddy));
  return mAccount->CreateConversation(buddyName, aResult);
}

#define PURPLE_PRESENCE_GET_BOOL_IMPL(aName, aFctName)                  \
  NS_IMETHODIMP purpleAccountBuddy::Get##aName(PRBool *a##aName)        \
  {                                                                     \
    if (!mBuddy) {                                                      \
      return NS_ERROR_NOT_INITIALIZED;                                  \
    }                                                                   \
                                                                        \
    *a##aName = purple_presence_is_##aFctName(purple_buddy_get_presence(mBuddy)); \
    return NS_OK;                                                       \
  }

/* readonly attribute boolean online; */
PURPLE_PRESENCE_GET_BOOL_IMPL(Online, online)

/* readonly attribute boolean available; */
PURPLE_PRESENCE_GET_BOOL_IMPL(Available, available)

/* readonly attribute boolean idle; */
PURPLE_PRESENCE_GET_BOOL_IMPL(Idle, idle)

/* readonly attribute string status; */
NS_IMETHODIMP purpleAccountBuddy::GetStatus(nsACString &aStatus)
{
  if (!mBuddy) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  PurplePlugin *prpl = purple_find_prpl(purple_account_get_protocol_id(mBuddy->account));
  NS_ENSURE_ARG_POINTER(prpl);

  PurplePluginProtocolInfo *prpl_info = PURPLE_PLUGIN_PROTOCOL_INFO(prpl);
  NS_ENSURE_ARG_POINTER(prpl_info);

  if (prpl_info->status_text && mBuddy->account->gc) {
    char *tmp1 = prpl_info->status_text(mBuddy);
    char *tmp2 = purple_unescape_html(tmp1);
    aStatus = tmp2;
    g_free(tmp1);
    g_free(tmp2);
  }
  else
    aStatus = "";

  return NS_OK;
}
