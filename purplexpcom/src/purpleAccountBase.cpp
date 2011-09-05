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

#include "purpleAccountBase.h"
#include "purpleCoreService.h"
#include "purpleGListEnumerator.h"
#include "purpleStorage.h"
#include <nsIObserverService.h>
#include <nsIPrefService.h>
#include <nsIClassInfoImpl.h>
#include <nsServiceManagerUtils.h>
#include <nsComponentManagerUtils.h>
#include <nsNetUtil.h>
#include <prprf.h>

NS_IMPL_CLASSINFO(purpleAccountBase, NULL, 0, PURPLE_ACCOUNT_CID)
NS_IMPL_ISUPPORTS2_CI(purpleAccountBase, purpleIAccount,
                      purpleIAccountBase)

#define AUTO_SAVE_PREFS_TIMER   5000

#define TIME_BEFORE_CONSIDERING_CONNECTION_AS_SUCCESSFUL 10

#ifdef PR_LOGGING
//
// NSPR_LOG_MODULES=purpleAccountBase:5
//
static PRLogModuleInfo *gPurpleAccountBaseLog = nsnull;
#endif
#define LOG(args) PR_LOG(gPurpleAccountBaseLog, PR_LOG_DEBUG, args)


nsCOMPtr<nsITimer> purpleAccountBase::sSavePrefsTimer;
PRUint32 purpleAccountBase::sAutoLoginsPending = 0;

purpleAccountBase::purpleAccountBase()
  : mHasValidProtocol(PR_TRUE),
    mConnectionState(STATE_DISCONNECTED),
    mConnectionErrorReason((PurpleConnectionError)NO_ERROR),
    mReconnectAttempts(0),
    mTimeOfNextReconnect(0),
    mTimeOfLastConnect(0),
    mAutoLoginPending(PR_FALSE)
{
  /* member initializers and constructor code */
#ifdef PR_LOGGING
  if (!gPurpleAccountBaseLog)
    gPurpleAccountBaseLog = PR_NewLogModule("purpleAccountBase");
#endif
  LOG(("Creating purpleAccountBase @%x\n", this));
}

purpleAccountBase::~purpleAccountBase()
{
  /* destructor code */
  LOG(("Destructing purpleAccountBase @%x\n", this));
  if (mId)
    UnInit();
}

/* void UnInit (); */
NS_IMETHODIMP purpleAccountBase::UnInit()
{
  if (mTimer) {
    mTimer->Cancel();
    mTimer = nsnull;
  }

  // remove any pending autologin preference used for crash detection
  if (mAutoLoginPending)
    finishedAutoLogin();

  // If the first connection was pending on quit, we set it back to "set"
  CheckAndSetFirstConnectionState(FIRST_CONNECTION_SET);

  // and make sure we cleanup the save pref timer
  if (sSavePrefsTimer)
    savePrefsNow(nsnull, nsnull);

  mId = 0;
  mConcreteAccount = nsnull;
  return NS_OK;
}

/* void init (in AUTF8String aKey, in AUTF8String aName); */
nsresult purpleAccountBase::Init(const nsACString & aKey,
                                 const nsACString & aName,
                                 purpleIProtocol *aPrpl)
{
  NS_ENSURE_ARG_POINTER(aPrpl);

  nsresult rv = SetId(aKey);
  NS_ENSURE_SUCCESS(rv, rv);
  mName = aName;
  mProtocol = aPrpl;
  LOG(("initializing account : %s (%s)\n", mKey.get(), mName.get()));

  rv = GetPrefBranch();
  NS_ENSURE_SUCCESS(rv, rv);

  if (!mHasValidProtocol) {
    mConnectionErrorReason = (PurpleConnectionError) ERROR_UNKNOWN_PRPL;
    return NS_OK;
  }

  /* We previously crashed while connecting an account individually. */
  PRInt16 currentState;
  if (NS_SUCCEEDED(GetFirstConnectionState(&currentState)) &&
      (currentState == FIRST_CONNECTION_PENDING ||
       currentState == FIRST_CONNECTION_CRASHED)) {
    SetFirstConnectionState(FIRST_CONNECTION_CRASHED);
    mConnectionErrorReason = (PurpleConnectionError) ERROR_CRASHED;
  }

  /* Check if a password is required and missing */
  nsCString pass;
  rv = mPrefBranch->GetCharPref(PREF_PASSWORD, getter_Copies(pass));
  if (pass.IsEmpty())
    SetMissingPasswordIfRequired();

  return NS_OK;
}

