/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "purpleAccountBuddy.h"
#include "purpleCoreService.h"
#include "purpleGetText.h"
#include "purpleGListEnumerator.h"
#include "purpleTooltipInfo.h"

#include <nsServiceManagerUtils.h>
#include <nsIClassInfoImpl.h>
#include <nsMemory.h>
#include <nsNetUtil.h>
#include <prprf.h>
#include "mozilla/Likely.h"

#pragma GCC visibility push(default)
#include <libpurple/core.h>
#include <libpurple/status.h>
#pragma GCC visibility pop

NS_IMPL_CLASSINFO(purpleAccountBuddy, NULL, 0, PURPLE_ACCOUNTBUDDY_CID)
NS_IMPL_ISUPPORTS1_CI(purpleAccountBuddy, imIAccountBuddy)

#ifdef PR_LOGGING
//
// NSPR_LOG_MODULES=purpleAccountBuddy:5
//
static PRLogModuleInfo *gPurpleAccountBuddyLog = nullptr;
#endif
#define LOG(args) PR_LOG(gPurpleAccountBuddyLog, PR_LOG_DEBUG, args)

purpleAccountBuddy::purpleAccountBuddy(PurpleBuddy *aPurpleBuddy,
                                       imIBuddy *aBuddy)
  : mPurpleBuddy(aPurpleBuddy),
    mBuddy(aBuddy)
{
  /* member initializers and constructor code */
  MOZ_COUNT_CTOR(purpleAccountBuddy);
#ifdef PR_LOGGING
  if (!gPurpleAccountBuddyLog)
    gPurpleAccountBuddyLog = PR_NewLogModule("purpleAccountBuddy");
#endif

  purple_blist_node_set_ui_data(PURPLE_BLIST_NODE(aPurpleBuddy), this);
  PurpleAccount *account = purple_buddy_get_account(aPurpleBuddy);
  purpleAccount *pAccount = purpleAccount::fromPurpleAccount(account);
  if (MOZ_LIKELY(pAccount))
    pAccount->GetImAccount(getter_AddRefs(mAccount));
  NS_ASSERTION(mAccount, "Failed to get the account from a PurpleBuddy");
}

purpleAccountBuddy::~purpleAccountBuddy()
{
  /* destructor code */
  MOZ_COUNT_DTOR(purpleAccountBuddy);
  UnInit();
}

/* void unInit (); */
NS_IMETHODIMP purpleAccountBuddy::UnInit()
{
  if (mPurpleBuddy) {
    if (purple_get_core())
      purple_blist_node_set_ui_data(PURPLE_BLIST_NODE(mPurpleBuddy), nullptr);
    mPurpleBuddy = NULL;
  }
  mAccount = nullptr;
  mBuddy = nullptr;
  return NS_OK;
}

/* attribute imIBuddy buddy; */
NS_IMETHODIMP purpleAccountBuddy::GetBuddy(imIBuddy * *aBuddy)
{
  NS_ENSURE_TRUE(mBuddy, NS_ERROR_NOT_INITIALIZED);

  NS_ADDREF(*aBuddy = mBuddy);
  return NS_OK;
}
NS_IMETHODIMP purpleAccountBuddy::SetBuddy(imIBuddy * aBuddy)
{
  NS_ENSURE_TRUE(!mBuddy, NS_ERROR_ALREADY_INITIALIZED);

  mBuddy = aBuddy;
  return NS_OK;
}

/* readonly attribute imIAccount account; */
NS_IMETHODIMP purpleAccountBuddy::GetAccount(imIAccount * *aAccount)
{
  NS_ENSURE_TRUE(mAccount, NS_ERROR_NOT_INITIALIZED);

  NS_ADDREF(*aAccount = mAccount);
  return NS_OK;
}

PurpleGroup *purpleAccountBuddy::GetPurpleGroupForTag(imITag *aTag)
{
  nsCString name;
  nsresult rv = aTag->GetName(name);
  NS_ENSURE_SUCCESS(rv, NULL);

  // creating an already existing group will return the existing group
  PurpleGroup *group = purple_group_new(name.get());
  NS_ENSURE_TRUE(group, NULL);

  if (!GET_NODE_UI_DATA(group)) {
    // The group is new, set ui_data to the tag.
    purple_blist_node_set_ui_data(PURPLE_BLIST_NODE(group), aTag);
    NS_ADDREF(aTag);

    purple_blist_add_group(group, NULL);
  }

  return group;
}

