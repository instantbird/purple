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
#include "purpleStorage.h"
#include "purpleGetText.h"
#include "purpleGListEnumerator.h"
#include "purpleTooltipInfo.h"
#include "purpleLog.h"

#include <nsServiceManagerUtils.h>
#include <nsIClassInfoImpl.h>
#include <nsMemory.h>
#include <prprf.h>

#pragma GCC visibility push(default)
#include <libpurple/status.h>
#pragma GCC visibility pop

NS_IMPL_ISUPPORTS1_CI(purpleAccountBuddy, purpleIAccountBuddy)

#ifdef PR_LOGGING
//
// NSPR_LOG_MODULES=purpleAccountBuddy:5
//
static PRLogModuleInfo *gPurpleAccountBuddyLog = nsnull;
#endif
#define LOG(args) PR_LOG(gPurpleAccountBuddyLog, PR_LOG_DEBUG, args)

purpleAccountBuddy::purpleAccountBuddy()
{
  /* member initializers and constructor code */
#ifdef PR_LOGGING
  if (!gPurpleAccountBuddyLog)
    gPurpleAccountBuddyLog = PR_NewLogModule("purpleAccountBuddy");
#endif
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
  // aTag is unused because we don't cache the tag here any more, we
  // use the ui data of the libpurple buddy list node instead.
  // ToDo for 0.3: see if we should remove the 'tag' parameters in most
  // 'AddAccount' methods.
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
  PRInt32 count = PR_sscanf(key.get(), ACCOUNT_KEY "%u", &accountId);
  NS_ENSURE_TRUE(count == 1, NS_ERROR_UNEXPECTED);

  nsCOMPtr<purpleITag> tag;
  rv = GetTag(getter_AddRefs(tag));
  NS_ENSURE_SUCCESS(rv, rv);

  PRInt32 groupId;
  rv = tag->GetId(&groupId);
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

  LOG(("purpleAccountBuddy::Store(buddyId = %i, groupId = %i)",
       aBuddyId, groupId));

  return statement->Execute();
}

/* readonly attribute purpleIBuddy buddy; */
NS_IMETHODIMP purpleAccountBuddy::GetBuddy(purpleIBuddy * *aBuddy)
{
  NS_ENSURE_TRUE(mBuddy, NS_ERROR_NOT_INITIALIZED);

  nsCOMPtr<purpleICoreService> pcs =
    do_GetService(PURPLE_CORE_SERVICE_CONTRACTID);
  return pcs->GetBuddyByPurpleBuddy(mBuddy, aBuddy);
}

/* readonly attribute purpleIAccount account; */
NS_IMETHODIMP purpleAccountBuddy::GetAccount(purpleIAccount * *aAccount)
{
  NS_ENSURE_TRUE(mAccount, NS_ERROR_NOT_INITIALIZED);

  NS_ADDREF(*aAccount = mAccount);
  return NS_OK;
}

/* attribute purpleITag tag; */
NS_IMETHODIMP purpleAccountBuddy::GetTag(purpleITag * *aTag)
{
  NS_ENSURE_TRUE(mBuddy, NS_ERROR_NOT_INITIALIZED);

  PurpleGroup *group = purple_buddy_get_group(mBuddy);
  purpleTag *tag =
    (purpleTag *)purple_blist_node_get_ui_data((PurpleBlistNode *)group);
  NS_ENSURE_TRUE(tag, NS_ERROR_NOT_INITIALIZED);

  NS_ADDREF(*aTag = tag);
  return NS_OK;
}
NS_IMETHODIMP purpleAccountBuddy::SetTag(purpleITag * aTag)
{
  NS_ENSURE_ARG_POINTER(aTag);
  NS_ENSURE_TRUE(mBuddy, NS_ERROR_NOT_INITIALIZED);

  static_cast<purpleTag *>(aTag)->addBuddy(mBuddy);

  // Now collect the data we need to make the SQL UPDATE query
  // First, the tag id
  nsCOMPtr<purpleITag> tag;
  nsresult rv = GetTag(getter_AddRefs(tag));
  NS_ENSURE_SUCCESS(rv, rv);

  PRInt32 tagId;
  rv = tag->GetId(&tagId);
  NS_ENSURE_SUCCESS(rv, rv);

  // Then the account id
  nsCString key;
  rv = mAccount->GetId(key);
  NS_ENSURE_SUCCESS(rv, rv);

  PRInt32 accountId;
  PRInt32 count = PR_sscanf(key.get(), ACCOUNT_KEY "%u", &accountId);
  NS_ENSURE_TRUE(count == 1, NS_ERROR_UNEXPECTED);

  // And finally the buddy id
  nsCOMPtr<purpleICoreService> pcs =
    do_GetService(PURPLE_CORE_SERVICE_CONTRACTID);
  NS_ENSURE_TRUE(pcs, NS_ERROR_UNEXPECTED);

  nsCOMPtr<purpleIBuddy> buddy;
  rv = pcs->GetBuddyByPurpleBuddy(mBuddy, getter_AddRefs(buddy));
  NS_ENSURE_SUCCESS(rv, rv);

  PRInt32 buddyId;
  rv = buddy->GetId(&buddyId);
  NS_ENSURE_SUCCESS(rv, rv);

  // Get the mozStorage stuff
  purpleStorage *storageInstance = purpleStorage::GetInstance();
  NS_ENSURE_TRUE(storageInstance, NS_ERROR_OUT_OF_MEMORY);

  mozIStorageConnection *DBConn = storageInstance->GetConnection();
  NS_ENSURE_TRUE(DBConn, NS_ERROR_UNEXPECTED);

  // Finally, do our query!
  nsCOMPtr<mozIStorageStatement> statement;
  rv = DBConn->CreateStatement(NS_LITERAL_CSTRING(
    "UPDATE account_buddy SET tag_id = ?1 "
    "WHERE account_id = ?2 AND buddy_id = ?3"),
    getter_AddRefs(statement));
  NS_ENSURE_SUCCESS(rv, rv);
  rv = statement->BindInt32Parameter(0, tagId);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = statement->BindInt64Parameter(1, accountId);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = statement->BindInt64Parameter(2, buddyId);
  NS_ENSURE_SUCCESS(rv, rv);
  return statement->Execute();
}