void purpleAccountBase::startAutoLogin()
{
  NS_ENSURE_TRUE(!mAutoLoginPending, );
  LOG(("purpleAccountBase::startAutoLogin"));
  mAutoLoginPending = PR_TRUE;

  if (!sAutoLoginsPending++) {
    nsCOMPtr<nsIPrefBranch> prefs = do_GetService(NS_PREFSERVICE_CONTRACTID);
    NS_ENSURE_TRUE(prefs, );

    PRTime now = PR_Now() / PR_USEC_PER_SEC;
    nsresult rv = prefs->SetIntPref(PREF_AUTOLOGIN_PENDING, (PRInt32)now);
    NS_ENSURE_SUCCESS(rv, );

    LOG(("Added autoLoginPending pref, with value %i", now));
    savePrefsNow(nsnull, nsnull);
  }
}

void purpleAccountBase::finishedAutoLogin()
{
  if (!mAutoLoginPending) {
    LOG(("purpleAccountBase::finishedAutoLogin mAutoLoginPending wasn't set"));
    return;
  }

  LOG(("finishedAutoLogin"));
  mAutoLoginPending = PR_FALSE;

  if (!--sAutoLoginsPending) {
    nsCOMPtr<nsIPrefBranch> prefs = do_GetService(NS_PREFSERVICE_CONTRACTID);
    NS_ENSURE_TRUE(prefs, );
    nsresult rv = prefs->DeleteBranch(PREF_AUTOLOGIN_PENDING);
    NS_ENSURE_SUCCESS(rv, );

    initSavePrefsTimer();
  }
}

/* Function called to change the state of firstConnectionState
   Called on connecting, disconnecting, crashed, Uninit or connected to set or reset
   the value of firstConnectionState.
   For example, this function prevents from setting firstConnectionState to
   FIRST_CONNECTION_PENDING while connecting and the account is already reliable. */
inline void purpleAccountBase::CheckAndSetFirstConnectionState(const PRInt16 aState)
{
  PRInt16 currentState;
  if (NS_FAILED(GetFirstConnectionState(&currentState)) ||
      currentState == FIRST_CONNECTION_NONE || currentState == aState ||
      (currentState == FIRST_CONNECTION_CRASHED && aState == FIRST_CONNECTION_SET))
    return;

  LOG(("purpleAccountBase::CheckAndSetFirstConnectionState"));
  LOG(("\tOld State: %d\tAsked State: %d", currentState, aState));

  nsresult rv = SetFirstConnectionState(aState);
  NS_ENSURE_SUCCESS(rv, );
}

/* void checkAutoLogin (); */
NS_IMETHODIMP purpleAccountBase::CheckAutoLogin()
{
  NS_ENSURE_TRUE(mId, NS_ERROR_NOT_INITIALIZED);
  NS_ENSURE_TRUE(mConcreteAccount, NS_ERROR_NOT_INITIALIZED);

  /* If we don't have a valid protocol or the passwordi is missing,
     we can't autologin so don't bother checking the value of the pref */
  if (!mHasValidProtocol ||
      mConnectionErrorReason == (PurpleConnectionError)ERROR_MISSING_PASSWORD)
    return NS_OK;

  /* We don't want to connect an already connecting account. */
  if (mConnectionState == STATE_CONNECTING)
    return NS_OK;

  PRInt16 currentState;
  if (NS_SUCCEEDED(GetFirstConnectionState(&currentState)) &&
      currentState == FIRST_CONNECTION_CRASHED)
    return NS_OK;

  /* Check if we should connect the account automatically */
  PRBool autoLogin;
  nsresult rv = GetPrefBranch();
  if (NS_SUCCEEDED(rv))
    rv = mPrefBranch->GetBoolPref(PREF_AUTOLOGIN, &autoLogin);
  if (NS_FAILED(rv) || autoLogin) {
    LOG(("autologin\n"));
    startAutoLogin();
    rv = mConcreteAccount->Connect();
#ifdef DEBUG
    if (NS_FAILED(rv))
      finishedAutoLogin();
#endif
  }

  mPrefBranch = nsnull;
  return NS_OK;
}

nsresult purpleAccountBase::unstoreAccount()
{
  LOG(("Removing account %u from mozStorage\n", mId));
  purpleStorage *storageInstance = purpleStorage::GetInstance();
  NS_ENSURE_TRUE(storageInstance, NS_ERROR_OUT_OF_MEMORY);

  mozIStorageConnection *DBConn = storageInstance->GetConnection();
  nsCOMPtr<mozIStorageStatement> statement;
  nsresult rv = DBConn->CreateStatement(NS_LITERAL_CSTRING(
    "DELETE FROM accounts WHERE id = ?1"),
    getter_AddRefs(statement));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = statement->BindInt32Parameter(0, mId);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = statement->Execute();
  NS_ASSERTION(NS_SUCCEEDED(rv), "removing account failed");
  NS_ENSURE_SUCCESS(rv, rv);

  // removing the account from the accounts table is not enought,
  // we need to remove all the associated account_buddy entries too
  LOG(("Removing account_buddy entries of account %u from mozStorage\n", mId));
  rv = DBConn->CreateStatement(NS_LITERAL_CSTRING(
    "DELETE FROM account_buddy WHERE account_id = ?1"),
    getter_AddRefs(statement));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = statement->BindInt32Parameter(0, mId);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = statement->Execute();
  NS_ASSERTION(NS_SUCCEEDED(rv), "removing account_buddy failed");

  return rv;
}