/* attribute purpleITag tag; */
NS_IMETHODIMP purpleAccountBuddy::GetTag(imITag * *aTag)
{
  NS_ENSURE_TRUE(mPurpleBuddy, NS_ERROR_NOT_INITIALIZED);

  PurpleGroup *group = purple_buddy_get_group(mPurpleBuddy);
  imITag *tag = GetTagFromPurpleGroup(group);
  if (!tag) {
    // The group is new, create a tag for it, and set ui_data on the group node
    nsCOMPtr<imITagsService> its =
      do_GetService("@mozilla.org/chat/tags-service;1");
    NS_ENSURE_TRUE(its, NS_ERROR_UNEXPECTED);

    nsCString groupName(purple_group_get_name(group));
    nsresult rv = its->CreateTag(groupName, &tag);
    NS_ENSURE_SUCCESS(rv, rv);
    NS_ENSURE_TRUE(tag, NS_ERROR_UNEXPECTED);

    // The CreateTag function adds a reference that we keep for the ui_data
    purple_blist_node_set_ui_data(PURPLE_BLIST_NODE(group), tag);
  }

  NS_ADDREF(*aTag = tag);
  return NS_OK;
}
NS_IMETHODIMP purpleAccountBuddy::SetTag(imITag * aTag)
{
  NS_ENSURE_ARG_POINTER(aTag);
  NS_ENSURE_TRUE(mBuddy, NS_ERROR_NOT_INITIALIZED);

  PurpleGroup *oldGroup = purple_buddy_get_group(mPurpleBuddy);
  imITag *oldTag = GetTagFromPurpleGroup(oldGroup);

  PurpleContact *contact =
    PURPLE_CONTACT(PURPLE_BLIST_NODE(mPurpleBuddy)->parent);
  PurpleGroup *group = GetPurpleGroupForTag(aTag);

  purple_blist_add_contact(contact, group, NULL);

  nsCOMPtr<imIContactsService> contacts =
    do_GetService("@mozilla.org/chat/contacts-service;1");
  NS_ENSURE_TRUE(contacts, NS_ERROR_UNEXPECTED);

  return contacts->AccountBuddyMoved(this, oldTag, aTag);
}

/* readonly attribute AUTF8String userName; */
NS_IMETHODIMP purpleAccountBuddy::GetUserName(nsACString& aUserName)
{
  NS_ENSURE_TRUE(mPurpleBuddy, NS_ERROR_NOT_INITIALIZED);

  aUserName = purple_buddy_get_name(mPurpleBuddy);
  return NS_OK;
}

/* readonly attribute AUTF8String normalizedName; */
NS_IMETHODIMP purpleAccountBuddy::GetNormalizedName(nsACString& aNormalizedName)
{
  NS_ENSURE_TRUE(mPurpleBuddy, NS_ERROR_NOT_INITIALIZED);

  aNormalizedName = purple_normalize(purple_buddy_get_account(mPurpleBuddy),
                                     purple_buddy_get_name(mPurpleBuddy));
  return NS_OK;
}

/* readonly attribute AUTF8String buddyIconFilename; */
NS_IMETHODIMP purpleAccountBuddy::GetBuddyIconFilename(nsACString& aBuddyIconFilename)
{
  NS_ENSURE_TRUE(mPurpleBuddy, NS_ERROR_NOT_INITIALIZED);

  PurpleBuddyIcon *icon = purple_buddy_get_icon(mPurpleBuddy);
  char *fname;

  if (!icon || !(fname = purple_buddy_icon_get_full_path(icon))) {
    aBuddyIconFilename.Truncate();
    return NS_OK;
  }

  // Create a nsIFile and then a nsIFileURI from that.
  nsCOMPtr <nsIFile> iconFile;
  nsresult rv = NS_NewLocalFile(NS_ConvertUTF8toUTF16(fname), PR_TRUE,
                                getter_AddRefs(iconFile));
  g_free(fname);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIURI> fileURI;
  rv = NS_NewFileURI(getter_AddRefs(fileURI), iconFile);
  NS_ENSURE_SUCCESS(rv, rv);

  return fileURI->GetSpec(aBuddyIconFilename);
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
       mPurpleBuddy->name, name.get()));