/* readonly attribute AUTF8String buddyName; */
NS_IMETHODIMP purpleAccountBuddy::GetBuddyName(nsACString& aBuddyName)
{
  NS_ENSURE_TRUE(mBuddy, NS_ERROR_NOT_INITIALIZED);

  aBuddyName = purple_buddy_get_name(mBuddy);
  return NS_OK;
}

/* readonly attribute AUTF8String loggedIn */
NS_IMETHODIMP purpleAccountBuddy::GetLoggedIn(nsACString& aLoggedIn)
{
  NS_ENSURE_TRUE(mBuddy, NS_ERROR_NOT_INITIALIZED);

  PurplePresence *presence = purple_buddy_get_presence(mBuddy);
  time_t loginTime = purple_presence_get_login_time(presence);

  if ((PURPLE_BUDDY_IS_ONLINE(mBuddy)) && (loginTime > 0)) {
    time_t now = time(NULL);

    if (loginTime > now)
      aLoggedIn = purple_date_format_long(localtime(&loginTime));
    else {
      // returns a copy - we need to free it
      char *tmp = purple_str_seconds_to_string(now - loginTime);
      aLoggedIn = tmp;
      g_free(tmp);
    }
  }
  else {
    // didn't find a valid time - set it to NULL
    aLoggedIn.SetIsVoid(true);
  }

  return NS_OK;
}

/* readonly attribute AUTF8String buddyIconFilename; */
NS_IMETHODIMP purpleAccountBuddy::GetBuddyIconFilename(nsACString& aBuddyIconFilename)
{
  NS_ENSURE_TRUE(mBuddy, NS_ERROR_NOT_INITIALIZED);

  PurpleBuddyIcon *icon = purple_buddy_get_icon(mBuddy);
  char *fname;

  if (icon && (fname = purple_buddy_icon_get_full_path(icon))) {
    aBuddyIconFilename = NS_LITERAL_CSTRING("file://");
    aBuddyIconFilename.Append(fname);
    g_free(fname);
  } else {
    // set it to NULL
    aBuddyIconFilename.SetIsVoid(true);
  }

  return NS_OK;
}

void purpleAccountBuddy::CleanUserInfo(void *aData)
{
  if (aData)
    purple_notify_user_info_destroy((PurpleNotifyUserInfo *)aData);
}

/*   nsISimpleEnumerator getTooltipInfo(); */
NS_IMETHODIMP purpleAccountBuddy::GetTooltipInfo(nsISimpleEnumerator** aTooltipInfo)
{
  NS_ENSURE_TRUE(mBuddy, NS_ERROR_NOT_INITIALIZED);
  NS_ENSURE_TRUE(mAccount, NS_ERROR_NOT_INITIALIZED);

#ifdef PR_LOGGING
  nsCString name;
  mAccount->GetName(name);
  LOG(("purpleAccountBuddy::GetTooltipInfo buddy = %s, account = %s",
       mBuddy->name, name.get()));
#endif

  nsCOMPtr<purpleIProtocol> proto;
  nsresult rv = mAccount->GetProtocol(getter_AddRefs(proto));
  NS_ENSURE_SUCCESS(rv, rv);

  PurplePluginProtocolInfo *prpl_info;
  rv = proto->GetInfo(&prpl_info);
  NS_ENSURE_SUCCESS(rv, rv);

  if (prpl_info->tooltip_text && mBuddy->account->gc) {
    PurpleNotifyUserInfo *user_info = purple_notify_user_info_new();

    /* Idle */
    PurplePresence *presence = purple_buddy_get_presence(mBuddy);
    if (purple_presence_is_idle(presence)) {
      time_t idle_secs = purple_presence_get_idle_time(presence);
      if (idle_secs > 0) {
        char *tmp = purple_str_seconds_to_string(time(NULL) - idle_secs);
        purple_notify_user_info_add_pair(user_info,
                                         purpleGetText::GetText("purple", "Idle"),
                                         tmp);
        g_free(tmp);
      }
    }

    prpl_info->tooltip_text(mBuddy, user_info, true);
    purpleGListEnumerator *enumerator = new purpleGListEnumerator();
    enumerator->Init(purple_notify_user_info_get_entries(user_info),
                     purpleTypeToInterface<purpleTooltipInfo,
                                           purpleITooltipInfo,
                                           PurpleNotifyUserInfoEntry>,
                     CleanUserInfo, user_info);
    NS_ADDREF(*aTooltipInfo = enumerator);
    return NS_OK;
  }

  *aTooltipInfo = nsnull;
  return NS_OK;
}