/* void remove (); */
NS_IMETHODIMP purpleAccountBase::Remove()
{
  nsresult rv = GetPrefBranch();
  NS_ENSURE_SUCCESS(rv, rv);

#ifdef DEBUG
  nsCString protoId;
  rv = NS_ERROR_FAILURE;
  if (mProtocol)
    rv = mProtocol->GetId(protoId);
  LOG(("Deleting purpleAccountBase %s (%s)\n",
       !mName.IsEmpty() ? mName.get() : "(mName = NULL)",
       NS_SUCCEEDED(rv) ? protoId.get() : "mProtocol = NULL"));
#endif

  /* Remove data from the pref branch */
  rv = mPrefBranch->DeleteBranch("");
  NS_ENSURE_SUCCESS(rv, rv); //XXX Should we really return here?

  /* Remove the data from the accounts table in mozStorage */
  unstoreAccount();

  /* Cleanup timers, autoreconnect and anti-crash stuff */
  return UnInit();
}

/* void connect (); */
NS_IMETHODIMP purpleAccountBase::Connect()
{
  NS_NOTREACHED("purpleAccountBase::Connect");
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* void disconnect (); */
NS_IMETHODIMP purpleAccountBase::Disconnect()
{
  NS_NOTREACHED("purpleAccountBase::Disconnect");
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* void cancelReconnection(); */
NS_IMETHODIMP purpleAccountBase::CancelReconnection()
{
  if (mTimer) {
    mTimer->Cancel();
    mTimer = nsnull;
  }

  mReconnectAttempts = 0;
  mTimeOfNextReconnect = 0;
  return NS_OK;
}

/* purpleIConversation createConversation (in AUTF8String aName); */
NS_IMETHODIMP purpleAccountBase::CreateConversation(const nsACString& aName,
                                                    purpleIConversation **aResult)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* void addBuddy (in imITag aTag, in AUTF8String aName); */
NS_IMETHODIMP purpleAccountBase::AddBuddy(imITag *aTag,
                                          const nsACString& aName)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* imIAccountBuddy LoadBuddy (in imIBuddy aBuddy, in imITag aTag); */
NS_IMETHODIMP purpleAccountBase::LoadBuddy(imIBuddy *aBuddy,
                                           imITag *aTag,
                                           imIAccountBuddy **aResult)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* void requestBuddyInfo (in AUTF8String aBuddyName); */
NS_IMETHODIMP purpleAccountBase::RequestBuddyInfo(const nsACString & aBuddyName)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* nsISimpleEnumerator getChatRoomFields (); */
NS_IMETHODIMP purpleAccountBase::GetChatRoomFields(nsISimpleEnumerator **aResult)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* readonly attribute boolean canJoinChat; */
NS_IMETHODIMP purpleAccountBase::GetCanJoinChat(PRBool *aCanJoinChat)
{
  *aCanJoinChat = PR_FALSE;
  return NS_OK;
}

/* purpleIChatRoomFieldValues getChatRoomDefaultFieldValues ([optional] in AUTF8String aDefaultChatName); */
NS_IMETHODIMP
purpleAccountBase::GetChatRoomDefaultFieldValues(const nsACString & aDefaultChatName,
                                                 purpleIChatRoomFieldValues **aResult)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* void joinChat (in purpleIChatRoomFieldValues aComponents); */
NS_IMETHODIMP purpleAccountBase::JoinChat(purpleIChatRoomFieldValues *aComponents)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* readonly attribute AUTF8String name; */
NS_IMETHODIMP purpleAccountBase::GetName(nsACString& aName)
{
  NS_ENSURE_TRUE(!mName.IsEmpty(), NS_ERROR_NOT_INITIALIZED);

  aName = mName;
  return NS_OK;
}

/* readonly attribute AUTF8String normalizedName; */
NS_IMETHODIMP purpleAccountBase::GetNormalizedName(nsACString& aNormalizedName)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* readonly attribute AUTF8String id; */
NS_IMETHODIMP purpleAccountBase::GetId(nsACString& aId)
{
  NS_ENSURE_TRUE(!mKey.IsEmpty(), NS_ERROR_NOT_INITIALIZED);

  aId = mKey;
  return NS_OK;
}

/* readonly attribute PRUint32 numericId; */
NS_IMETHODIMP purpleAccountBase::GetNumericId(PRUint32 *aNumericId)
{
  NS_ENSURE_TRUE(mId, NS_ERROR_NOT_INITIALIZED);

  *aNumericId = mId;
  return NS_OK;
}

nsresult purpleAccountBase::SetId(const nsACString& aId)
{
  /* Save the id of the account */
  mKey = aId;

  PRInt32 count = PR_sscanf(mKey.get(), ACCOUNT_KEY "%u", &mId);
  NS_ENSURE_TRUE(count == 1, NS_ERROR_FAILURE);

  return NS_OK;
}

nsresult purpleAccountBase::sendNotification(const char *aNotification)
{
  nsCOMPtr<nsIObserverService> os =
    do_GetService(NS_OBSERVERSERVICE_CONTRACTID);
  return os->NotifyObservers(mConcreteAccount, aNotification, nsnull);
}

nsresult purpleAccountBase::sendUpdateNotification()
{
  return sendNotification("account-updated");
}

nsresult purpleAccountBase::GetPrefBranch()
{
  /* if we already have the pref branch, return early */
  if (mPrefBranch) {
    return NS_OK;
  }

  /* create the string for the root of the pref branch */
  nsCString root(PREF_ACCOUNT_PREFIX);
  root.Append(mKey);
  root.Append('.');

  /* get the branch */
  nsCOMPtr<nsIPrefService> prefs = do_GetService(NS_PREFSERVICE_CONTRACTID);
  NS_ENSURE_TRUE(prefs, NS_ERROR_UNEXPECTED);

  nsresult rv = prefs->GetBranch(root.get(), getter_AddRefs(mPrefBranch));
  NS_ENSURE_SUCCESS(rv, rv);

  /* get the options pref branch */
  root.Append(PREF_OPTIONS);
  return prefs->GetBranch(root.get(), getter_AddRefs(mPrefOptBranch));
}

/* readonly attribute purpleIProtocol protocol; */
NS_IMETHODIMP purpleAccountBase::GetProtocol(purpleIProtocol * *aProtocol)
{
  NS_ENSURE_TRUE(mProtocol, NS_ERROR_NOT_INITIALIZED);

  NS_ADDREF(*aProtocol = mProtocol);
  return NS_OK;
}

/* attribute boolean autoLogin; */
NS_IMETHODIMP purpleAccountBase::GetAutoLogin(PRBool *aAutoLogin)
{
  PURPLE_ENSURE_INIT(!mKey.IsEmpty());

  nsresult rv = GetPrefBranch();
  NS_ENSURE_SUCCESS(rv, rv);

  rv = mPrefBranch->GetBoolPref(PREF_AUTOLOGIN, aAutoLogin);
  /* If the pref isn't set, assume we want to sign-on at startup */
  if (NS_FAILED(rv)) {
    *aAutoLogin = PR_TRUE;
  }

  return NS_OK;
}
NS_IMETHODIMP purpleAccountBase::SetAutoLogin(PRBool aAutoLogin)
{
  PURPLE_ENSURE_INIT(mProtocol);

  nsresult rv = SetBoolPref(PREF_AUTOLOGIN, aAutoLogin);
  NS_ENSURE_SUCCESS(rv, rv);

  initSavePrefsTimer();
  return sendUpdateNotification();
}

/* attribute short firstConnectionState; */
NS_IMETHODIMP purpleAccountBase::GetFirstConnectionState(PRInt16 *aState)
{
  PURPLE_ENSURE_INIT(!mKey.IsEmpty());

  nsresult rv = GetPrefBranch();
  NS_ENSURE_SUCCESS(rv, rv);

  PRInt32 prefState;
  rv = mPrefBranch->GetIntPref(PREF_FIRST_CONNECTION_STATE, &prefState);

  /* If the pref does not exist, this is not the first connection. */
  if (NS_FAILED(rv))
    prefState = FIRST_CONNECTION_NONE;

  // GetIntPref takes a *PRInt32 as second argument and aState is *PRInt16
  *aState = static_cast<PRInt16>(prefState);
  return NS_OK;
}
NS_IMETHODIMP purpleAccountBase::SetFirstConnectionState(PRInt16 aState)
{
  PURPLE_ENSURE_INIT(mProtocol);

  nsresult rv = GetPrefBranch();
  NS_ENSURE_SUCCESS(rv, rv);

  if (aState != FIRST_CONNECTION_NONE)
    rv = mPrefBranch->SetIntPref(PREF_FIRST_CONNECTION_STATE, aState);
  else
    rv = mPrefBranch->DeleteBranch(PREF_FIRST_CONNECTION_STATE);

  // We want to save this pref immediately when trying to connect.
  if (aState == FIRST_CONNECTION_PENDING)
    savePrefsNow(nsnull, nsnull);
  else
    initSavePrefsTimer();
  return NS_OK;
}

/* attribute AUTF8String password; */
NS_IMETHODIMP purpleAccountBase::GetPassword(nsACString& aPassword)
{
  // Get the password from the preferences
  nsresult rv = GetPrefBranch();
  NS_ENSURE_SUCCESS(rv, rv);

  nsCString password;
  rv = mPrefBranch->GetCharPref(PREF_PASSWORD, getter_Copies(password));
  NS_ENSURE_SUCCESS(rv, rv);

  aPassword = password;
  return NS_OK;
}

void purpleAccountBase::SetMissingPasswordIfRequired()
{
  NS_ASSERTION(mHasValidProtocol && mProtocol,
               "SetMissingPasswordIfRequired without a valid protocol!");
  NS_ENSURE_TRUE(mHasValidProtocol && mProtocol, );

  PRBool noPassword = PR_TRUE;
  PRBool passwordOptional = PR_TRUE;
  if (NS_SUCCEEDED(mProtocol->GetNoPassword(&noPassword)) &&
      NS_SUCCEEDED(mProtocol->GetPasswordOptional(&passwordOptional)) &&
      !noPassword && !passwordOptional) {
    mConnectionErrorReason = (PurpleConnectionError)ERROR_MISSING_PASSWORD;
    LOG(("missing password\n"));
  }
}

NS_IMETHODIMP purpleAccountBase::SetPassword(const nsACString& aPassword)
{
  NS_ENSURE_TRUE(mId, NS_ERROR_NOT_INITIALIZED);

  PromiseFlatCString flatPassword(aPassword);
  const char *pass = flatPassword.get();
  SetStringPref(PREF_PASSWORD, pass);

  if (mHasValidProtocol && aPassword.IsEmpty())
    SetMissingPasswordIfRequired();
  else if (mConnectionErrorReason == (PurpleConnectionError)ERROR_MISSING_PASSWORD &&
           !aPassword.IsEmpty())
    mConnectionErrorReason = (PurpleConnectionError)NO_ERROR;

  return sendUpdateNotification();
}

/* attribute boolean rememberPassword; */
NS_IMETHODIMP purpleAccountBase::GetRememberPassword(PRBool *aRememberPassword)
{
  return NS_OK;
}
NS_IMETHODIMP purpleAccountBase::SetRememberPassword(PRBool aRememberPassword)
{
  return NS_OK;
}

/* attribute string alias; */
NS_IMETHODIMP purpleAccountBase::GetAlias(nsACString& aAlias)
{
  NS_ENSURE_TRUE(mId, NS_ERROR_NOT_INITIALIZED);

  // Get the alias from the preferences
  nsresult rv = GetPrefBranch();
  NS_ENSURE_SUCCESS(rv, rv);

  nsCString alias;
  rv = mPrefBranch->GetCharPref(PREF_ALIAS, getter_Copies(alias));
  aAlias = NS_SUCCEEDED(rv) ? alias : EmptyCString();
  return NS_OK;
}
NS_IMETHODIMP purpleAccountBase::SetAlias(const nsACString& aAlias)
{
  NS_ENSURE_TRUE(mId, NS_ERROR_NOT_INITIALIZED);

  if (!aAlias.IsEmpty()) {
    SetStringPref(PREF_ALIAS, PromiseFlatCString(aAlias).get());
  }
  else {
    nsresult rv = GetPrefBranch();
    if(NS_SUCCEEDED(rv)) {
      mPrefBranch->DeleteBranch(PREF_ALIAS);
    }
  }

  return sendUpdateNotification();
}

/* attribute purpleIProxyInfo proxyInfo; */
NS_IMETHODIMP purpleAccountBase::GetProxyInfo(purpleIProxyInfo * *aProxyInfo)
{
  return NS_ERROR_NOT_INITIALIZED;
}
NS_IMETHODIMP purpleAccountBase::SetProxyInfo(purpleIProxyInfo * aProxyInfo)
{
  return NS_OK;
}

#define PURPLE_IMPL_SET(prettyType, purpleType, PRType, prefType)       \
  NS_IMETHODIMP purpleAccountBase::Set##prettyType(const char *aName,   \
                                                   PRType aVal)         \
  {                                                                     \
    /* Save the pref in our preference system */                        \
    nsresult rv = GetPrefBranch();                                      \
    NS_ENSURE_SUCCESS(rv, rv);                                          \
                                                                        \
    return mPrefOptBranch->Set##prefType##Pref(aName, aVal);            \
  }

/* void setBool (in string aName, in boolean aVal); */
PURPLE_IMPL_SET(Bool, bool, PRBool, Bool)
/* void setInt (in string aName, in long aVal); */
PURPLE_IMPL_SET(Int, int, PRInt32, Int)
/* void setString (in string aName, in string aVal); */
PURPLE_IMPL_SET(String, string, const char *, Char)

void purpleAccountBase::initSavePrefsTimer()
{
  if (sSavePrefsTimer) {
    LOG(("purpleAccountBase::initSavePrefsTimer already initialized"));
    return;
  }
  sSavePrefsTimer = do_CreateInstance(NS_TIMER_CONTRACTID);
  sSavePrefsTimer->InitWithFuncCallback(savePrefsNow, nsnull,
                                        AUTO_SAVE_PREFS_TIMER,
                                        nsITimer::TYPE_ONE_SHOT);
  LOG(("purpleAccountBase::initSavePrefsTimer initialized"));
}

void purpleAccountBase::savePrefsNow(nsITimer *aTimer, void *aClosure)
{
  LOG(("purpleAccountBase::savePrefsNow()"));

  nsCOMPtr<nsIPrefService> prefs = do_GetService(NS_PREFSERVICE_CONTRACTID);
  if (prefs)
    prefs->SavePrefFile(nsnull);

  if (sSavePrefsTimer) {
    sSavePrefsTimer->Cancel();
    sSavePrefsTimer = nsnull;
  }
}

/* void savePrefs (); */
NS_IMETHODIMP purpleAccountBase::Save()
{
  NS_ENSURE_TRUE(mPrefBranch, NS_ERROR_UNEXPECTED);

  savePrefsNow(nsnull, nsnull);
  mPrefBranch = nsnull;
  return NS_OK;
}

#define PURPLE_IMPL_SETPREF(prettyType, prefType, PRType)               \
  nsresult purpleAccountBase::Set##prettyType##Pref(const char *aName,  \
                                                    PRType aValue)      \
  {                                                                     \
    nsresult rv = GetPrefBranch();                                      \
    NS_ENSURE_SUCCESS(rv, rv);                                          \
                                                                        \
    rv = mPrefBranch->Set##prefType##Pref(aName, aValue);               \
    NS_ENSURE_SUCCESS(rv, rv);                                          \
                                                                        \
    initSavePrefsTimer();                                               \
    return NS_OK;                                                       \
  }