#endif

  PurpleAccount *pAccount = purple_buddy_get_account(mPurpleBuddy);
  NS_ENSURE_TRUE(pAccount, NS_ERROR_NOT_INITIALIZED);
  if (!purple_account_is_connected(pAccount)) {
    *aTooltipInfo = nullptr;
    return NS_OK;
  }

  purpleAccount *account = purpleAccount::fromPurpleAccount(pAccount);
  NS_ENSURE_TRUE(account, NS_ERROR_NOT_INITIALIZED);

  *aTooltipInfo = nullptr;
  PurplePluginProtocolInfo *prpl_info = account->GetPrplInfo();
  if (prpl_info && prpl_info->tooltip_text) {
    PurpleNotifyUserInfo *user_info = purple_notify_user_info_new();

    /* Idle */
    PurplePresence *presence = purple_buddy_get_presence(mPurpleBuddy);
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

    prpl_info->tooltip_text(mPurpleBuddy, user_info, true);
    purpleGListEnumerator *enumerator = new purpleGListEnumerator();
    enumerator->Init(purple_notify_user_info_get_entries(user_info),
                     purpleTypeToInterface<purpleTooltipInfo,
                                           prplITooltipInfo,
                                           PurpleNotifyUserInfoEntry>,
                     CleanUserInfo, user_info);
    NS_ADDREF(*aTooltipInfo = enumerator);
  }

  return NS_OK;
}

/* readonly attribute AUTF8String displayName; */
NS_IMETHODIMP purpleAccountBuddy::GetDisplayName(nsACString& aDisplayName)
{
  NS_ENSURE_TRUE(mPurpleBuddy, NS_ERROR_NOT_INITIALIZED);

  aDisplayName = purple_buddy_get_alias(mPurpleBuddy);
  return NS_OK;
}

/* attribute AUTF8String serverAlias; */
NS_IMETHODIMP purpleAccountBuddy::GetServerAlias(nsACString& aServerAlias)
{
  NS_ENSURE_TRUE(mPurpleBuddy, NS_ERROR_NOT_INITIALIZED);

  aServerAlias = purple_buddy_get_alias(mPurpleBuddy);
  return NS_OK;
}
NS_IMETHODIMP purpleAccountBuddy::SetServerAlias(const nsACString& aServerAlias)
{
  nsString wideOldServerAlias;
  NS_CStringToUTF16(nsDependentCString(purple_buddy_get_alias(mPurpleBuddy)),
                    NS_CSTRING_ENCODING_UTF8, wideOldServerAlias);

  purple_blist_alias_buddy(mPurpleBuddy,
                           PromiseFlatCString(aServerAlias).get());
  serv_alias_buddy(mPurpleBuddy);
  return NotifyObservers("account-buddy-display-name-changed",
                         wideOldServerAlias.get());
}

nsresult purpleAccountBuddy::NotifyObservers(const char* aSignal,
                                             const PRUnichar *aData)
{
  NS_ENSURE_ARG(aSignal);
  NS_ENSURE_TRUE(mBuddy, NS_ERROR_NOT_INITIALIZED);

  return mBuddy->Observe(this, aSignal, aData);
}

/* prplIConversation createConversation (); */
NS_IMETHODIMP purpleAccountBuddy::CreateConversation(prplIConversation **aResult)
{
  NS_ENSURE_TRUE(mAccount && mPurpleBuddy, NS_ERROR_NOT_INITIALIZED);

  nsCString buddyName(purple_buddy_get_name(mPurpleBuddy));
  return mAccount->CreateConversation(buddyName, aResult);
}

/* readonly attribute boolean canSendMessage; */
NS_IMETHODIMP purpleAccountBuddy::GetCanSendMessage(bool *aCanSendMessage)
{
  NS_ENSURE_TRUE(mPurpleBuddy, NS_ERROR_NOT_INITIALIZED);

  *aCanSendMessage =
    purple_account_is_connected(purple_buddy_get_account(mPurpleBuddy)) &&
    (purple_presence_is_online(purple_buddy_get_presence(mPurpleBuddy)) ||
     purple_account_supports_offline_message(purple_buddy_get_account(mPurpleBuddy),
                                             mPurpleBuddy));
  return NS_OK;
}