/* readonly attribute AUTF8String buddyAlias; */
NS_IMETHODIMP purpleAccountBuddy::GetBuddyAlias(nsACString& aBuddyAlias)
{
  NS_ENSURE_TRUE(mBuddy, NS_ERROR_NOT_INITIALIZED);

  aBuddyAlias = purple_buddy_get_alias(mBuddy);
  return NS_OK;
}
void purpleAccountBuddy::SetBuddyAlias(const nsCString& aBuddyAlias)
{
  purple_blist_alias_buddy(mBuddy, aBuddyAlias.get());
  serv_alias_buddy(mBuddy);
}

/* purpleIConversation createConversation (); */
NS_IMETHODIMP purpleAccountBuddy::CreateConversation(purpleIConversation **aResult)
{
  NS_ENSURE_TRUE(mAccount && mBuddy, NS_ERROR_NOT_INITIALIZED);

  nsCString buddyName(purple_buddy_get_name(mBuddy));
  return mAccount->CreateConversation(buddyName, aResult);
}

/* readonly attribute boolean canSendMessage; */
NS_IMETHODIMP purpleAccountBuddy::GetCanSendMessage(PRBool *aCanSendMessage)
{
  NS_ENSURE_TRUE(mBuddy, NS_ERROR_NOT_INITIALIZED);

  *aCanSendMessage =
    purple_presence_is_online(purple_buddy_get_presence(mBuddy)) ||
    purple_account_supports_offline_message(purple_buddy_get_account(mBuddy),
                                            mBuddy);
  return NS_OK;
}

#define PURPLE_PRESENCE_GET_BOOL_IMPL(aName, aFctName)                  \
  NS_IMETHODIMP purpleAccountBuddy::Get##aName(PRBool *a##aName)        \
  {                                                                     \
    NS_ENSURE_TRUE(mAccount && mBuddy, NS_ERROR_NOT_INITIALIZED);       \
                                                                        \
    *a##aName =                                                         \
      purple_account_is_connected(purple_buddy_get_account(mBuddy)) &&  \
      purple_presence_is_##aFctName(purple_buddy_get_presence(mBuddy)); \
    return NS_OK;                                                       \
  }

/* readonly attribute boolean online; */
PURPLE_PRESENCE_GET_BOOL_IMPL(Online, online)

/* readonly attribute boolean available; */
PURPLE_PRESENCE_GET_BOOL_IMPL(Available, available)

/* readonly attribute boolean idle; */
PURPLE_PRESENCE_GET_BOOL_IMPL(Idle, idle)

/* readonly attribute boolean mobile; */
NS_IMETHODIMP purpleAccountBuddy::GetMobile(PRBool *aMobile)
{
  NS_ENSURE_TRUE(mBuddy, NS_ERROR_NOT_INITIALIZED);

  *aMobile =
    purple_presence_is_status_primitive_active(purple_buddy_get_presence(mBuddy),
                                               PURPLE_STATUS_MOBILE);
  return NS_OK;
}

/* readonly attribute string status; */
NS_IMETHODIMP purpleAccountBuddy::GetStatus(nsACString &aStatus)
{
  NS_ENSURE_TRUE(mBuddy, NS_ERROR_NOT_INITIALIZED);

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

/* nsISimpleEnumerator getLogs (); */
NS_IMETHODIMP purpleAccountBuddy::GetLogs(nsISimpleEnumerator **aResult)
{
  NS_ENSURE_TRUE(mBuddy, NS_ERROR_NOT_INITIALIZED);

  GList *logs = purple_log_get_logs(PURPLE_LOG_IM,
                                    purple_buddy_get_name(mBuddy),
                                    mBuddy->account);
  purpleGListEnumerator *enumerator = new purpleGListEnumerator();
  enumerator->Init(logs,
                   purpleTypeToInterface<purpleLog, purpleILog, PurpleLog>,
                   (void (*)(void*))g_list_free, logs);
  NS_ADDREF(*aResult = enumerator);
  return NS_OK;
}