// nsresult purpleAccountBase::SetBoolPref(const char *aName, PRBool aValue)
PURPLE_IMPL_SETPREF(Bool, Bool, PRBool)
// nsresult purpleAccountBase::SetIntPref(const char *aName, PRInt32 aValue)
PURPLE_IMPL_SETPREF(Int, Int, PRInt32)
// nsresult purpleAccountBase::SetStringPref(const char *aName, const char *aValue)
PURPLE_IMPL_SETPREF(String, Char, const char *)


/* attribute string connectionStateMsg; */
NS_IMETHODIMP purpleAccountBase::GetConnectionStateMsg(nsACString &aConnectionStateMsg)
{
  aConnectionStateMsg = mConnectionStateMsg;
  return NS_OK;
}

/* attribute purpleIAccount concreteAccount; */
NS_IMETHODIMP purpleAccountBase::GetConcreteAccount(purpleIAccount * *aConcreteAccount)
{
  NS_IF_ADDREF(*aConcreteAccount = mConcreteAccount);
  return NS_OK;
}
NS_IMETHODIMP purpleAccountBase::SetConcreteAccount(purpleIAccount * aConcreteAccount)
{
  mConcreteAccount = aConcreteAccount;
  return NS_OK;
}

/* void connecting ([optional] in AUTF8String aConnectionStateMessage); */
NS_IMETHODIMP purpleAccountBase::Connecting(const nsACString & aConnectionStateMessage)
{
  mConnectionStateMsg = aConnectionStateMessage;

  if (mConnectionState == STATE_CONNECTING)
    return sendNotification("account-connect-progress");

  if (mConnectionErrorReason != (PurpleConnectionError) NO_ERROR) {
    /* The account is offline because of an error, clear the error */
    LOG(("Reconnecting account and clearing the error"));
    mConnectionErrorReason = (PurpleConnectionError) NO_ERROR;
    mConnectionErrorMessage.Truncate();

    if (mTimeOfNextReconnect - PR_Now() > PRTime(PR_USEC_PER_SEC)) {
      LOG(("mReconnectAttempts := 0 because: mTimeOfNextReconnect - PR_Now() = %li > PR_USEC_PER_SEC",
           mTimeOfNextReconnect - PR_Now()));
      // This is a manual reconnection, reset the auto-reconnect stuff
      mTimeOfLastConnect = 0;
      CancelReconnection();
    }
  }

  // Sets the firstConnectionState pref as pending if needed.
  CheckAndSetFirstConnectionState(FIRST_CONNECTION_PENDING);

  mConnectionState = STATE_CONNECTING;
  return sendNotification("account-connecting");
}

