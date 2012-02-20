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
class purpleAuthorizationRequest : public prplIBuddyRequest
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
  if (NS_LIKELY(os))
    os->NotifyObservers(this, "buddy-authorization-request", nsnull);
}

void purpleAuthorizationRequest::Cancel()
{
  mAuthorizeCb = NULL;
  mDenyCb = NULL;
  mUserData = NULL;

  nsCOMPtr<nsIObserverService> os =
    do_GetService(NS_OBSERVERSERVICE_CONTRACTID);
  if (NS_LIKELY(os))
    os->NotifyObservers(this, "buddy-authorization-request-canceled", nsnull);
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
  if (NS_UNLIKELY(!pAccount))
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
