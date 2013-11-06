/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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
#include <prtime.h> // for PR_Now

#ifdef PR_LOGGING
//
// NSPR_LOG_MODULES=purpleConversation:5
//
PRLogModuleInfo *gPurpleConvLog = nullptr;
#endif
#define LOG(args) PR_LOG(gPurpleConvLog, PR_LOG_DEBUG, args)

purpleConversation::purpleConversation()
  : mId(0),
    mStartDate(PR_Now()),
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
NS_IMETHODIMP purpleConversation::GetIsChat(bool *aIsChat)
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
  NS_ENSURE_TRUE(mConv, nullptr);

  PurpleAccount *pAccount = purple_conversation_get_account(mConv);
  NS_ENSURE_TRUE(pAccount, nullptr);

  return purpleAccount::fromPurpleAccount(pAccount);
}

/* readonly attribute imIAccount account; */
NS_IMETHODIMP purpleConversation::GetAccount(imIAccount * *aAccount)
{
  NS_ENSURE_TRUE(mConv, NS_ERROR_NOT_INITIALIZED);

  PurpleAccount *pAccount = purple_conversation_get_account(mConv);
  NS_ENSURE_TRUE(pAccount, NS_ERROR_FAILURE);

  purpleAccount *account = purpleAccount::fromPurpleAccount(pAccount);
  NS_ENSURE_TRUE(account, NS_ERROR_FAILURE);

  return account->GetImAccount(aAccount);
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

/* readonly attribute PRTime startDate; */
NS_IMETHODIMP purpleConversation::GetStartDate(PRTime *aStartDate)
{
  *aStartDate = mStartDate;
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
NS_IMETHODIMP purpleConversation::SendTyping(const nsACString & aString, int32_t *_retval)
{
  *_retval = purpleConversation::NO_TYPING_LIMIT;
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

  os->NotifyObservers(this, "closing-conversation", nullptr);

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