/* void connected (); */
NS_IMETHODIMP purpleAccountBase::Connected()
{
  mConnectionState = STATE_CONNECTED;
  finishedAutoLogin();
  mTimeOfLastConnect = PR_Now();
  CheckAndSetFirstConnectionState(FIRST_CONNECTION_NONE);

  /* Probably no longer needed, but just for sanity:
     Rremove the current connection state message so that the UI
     doesn't interpret it as an error / the cause of the disconnection
     We used to get a bogus error message when the user disconnected
     manually an account while it was connecting (see bug 88) */
  mConnectionStateMsg.Truncate();

  return sendNotification("account-connected");
}

/* void disconnecting ([optional] in short aConnectionErrorReason,
                       [optional] in AUTF8String aConnectionErrorMessage); */
NS_IMETHODIMP purpleAccountBase::Disconnecting(PRInt16 aConnectionErrorReason,
                                               const nsACString & aConnectionErrorMessage)
{
  mConnectionState = STATE_DISCONNECTING;
  mConnectionErrorReason = (PurpleConnectionError) aConnectionErrorReason;
  mConnectionErrorMessage = aConnectionErrorMessage;
  mConnectionStateMsg.Truncate();

  finishedAutoLogin();
  CheckAndSetFirstConnectionState(FIRST_CONNECTION_SET);

  if (aConnectionErrorReason != NO_ERROR) {
    startReconnectTimer();
    sendNotification("account-connect-error");
  }
  return sendNotification("account-disconnecting");
}

