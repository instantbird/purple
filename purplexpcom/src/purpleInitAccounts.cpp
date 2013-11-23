/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#pragma GCC visibility push(default)
#include <glib.h>
#include <libpurple/core.h>
#pragma GCC visibility pop

#include "purpleAccount.h"
#include "prplIRequest.h"
#include <nsServiceManagerUtils.h>
#include <nsCOMPtr.h>
#include <nsIDOMWindow.h>
#include <nsIObserverService.h>
#include <nsIPromptService.h>
#include <nsIStringBundle.h>
#include <nsThreadUtils.h>
#include <nsIWindowMediator.h>
#include "mozilla/Likely.h"

#ifdef PR_LOGGING
//
// NSPR_LOG_MODULES=purpleInit:5
//
extern PRLogModuleInfo *gPurpleInitLog;
#endif
#define LOG(args) PR_LOG(gPurpleInitLog, PR_LOG_DEBUG, args)

extern void buddy_availability_changed(PurpleBuddy *aBuddy);

/*** Connection uiops ***/
#define IMPL_GET_ACCOUNT_FROM_UI_DATA(aVar)                        \
  purpleAccount *account = purpleAccount::fromPurpleAccount(aVar); \
  NS_ENSURE_TRUE(account,)

#define IMPL_GET_ACCOUNT                                        \
  PurpleAccount *pAccount = purple_connection_get_account(gc);  \
  NS_ENSURE_TRUE(pAccount,);                                    \
  IMPL_GET_ACCOUNT_FROM_UI_DATA(pAccount)

static void connecting(PurpleAccount *aAccount)
{
  IMPL_GET_ACCOUNT_FROM_UI_DATA(aAccount);
  account->Connecting(EmptyCString());
}
static void connect_progress(PurpleConnection *gc, const char *text,
                             size_t step, size_t step_count)
{
  IMPL_GET_ACCOUNT;
  account->Connecting(nsDependentCString(text));
}
static void connected(PurpleConnection *gc)
{
  IMPL_GET_ACCOUNT;
  account->Connected();

  for (PurpleBlistNode *gnode = purple_blist_get_root(); gnode; gnode = gnode->next) {
    if (!PURPLE_BLIST_NODE_IS_GROUP(gnode))
      continue;
    for (PurpleBlistNode *cnode = gnode->child; cnode; cnode = cnode->next) {
      if (!PURPLE_BLIST_NODE_IS_CONTACT(cnode))
        continue;
      for (PurpleBlistNode *bnode = cnode->child; bnode; bnode = bnode->next) {
        if (!PURPLE_BLIST_NODE_IS_BUDDY(bnode))
          continue;
        PurpleBuddy *buddy = (PurpleBuddy*)bnode;
        if (buddy->account == pAccount)
          buddy_availability_changed(buddy);
      }
    }
  }
}
static void disconnected(PurpleConnection *gc)
{
  IMPL_GET_ACCOUNT;
  account->Disconnected();
}
static void disconnecting(PurpleConnection *gc)
{
  IMPL_GET_ACCOUNT;
  account->Disconnecting(prplIAccount::NO_ERROR, EmptyCString());
}
static void report_disconnect_reason(PurpleConnection *gc,
                                     PurpleConnectionError reason,
                                     const char *text)
{
  IMPL_GET_ACCOUNT;
  account->Disconnecting(reason, nsDependentCString(text));
}

static PurpleConnectionUiOps connection_uiops = {
  connect_progress,
  connected,
  disconnected,
  NULL, /* notice = dead code ? */
  NULL, /* report_disconnect: deprecated in favour of report_disconnect_reason */
  NULL, /* network_connected */
  NULL, /* network_disconnected */
  report_disconnect_reason,

  NULL, NULL, NULL
};

static void connect_to_connections_signals()
{
  int handle;
  void *instance = purple_connections_get_handle();
  purple_signal_connect(instance, "signing-off", &handle,
                        PURPLE_CALLBACK(disconnecting), NULL);
}

static void before_status_change(PurpleAccount *aAccount,
                                 PurpleStatus *old_status,
                                 PurpleStatus *new_status)
{
  IMPL_GET_ACCOUNT_FROM_UI_DATA(aAccount);
  account->EnterScope();
}

static void after_status_change(PurpleAccount *aAccount,
                                PurpleStatus *old_status,
                                PurpleStatus *new_status)
{
  IMPL_GET_ACCOUNT_FROM_UI_DATA(aAccount);
  account->ExitScope();
}

static void connect_to_accounts_signals()
{
  int handle;
  void *instance = purple_accounts_get_handle();
  purple_signal_connect(instance, "account-connecting", &handle,
                        PURPLE_CALLBACK(connecting), NULL);
  purple_signal_connect(instance, "account-status-changing", &handle,
                        PURPLE_CALLBACK(before_status_change), NULL);
  purple_signal_connect(instance, "account-status-changed", &handle,
                        PURPLE_CALLBACK(after_status_change), NULL);
}