/* readonly attribute long statusType; */
NS_IMETHODIMP purpleAccountBuddy::GetStatusType(PRInt32 *aStatusType)
{
  NS_ENSURE_TRUE(mAccount && mPurpleBuddy, NS_ERROR_NOT_INITIALIZED);

  if (!purple_account_is_connected(purple_buddy_get_account(mPurpleBuddy))) {
    *aStatusType = STATUS_UNKNOWN;
    return NS_OK;
  }

  PurplePresence *presence = purple_buddy_get_presence(mPurpleBuddy);
  if (!purple_presence_is_online(presence))
    *aStatusType = STATUS_OFFLINE;
  else if (purple_presence_is_idle(presence))
    *aStatusType = STATUS_IDLE;
  else if (purple_presence_is_status_primitive_active(presence, PURPLE_STATUS_MOBILE))
    *aStatusType = STATUS_MOBILE;
  else if (purple_presence_is_available(presence))
    *aStatusType = STATUS_AVAILABLE;
  else if (purple_presence_is_status_primitive_active(presence, PURPLE_STATUS_AWAY) ||
           purple_presence_is_status_primitive_active(presence, PURPLE_STATUS_EXTENDED_AWAY))
    *aStatusType = STATUS_AWAY;
  else
    *aStatusType = STATUS_UNAVAILABLE;
  return NS_OK;
}

#define PURPLE_PRESENCE_GET_BOOL_IMPL(aName, aFctName)                        \
  NS_IMETHODIMP purpleAccountBuddy::Get##aName(bool *a##aName)                \
  {                                                                           \
    NS_ENSURE_TRUE(mAccount && mPurpleBuddy, NS_ERROR_NOT_INITIALIZED);       \
                                                                              \
    *a##aName =                                                               \
      purple_account_is_connected(purple_buddy_get_account(mPurpleBuddy)) &&  \
      purple_presence_is_##aFctName(purple_buddy_get_presence(mPurpleBuddy)); \
    return NS_OK;                                                             \
  }

/* readonly attribute boolean online; */
PURPLE_PRESENCE_GET_BOOL_IMPL(Online, online)

/* readonly attribute boolean available; */
PURPLE_PRESENCE_GET_BOOL_IMPL(Available, available)

/* readonly attribute boolean idle; */
PURPLE_PRESENCE_GET_BOOL_IMPL(Idle, idle)

/* readonly attribute boolean mobile; */
NS_IMETHODIMP purpleAccountBuddy::GetMobile(bool *aMobile)
{
  NS_ENSURE_TRUE(mBuddy, NS_ERROR_NOT_INITIALIZED);

  PurplePresence *presence = purple_buddy_get_presence(mPurpleBuddy);
  *aMobile = purple_presence_is_status_primitive_active(presence,
                                                        PURPLE_STATUS_MOBILE);
  return NS_OK;
}

/* readonly attribute string status; */
NS_IMETHODIMP purpleAccountBuddy::GetStatusText(nsACString &aStatusText)
{
  NS_ENSURE_TRUE(mPurpleBuddy, NS_ERROR_NOT_INITIALIZED);

  PurpleAccount *pAccount = purple_buddy_get_account(mPurpleBuddy);
  NS_ENSURE_TRUE(pAccount, NS_ERROR_NOT_INITIALIZED);

  if (!purple_account_is_connected(pAccount)) {
    aStatusText.Truncate();
    return NS_OK;
  }

  purpleAccount *account = purpleAccount::fromPurpleAccount(pAccount);
  NS_ENSURE_TRUE(account, NS_ERROR_NOT_INITIALIZED);

  aStatusText.Truncate();
  PurplePluginProtocolInfo *prpl_info = account->GetPrplInfo();
  if (prpl_info && prpl_info->status_text) {
    char *tmp1 = prpl_info->status_text(mPurpleBuddy);
    char *tmp2 = purple_unescape_html(tmp1);
    aStatusText = tmp2;
    g_free(tmp1);
    g_free(tmp2);
  }

  return NS_OK;
}

/* readonly attribute long availabilityDetails; */
NS_IMETHODIMP purpleAccountBuddy::GetAvailabilityDetails(PRInt32 *aAvailabilityDetails)
{
  *aAvailabilityDetails = 0; // FIXME
  return NS_OK;
}

/* void remove (); */
NS_IMETHODIMP purpleAccountBuddy::Remove()
{
  NS_ENSURE_TRUE(mPurpleBuddy, NS_ERROR_NOT_INITIALIZED);

  purple_account_remove_buddy(mPurpleBuddy->account, mPurpleBuddy,
                              purple_buddy_get_group(mPurpleBuddy));
  purple_blist_remove_buddy(mPurpleBuddy);
  return NS_OK;
}