/* void disconnected (); */
NS_IMETHODIMP purpleAccountBase::Disconnected()
{
  NS_ENSURE_TRUE(mConnectionState != STATE_DISCONNECTED, NS_ERROR_UNEXPECTED);
  mConnectionState = STATE_DISCONNECTED;

  return sendNotification("account-disconnected");
}

nsresult purpleAccountBase::startReconnectTimer()
{
  if (mConnectionErrorReason != (PurpleConnectionError)ERROR_NETWORK_ERROR &&
      mConnectionErrorReason != (PurpleConnectionError)ERROR_ENCRYPTION_ERROR)
    return NS_OK;

  NS_ENSURE_TRUE(mConcreteAccount, NS_ERROR_NOT_INITIALIZED);
  // Start the auto-reconnect process
  if (!mTimer)
    mTimer = do_CreateInstance(NS_TIMER_CONTRACTID);

  nsCString timers;
  nsCOMPtr<nsIPrefBranch> prefs = do_GetService(NS_PREFSERVICE_CONTRACTID);
  NS_ENSURE_TRUE(prefs, NS_ERROR_OUT_OF_MEMORY);
  nsresult rv = prefs->GetCharPref("messenger.accounts.reconnectTimer",
                                   getter_Copies(timers));
  NS_ENSURE_SUCCESS(rv, rv);

  /* If the last successful connection is older than 10 seconds, reset the
     number of reconnection attemps. */
  if (mTimeOfLastConnect && mTimeOfLastConnect +
      TIME_BEFORE_CONSIDERING_CONNECTION_AS_SUCCESSFUL * PR_USEC_PER_SEC < PR_Now()) {
    mReconnectAttempts = 0;
    mTimeOfLastConnect = 0;
  }

  char *newStr = timers.BeginWriting();
  nsCAutoString time;
  PRUint32 i = 0;
  for (char *token = NS_strtok(",", &newStr);
       i <= mReconnectAttempts && token;
       ++i, token = NS_strtok(",", &newStr)) {
    time = token;
    time.StripWhitespace();
  }

  LOG(("time: %s, attempt %i", time.get(), mReconnectAttempts + 1));
  PRInt32 timer = strtol(time.get(), NULL, 10);
  if (timer <= 0) {
    LOG(("No auto-reconnect"));
    return NS_OK;
  }
  ++mReconnectAttempts;
  mTimeOfNextReconnect = PR_Now() + timer * PR_USEC_PER_SEC;
  mTimer->InitWithFuncCallback(reconnectTimerCallback, mConcreteAccount,
                               timer * PR_MSEC_PER_SEC,
                               nsITimer::TYPE_ONE_SHOT);
  LOG(("timer initialized with %i s", timer));
  return NS_OK;
}