/*** Account uiops ***/
class purpleAuthorizationRequest MOZ_FINAL : public prplIBuddyRequest
{
  public:
    NS_DECL_ISUPPORTS
    NS_DECL_PRPLIBUDDYREQUEST

    purpleAuthorizationRequest(purpleAccount *aAccount,
                               const char *remote_user,
                               PurpleAccountRequestAuthorizationCb authorize_cb,
                               PurpleAccountRequestAuthorizationCb deny_cb,
                               void *user_data);
    void Cancel();


  private:
    nsCOMPtr<purpleAccount> mAccount;
    nsCString mRemoteUser;
    PurpleAccountRequestAuthorizationCb mAuthorizeCb, mDenyCb;
    void *mUserData;
};

NS_IMPL_ISUPPORTS1(purpleAuthorizationRequest, prplIBuddyRequest)

purpleAuthorizationRequest::purpleAuthorizationRequest(purpleAccount *aAccount,
                                                       const char *aRemoteUser,
                                                       PurpleAccountRequestAuthorizationCb authorize_cb,
                                                       PurpleAccountRequestAuthorizationCb deny_cb,
                                                       void *user_data)
  : mAccount(aAccount),
    mRemoteUser(aRemoteUser),
    mAuthorizeCb(authorize_cb),
    mDenyCb(deny_cb),
    mUserData(user_data)
{
  nsCOMPtr<nsIObserverService> os =
    do_GetService(NS_OBSERVERSERVICE_CONTRACTID);
  if (MOZ_LIKELY(os))
    os->NotifyObservers(this, "buddy-authorization-request", nullptr);
}

void purpleAuthorizationRequest::Cancel()
{
  mAuthorizeCb = NULL;
  mDenyCb = NULL;
  mUserData = NULL;

  nsCOMPtr<nsIObserverService> os =
    do_GetService(NS_OBSERVERSERVICE_CONTRACTID);
  if (MOZ_LIKELY(os))
    os->NotifyObservers(this, "buddy-authorization-request-canceled", nullptr);
}

/* readonly attribute imIAccount account; */
NS_IMETHODIMP purpleAuthorizationRequest::GetAccount(imIAccount * *aAccount)
{
  NS_ENSURE_TRUE(mAccount, NS_ERROR_NOT_INITIALIZED);

  return mAccount->GetImAccount(aAccount);
}

/* readonly attribute AUTF8String userName; */
NS_IMETHODIMP purpleAuthorizationRequest::GetUserName(nsACString & aUserName)
{
  aUserName = mRemoteUser;
  return NS_OK;
}

/* void grant (); */
NS_IMETHODIMP purpleAuthorizationRequest::Grant()
{
  if (mAuthorizeCb)
    mAuthorizeCb(mUserData);
  //TODO Prompt to add to blist
  return NS_OK;
}

/* void deny (); */
NS_IMETHODIMP purpleAuthorizationRequest::Deny()
{
  if (mDenyCb)
    mDenyCb(mUserData);
  return NS_OK;
}


static void *request_authorize(PurpleAccount *account, const char *remote_user,
                               const char *id, const char *alias,
                               const char *message, gboolean on_list,
                               PurpleAccountRequestAuthorizationCb authorize_cb,
                               PurpleAccountRequestAuthorizationCb deny_cb,
                               void *user_data)
{
  purpleAccount *pAccount = purpleAccount::fromPurpleAccount(account);
  if (MOZ_UNLIKELY(!pAccount))
    return NULL;
  return new purpleAuthorizationRequest(pAccount, remote_user, authorize_cb,
                                        deny_cb, user_data);
}

static void close_account_request(void *ui_handle)
{
  NS_ENSURE_TRUE(ui_handle, );

  ((purpleAuthorizationRequest *)ui_handle)->Cancel();
}

static void set_account_int(PurpleAccount *aAccount,
                            const char *aName, int aValue)
{
  IMPL_GET_ACCOUNT_FROM_UI_DATA(aAccount);
  account->SetIntPref(aName, aValue);
}
static void set_account_string(PurpleAccount *aAccount,
                               const char *aName, const char *aValue)
{
  IMPL_GET_ACCOUNT_FROM_UI_DATA(aAccount);
  account->SetStringPref(aName, aValue ? aValue : "");
}
static void set_account_bool(PurpleAccount *aAccount,
                             const char *aName, gboolean aValue)
{
  IMPL_GET_ACCOUNT_FROM_UI_DATA(aAccount);
  account->SetBoolPref(aName, aValue);
}

static PurpleAccountUiOps account_uiops = {
  NULL, /* notify_added, nearly dead code ? */
  NULL, /* status_changed */
  NULL, /* request_add, nearly dead */
  request_authorize,
  close_account_request,

  set_account_int,
  set_account_string,
  set_account_bool,

  NULL, NULL, NULL, NULL
};

void init_libpurple_accounts()
{
  purple_connections_set_ui_ops(&connection_uiops);
  purple_accounts_set_ui_ops(&account_uiops);

  connect_to_connections_signals();
  connect_to_accounts_signals();
}
