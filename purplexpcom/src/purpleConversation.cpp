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

#include "purpleConversation.h"
#include "purpleGListEnumerator.h"
#include "purpleAccount.h"

#pragma GCC visibility push(default)
#include <libpurple/cmds.h>
#include <libpurple/connection.h>
#pragma GCC visibility pop

#include <nsCOMPtr.h>
#include <nsServiceManagerUtils.h>
#include <nsStringAPI.h>
#include <nsXPCOM.h>
#include <nsIDocument.h>
#include <nsIDOMDocument.h>
#include <nsIObserverService.h>

#ifdef PR_LOGGING
//
// NSPR_LOG_MODULES=purpleConversation:5
//
PRLogModuleInfo *gPurpleConvLog = nsnull;
#endif
#define LOG(args) PR_LOG(gPurpleConvLog, PR_LOG_DEBUG, args)

purpleConversation::purpleConversation()
  : mId(0),
    mUninitialized(PR_FALSE)
{
  /* member initializers and constructor code */
#ifdef PR_LOGGING
  if (!gPurpleConvLog)
    gPurpleConvLog = PR_NewLogModule("purpleConversation");
#endif
  LOG(("new purpleConversation @%x with id %i", this, mId));
}

purpleConversation::~purpleConversation()
{
  /* destructor code */
  LOG(("destructing conversation @%x with id %i", this, mId));
}

/* readonly attribute boolean isChat; */
NS_IMETHODIMP purpleConversation::GetIsChat(PRBool *aIsChat)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

void purpleConversation::SetConv(PurpleConversation *aConv)
{
  mConv = aConv;
  LOG(("conversion ptr set to: @%x", mConv));
  aConv->ui_data = this;
}

/* call this only from the destroy_conv uiop or when exiting */
/* void unInit (); */
NS_IMETHODIMP purpleConversation::UnInit()
{
  LOG(("purpleConversation::unInit (mId == %i)", mId));
  NS_ENSURE_TRUE(mConv && !mUninitialized, NS_ERROR_NOT_INITIALIZED);

  mConv->ui_data = NULL;
  mConv = NULL;
  mUninitialized = PR_TRUE;
  return NS_OK;
}

purpleAccount *purpleConversation::GetAccount()
{
  NS_ENSURE_TRUE(mConv, nsnull);

  PurpleAccount *pAccount = purple_conversation_get_account(mConv);
  NS_ENSURE_TRUE(pAccount, nsnull);

  return purpleAccount::fromPurpleAccount(pAccount);
}

/* readonly attribute purpleIAccount account; */
NS_IMETHODIMP purpleConversation::GetAccount(purpleIAccount * *aAccount)
{
  NS_ENSURE_TRUE(mConv, NS_ERROR_NOT_INITIALIZED);

  PurpleAccount *pAccount = purple_conversation_get_account(mConv);
  NS_ENSURE_TRUE(pAccount, NS_ERROR_FAILURE);

  purpleAccount *account = purpleAccount::fromPurpleAccount(pAccount);
  NS_ENSURE_TRUE(account, NS_ERROR_FAILURE);

  return account->GetConcreteAccount(aAccount);
}

/* readonly attribute AUTF8String name; */
NS_IMETHODIMP purpleConversation::GetName(nsACString& aName)
{
  NS_ENSURE_TRUE(mConv, NS_ERROR_NOT_INITIALIZED);

  aName = purple_conversation_get_name(mConv);
  return NS_OK;
}

/* readonly attribute AUTF8String normalizedName; */
NS_IMETHODIMP purpleConversation::GetNormalizedName(nsACString& aNormalizedName)
{
  NS_ENSURE_TRUE(mConv, NS_ERROR_NOT_INITIALIZED);

  return NS_ERROR_NOT_IMPLEMENTED;
}

/* readonly attribute AUTF8String title; */
NS_IMETHODIMP purpleConversation::GetTitle(nsACString& aTitle)
{
  NS_ENSURE_TRUE(mConv, NS_ERROR_NOT_INITIALIZED);

  aTitle = purple_conversation_get_title(mConv);
  return NS_OK;
}

/* attribute unsigned long id; */
NS_IMETHODIMP purpleConversation::GetId(PRUint32 *aId)
{
  *aId = mId;
  return NS_OK;
}
NS_IMETHODIMP purpleConversation::SetId(PRUint32 aId)
{
  NS_ENSURE_ARG(aId);
  NS_ENSURE_TRUE(!mId, NS_ERROR_ALREADY_INITIALIZED);

  mId = aId;
  return NS_OK;
}

/* void sendMsg (in AUTF8String aMsg); */
NS_IMETHODIMP purpleConversation::SendMsg(const nsACString& aMsg)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* void sendTyping (in unsigned long aLength); */
NS_IMETHODIMP purpleConversation::SendTyping(PRUint32 aLength)
{
  return NS_OK;
}

/* void close (); */
NS_IMETHODIMP purpleConversation::Close()
{
  NS_ENSURE_TRUE(mConv || mUninitialized, NS_ERROR_NOT_INITIALIZED);

  LOG(("purpleConversation::Close (mId = %i)", mId));

  nsresult rv;
  nsCOMPtr<nsIObserverService> os =
    do_GetService(NS_OBSERVERSERVICE_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  os->NotifyObservers(this, "closing-conversation", nsnull);

  if (!mUninitialized) {
    purpleAccountScoper scoper(GetAccount());
    purple_conversation_destroy(mConv);
    mConv = NULL;
  }

  return NS_OK;
}

/* void addObserver (in nsIObserver aObserver); */
NS_IMETHODIMP purpleConversation::AddObserver(nsIObserver *aObserver)
{
  NS_ENSURE_ARG_POINTER(aObserver);
  NS_ENSURE_TRUE(mObservers.IndexOfObject(aObserver) == -1,
                 NS_ERROR_ALREADY_INITIALIZED);
  LOG(("purpleConversation::AddObserver (mId = %i, observer = @%x)",
      mId, aObserver));

  mObservers.AppendObject(aObserver);
  return NS_OK;
}

/* void removeObserver (in nsIObserver aObserver); */
NS_IMETHODIMP purpleConversation::RemoveObserver(nsIObserver *aObserver)
{
  NS_ENSURE_ARG_POINTER(aObserver);

  LOG(("purpleConversation::RemoveObserver (mId = %i, observer = @%x)",
       mId, aObserver));
  NS_ENSURE_TRUE(mObservers.RemoveObject(aObserver), NS_ERROR_FAILURE);
  return NS_OK;
}

void purpleConversation::NotifyObservers(nsISupports *aSubject,
                                         const char *aTopic,
                                         const PRUnichar *aData)
{
  LOG(("purpleConversation(mId = %i)::NotifyObservers(@%x, topic = %s)",
       mId, aSubject, aTopic));
  for (PRInt32 i = 0; i < mObservers.Count(); ++i)
    mObservers[i]->Observe(aSubject, aTopic, aData);
}