void purpleAccountBase::reconnectTimerCallback(nsITimer *aTimer, void *aClosure)
{
  LOG(("running reconnectTimerCallback"));
  static_cast<purpleIAccount *>(aClosure)->Connect();
}

/* readonly attribute short connectionErrorReason; */
NS_IMETHODIMP purpleAccountBase::GetConnectionErrorReason(PRInt16 *aConnectionErrorReason)
{
  *aConnectionErrorReason = mConnectionErrorReason;
  return NS_OK;
}

/* readonly attribute AUTF8String connectionErrorMessage; */
NS_IMETHODIMP purpleAccountBase::GetConnectionErrorMessage(nsACString & aConnectionErrorMessage)
{
  aConnectionErrorMessage = mConnectionErrorMessage;
  return NS_OK;
}

/* readonly attribute PRUint16 reconnectAttempt; */
NS_IMETHODIMP purpleAccountBase::GetReconnectAttempt(PRUint16 *aReconnectAttempt)
{
  *aReconnectAttempt = mReconnectAttempts;
  return NS_OK;
}

/* readonly attribute PRInt64 timeOfNextReconnect; */
NS_IMETHODIMP purpleAccountBase::GetTimeOfNextReconnect(PRInt64 *aTimeOfNextReconnect)
{
  *aTimeOfNextReconnect = mTimeOfNextReconnect / 1000;
  return NS_OK;
}

/* readonly attribute PRInt64 timeOfLastConnect; */
NS_IMETHODIMP purpleAccountBase::GetTimeOfLastConnect(PRInt64 *aTimeOfLastConnect)
{
  *aTimeOfLastConnect = mTimeOfLastConnect / 1000;
  return NS_OK;
}

/* readonly attribute short connectionState; */
NS_IMETHODIMP purpleAccountBase::GetConnectionState(PRInt16 *aConnectionState)
{
  *aConnectionState = mConnectionState;
  return NS_OK;
}

#define PURPLE_IMPL_GETBOOL(aName, aState)                      \
  NS_IMETHODIMP purpleAccountBase::Get##aName(PRBool *a##aName) \
  {                                                             \
    *a##aName = mConnectionState == STATE_##aState;             \
    return NS_OK;                                               \
  }

/* readonly attribute boolean disconnecting; */
PURPLE_IMPL_GETBOOL(Disconnecting, DISCONNECTING)

/* readonly attribute boolean disconnected; */
PURPLE_IMPL_GETBOOL(Disconnected, DISCONNECTED)

/* readonly attribute boolean connected; */
PURPLE_IMPL_GETBOOL(Connected, CONNECTED)

/* readonly attribute boolean connecting; */
PURPLE_IMPL_GETBOOL(Connecting, CONNECTING)

#define PURPLE_IMPL_GETFLAG(aName, aFlag, aDefaultValue)                \
  NS_IMETHODIMP purpleAccountBase::Get##aName(PRBool *a##aName)         \
  {                                                                     \
    *a##aName = aDefaultValue;                                          \
    return NS_OK;                                                       \
  }

/* readonly attribute boolean HTMLEnabled; */
PURPLE_IMPL_GETFLAG(HTMLEnabled, HTML, PR_TRUE)

/* readonly attribute boolean noBackgroundColors; */
PURPLE_IMPL_GETFLAG(NoBackgroundColors, NO_BGCOLOR, PR_TRUE)

/* readonly attribute boolean autoResponses; */
PURPLE_IMPL_GETFLAG(AutoResponses, AUTO_RESP, PR_FALSE)

/* readonly attribute boolean singleFormatting; */
PURPLE_IMPL_GETFLAG(SingleFormatting, FORMATTING_WBFO, PR_FALSE)

/* readonly attribute boolean noNewlines; */
PURPLE_IMPL_GETFLAG(NoNewlines, NO_NEWLINES, PR_FALSE)

/* readonly attribute boolean noFontSizes; */
PURPLE_IMPL_GETFLAG(NoFontSizes, NO_FONTSIZE, PR_FALSE)

/* readonly attribute boolean noUrlDesc; */
PURPLE_IMPL_GETFLAG(NoUrlDesc, NO_URLDESC, PR_FALSE)

/* readonly attribute boolean noImages; */
PURPLE_IMPL_GETFLAG(NoImages, NO_IMAGES, PR_TRUE)

/* readonly attribute PRInt32 maxMessageLength; */
NS_IMETHODIMP purpleAccountBase::GetMaxMessageLength(PRInt32 *aMaxMessageLength)
{
  *aMaxMessageLength = 0;
  return NS_OK;
}
