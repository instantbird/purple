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

#include "purpleAccountsService.h"
#include "purpleAccount.h"
#include "purpleCoreService.h"
#include "purpleStorage.h"
#include "purpleUnknownProtocol.h"

#include <nsServiceManagerUtils.h>
#include <nsComponentManagerUtils.h>
#include <nsIObserverService.h>
#include <nsIConsoleService.h>
#include <nsArrayEnumerator.h>

#ifdef PR_LOGGING
#include <prprf.h>
//
// NSPR_LOG_MODULES=purpleAccountsService:5
//
static PRLogModuleInfo *gPurpleAccountsServiceLog = nsnull;
#endif
#define LOG(args) PR_LOG(gPurpleAccountsServiceLog, PR_LOG_DEBUG, args)

#define PREF_MESSENGER_ACCOUNTS "messenger.accounts"

NS_IMPL_ISUPPORTS2(purpleAccountsService, purpleIAccountsService, nsIObserver)

purpleAccountsService::purpleAccountsService()
  :mInitialized(PR_FALSE),
   mAccounts(nsnull)
{
#ifdef PR_LOGGING
  if (!gPurpleAccountsServiceLog)
    gPurpleAccountsServiceLog = PR_NewLogModule("purpleAccountsService");
#endif
  mPrefService = do_GetService(NS_PREFSERVICE_CONTRACTID);
  mPrefBranch = do_QueryInterface(mPrefService);
}

purpleAccountsService::~purpleAccountsService()
{
  if (mInitialized)
    UnInitAccounts();
}

/* nsIObserver implementation */
NS_IMETHODIMP purpleAccountsService::Observe(nsISupports *aSubject,
                                             const char *aTopic,
                                             const PRUnichar *aData)
{
  NS_ASSERTION(mInitialized, "Observing notification after uninitialization");

  if (!strcmp(NS_PREFBRANCH_PREFCHANGE_TOPIC_ID, aTopic) &&
      NS_LITERAL_STRING(PREF_MESSENGER_ACCOUNTS).Equals(aData)) {
      UpdateAccountList();
      return NS_OK;
    }

  return NS_ERROR_UNEXPECTED;
}

NS_IMETHODIMP purpleAccountsService::InitAccounts()
{
  /* We shouldn't load the list twice. */
  NS_ENSURE_TRUE(!mInitialized, NS_ERROR_ALREADY_INITIALIZED);

  /* Set to true before the end of the initialization because
     InitAccounts calls GetProtoById that ensures the core is
     initialized */
  mInitialized = PR_TRUE;

  /* Get the list of accounts */
  nsCString accountList;
  nsresult rv = mPrefBranch->GetCharPref(PREF_MESSENGER_ACCOUNTS,
                                         getter_Copies(accountList));
  NS_ENSURE_SUCCESS(rv, rv);

  LOG(("InitAccounts list = %s", accountList.get()));

  char *newStr = accountList.BeginWriting();
  nsCAutoString key;
  for (char *token = NS_strtok(",", &newStr);
       token;
       token = NS_strtok(",", &newStr)) {
    key = token;
    key.StripWhitespace();

    nsCOMPtr<purpleIAccount> account;
    rv = LoadAccount(key, getter_AddRefs(account));
    if (NS_SUCCEEDED(rv)) {
      /* Keep it in the local account list */
      mAccounts.AppendObject(account);
    }
  }

  /* Observe for changes in the order of accounts */
  rv = mPrefBranch->AddObserver(PREF_MESSENGER_ACCOUNTS, this, PR_FALSE);
  NS_ENSURE_SUCCESS(rv, rv);

  LOG(("Accounts service initialized"));
  return NS_OK;
}

nsresult purpleAccountsService::UpdateAccountList()
{
  PURPLE_ENSURE_INIT(mInitialized);

  /* Get the list of accounts */
  nsCString accountList;
  nsresult rv = mPrefBranch->GetCharPref(PREF_MESSENGER_ACCOUNTS,
                                         getter_Copies(accountList));
  NS_ENSURE_SUCCESS(rv, rv);

  LOG(("UpdateAccountList list = %s", accountList.get()));

  nsCOMPtr<purpleIAccount> account;
  char *newStr = accountList.BeginWriting();
  nsCAutoString key;
  PRInt32 i = 0;
  PRInt32 accountCount = mAccounts.Count();
  for (char *token = NS_strtok(",", &newStr);
       token;
       token = NS_strtok(",", &newStr)) {
    key = token;
    key.StripWhitespace();

    if (key.IsEmpty()) {
      continue;
    }

    for (PRInt32 j = i; j < accountCount; ++j) {
      nsCString id;
      if (NS_SUCCEEDED(mAccounts[j]->GetId(id)) && id.Equals(key)) {
        if (i < j) {
          account = mAccounts[j];
          mAccounts.RemoveObject(account);
          mAccounts.InsertObjectAt(account, i);
        }
        break;
      }
    }

    ++i;
  }

  return NotifyObservers(static_cast<purpleIAccountsService *>(this),
                         "account-list-updated");
}

NS_IMETHODIMP purpleAccountsService::UnInitAccounts()
{
  PURPLE_ENSURE_INIT(mInitialized);

  for (PRInt32 i = mAccounts.Count() - 1; i >= 0; --i) {
    mAccounts[i]->UnInit();
  }
  mAccounts.Clear();

  nsresult rv = mPrefBranch->RemoveObserver(PREF_MESSENGER_ACCOUNTS, this);
  NS_ENSURE_SUCCESS(rv, rv);

  mInitialized = PR_FALSE;
  return NS_OK;
}

/* purpleIAccount getAccountById (in AUTF8String accountId); */
NS_IMETHODIMP purpleAccountsService::GetAccountById(const nsACString& aAccountId, purpleIAccount **aResult)
{
  PURPLE_ENSURE_INIT(mInitialized);

  for (PRInt32 i = mAccounts.Count() - 1; i >= 0; --i) {
    nsCString id;
    if (NS_SUCCEEDED(mAccounts[i]->GetId(id)) && id.Equals(aAccountId)) {
      NS_ADDREF(*aResult = mAccounts[i]);
      return NS_OK;
    }
  }

  *aResult = nsnull;
  return NS_OK; // we don't want to throw an exception if called from JS
}

NS_IMETHODIMP purpleAccountsService::GetAccountByNumericId(PRUint32 aAccountId, purpleIAccount **aResult)
{
  PURPLE_ENSURE_INIT(mInitialized);

  for (PRInt32 i = mAccounts.Count() - 1; i >= 0; --i) {
    PRUint32 id;
    if (NS_SUCCEEDED(mAccounts[i]->GetNumericId(&id)) && id == aAccountId) {
      NS_ADDREF(*aResult = mAccounts[i]);
      return NS_OK;
    }
  }

  return NS_ERROR_FAILURE;
}

/* nsISimpleEnumerator getAccounts (); */
NS_IMETHODIMP purpleAccountsService::GetAccounts(nsISimpleEnumerator **aResult)
{
  PURPLE_ENSURE_INIT(mInitialized);

  return NS_NewArrayEnumerator(aResult, mAccounts);
}

nsresult purpleAccountsService::ensureStored(PRUint32 aId,
                                             const nsCString& aName,
                                             const nsCString& aPrpl)
{
  purpleStorage *storageInstance = purpleStorage::GetInstance();
  NS_ENSURE_TRUE(storageInstance, NS_ERROR_OUT_OF_MEMORY);

  LOG(("Checking that the account with the id %u in mozStorage has the name %s and the prpl %s\n",
       aId, aName.get(), aPrpl.get()));
  mozIStorageStatement *statement = storageInstance->mEnsureAccountStoredStatement;
  mozStorageStatementScoper scoper(statement);
  nsresult rv = statement->BindInt32Parameter(0, aId);
  NS_ENSURE_SUCCESS(rv, rv);

  PRBool hasMoreData;
  while (NS_SUCCEEDED(statement->ExecuteStep(&hasMoreData)) && hasMoreData) {
    nsCString name;
    rv = statement->GetUTF8String(0, name);
    NS_ENSURE_SUCCESS(rv, rv);
    nsCString prpl;
    rv = statement->GetUTF8String(1, prpl);
    NS_ENSURE_SUCCESS(rv, rv);

    LOG(("Account at the id %u in mozStorage has the name %s and the prpl %s\n",
         aId, name.get(), prpl.get()));
    NS_ENSURE_TRUE(name.Equals(aName) && prpl.Equals(aPrpl), NS_ERROR_FAILURE);

    LOG(("Checking account (%u, %s, %s) OK\n", aId, name.get(), prpl.get()));
    return NS_OK;
  }

  LOG(("account (%u, %s, %s) not found in mozStorage, attempting to store it\n",
       aId, aName.get(), aPrpl.get()));

  return storeAccount(aId, aName, aPrpl);
}

nsresult purpleAccountsService::storeAccount(PRUint32 aId,
                                             const nsCString& aName,
                                             const nsCString& aPrpl)
{
  LOG(("Adding account %u (%s, %s) in mozStorage\n",
       aId, aName.get(), aPrpl.get()));
  purpleStorage *storageInstance = purpleStorage::GetInstance();
  NS_ENSURE_TRUE(storageInstance, NS_ERROR_OUT_OF_MEMORY);

  mozIStorageConnection *DBConn = storageInstance->GetConnection();
  nsCOMPtr<mozIStorageStatement> statement;
  nsresult rv = DBConn->CreateStatement(NS_LITERAL_CSTRING(
    "INSERT INTO accounts (id, name, prpl) VALUES(?1, ?2, ?3)"),
    getter_AddRefs(statement));
  NS_ENSURE_SUCCESS(rv, rv);

  statement->BindInt32Parameter(0, aId);
  statement->BindUTF8StringParameter(1, aName);
  statement->BindUTF8StringParameter(2, aPrpl);

  rv = statement->Execute();
  NS_ASSERTION(NS_SUCCEEDED(rv), "adding account failed");
  return rv;
}

nsresult purpleAccountsService::LoadAccount(nsCString &key, purpleIAccount **aResult)
{
  NS_ENSURE_TRUE(!key.IsEmpty(), NS_ERROR_INVALID_ARG);
  LOG(("Attempting to load account %s", key.get()));

  nsCString prefRoot(PREF_ACCOUNT_PREFIX);
  prefRoot.Append(key);
  prefRoot.Append(".");

  nsCString prefName = prefRoot;
  prefName.Append(PREF_NAME);
  nsCString name;
  nsresult rv = mPrefBranch->GetCharPref(prefName.get(), getter_Copies(name));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCString prefPrpl = prefRoot;
  prefPrpl.Append(PREF_PRPL);
  nsCString prpl;
  rv = mPrefBranch->GetCharPref(prefPrpl.get(), getter_Copies(prpl));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<purpleICoreService> pcs =
    do_GetService(PURPLE_CORE_SERVICE_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<purpleIProtocol> proto;
  rv = pcs->GetProtocolById(prpl, getter_AddRefs(proto));
  if (NS_FAILED(rv) || !proto) {
    nsCOMPtr<purpleUnknownProtocol> unknown =
      do_CreateInstance(PURPLE_UNKNOWN_PROTOCOL_CONTRACTID);
    NS_ENSURE_TRUE(unknown, NS_ERROR_OUT_OF_MEMORY);
    unknown->Init(prpl);
    proto = unknown;
  }

  PRUint32 id;
  PRInt32 count = PR_sscanf(key.get(), ACCOUNT_KEY "%u", &id);
  NS_ENSURE_TRUE(count == 1, NS_ERROR_FAILURE);

  rv = ensureStored(id, name, prpl);
  NS_ENSURE_SUCCESS(rv, rv);

  return proto->GetAccount(key, name, aResult);
}

/* purpleIAccount createAccount(in AUTF8String aName, in AUTF8String aPrpl); */
NS_IMETHODIMP purpleAccountsService::CreateAccount(const nsACString &aName,
                                                   const nsACString &aPrpl,
                                                   purpleIAccount **aResult)
{
  /* Get the list of accounts */
  nsCString accountList;
  nsresult rv = mPrefBranch->GetCharPref(PREF_MESSENGER_ACCOUNTS,
                                         getter_Copies(accountList));
  NS_ENSURE_SUCCESS(rv, rv);

  /* Prepare the statement that will be used to check that the key
     isn't already used in the database */
  purpleStorage *storageInstance = purpleStorage::GetInstance();
  NS_ENSURE_TRUE(storageInstance, NS_ERROR_OUT_OF_MEMORY);

  mozIStorageConnection *DBConn = storageInstance->GetConnection();
  nsCOMPtr<mozIStorageStatement> statement;
  rv = DBConn->CreateStatement(NS_LITERAL_CSTRING(
    "SELECT id FROM accounts WHERE id = ?1"),
    getter_AddRefs(statement));
  NS_ENSURE_SUCCESS(rv, rv);

  /* First get a new unique account key */
  PRUint32 i;
  PRBool found = PR_TRUE;
  for (i = 1; found; ++i) {
    found = PR_FALSE;
    for (PRInt32 j = mAccounts.Count() - 1; j >= 0; --j) {
      PRUint32 id;
      if ((found = (NS_SUCCEEDED(mAccounts[j]->GetNumericId(&id)) && id == i)))
        break;
    }
    if (!found) {
      /* We have found a key that isn't used in the pref, double check
         it isn't already saved in the sqlite database. This should
         never happen. */
      LOG(("Checking in mozStorage if account id %i is free", i));
      statement->BindInt32Parameter(0, i);
      rv = statement->ExecuteStep(&found);
      NS_ENSURE_SUCCESS(rv, rv);
      rv = statement->Reset();
      NS_ENSURE_SUCCESS(rv, rv);
      if (found) {
        /* This should never happen so log a string to the error console */
        nsCOMPtr<nsIConsoleService> consoleService =
          do_GetService( "@mozilla.org/consoleservice;1" );

        nsString msg(NS_LITERAL_STRING("No account "));
        msg.AppendInt(i);
        msg.Append(NS_LITERAL_STRING(" but there is some data in the buddy list for an account with this number. Your profile may be corrupted."));
        consoleService->LogStringMessage(msg.get());
      }
    }
  }

  nsCString key(ACCOUNT_KEY);
  key.AppendInt(i);

  /* Get the prpl */
  nsCOMPtr<purpleICoreService> pcs =
    do_GetService(PURPLE_CORE_SERVICE_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<purpleIProtocol> proto;
  rv = pcs->GetProtocolById(aPrpl, getter_AddRefs(proto));
  NS_ENSURE_SUCCESS(rv, rv);
  NS_ENSURE_TRUE(proto, NS_ERROR_FAILURE);

  /* Avoid creating a duplicate account. */
  PRBool alreadyExists = PR_FALSE;
  rv = proto->AccountExists(aName, &alreadyExists);
  NS_ENSURE_SUCCESS(rv, rv);

  if (alreadyExists) {
    // adding a duplicate account would confuse libpurple
    NS_WARNING("Attempted to create a duplicate account!");
    return NS_ERROR_ALREADY_INITIALIZED;
  }

  /* Actually create the new account */
  nsCOMPtr<purpleIAccount> account;
  rv = proto->GetAccount(key, aName, getter_AddRefs(account));
  NS_ENSURE_SUCCESS(rv, rv);

  /* get the branch */
  nsCOMPtr<nsIPrefBranch> prefBranch;
  nsCString branchPath(PREF_ACCOUNT_PREFIX);
  branchPath.Append(key);
  branchPath.Append('.');
  rv = mPrefService->GetBranch(branchPath.get(), getter_AddRefs(prefBranch));
  NS_ENSURE_SUCCESS(rv, rv);

  PromiseFlatCString flatName(aName);
  prefBranch->SetCharPref(PREF_NAME, flatName.get());
  PromiseFlatCString flatPrpl(aPrpl);
  prefBranch->SetCharPref(PREF_PRPL, flatPrpl.get());

  rv = storeAccount(i, flatName, flatPrpl);
  if (NS_FAILED(rv)) {
    prefBranch->DeleteBranch("");
    return rv;
  }

  rv = account->SetFirstConnectionState(purpleIAccount::FIRST_CONNECTION_SET);
  NS_ENSURE_SUCCESS(rv, rv);

  /* Save the account list pref */
  if (accountList.IsEmpty())
    accountList = key;
  else {
    accountList.Append(',');
    accountList.Append(key);
  }
  mPrefBranch->SetCharPref(PREF_MESSENGER_ACCOUNTS, accountList.get());

  /* Keep it in the local account list */
  mAccounts.AppendObject(account);

  /* Notify observers */
  NotifyObservers(account, "account-added");

  /* And return the new account as result */
  NS_ADDREF(*aResult = account);
  return NS_OK;
}

/* void deleteAccount (in AUTF8String accountId); */
NS_IMETHODIMP purpleAccountsService::DeleteAccount(const nsACString& aAccountId)
{
  LOG(("Attempting to delete %s", PromiseFlatCString(aAccountId).get()));

  /* delete the account */
  PRInt32 i;
  for (i = mAccounts.Count() - 1; i >= 0; --i) {
    nsCString id;
    if (NS_SUCCEEDED(mAccounts[i]->GetId(id)) && id.Equals(aAccountId)) {
      /* Notify observers */
      NotifyObservers(mAccounts[i], "account-removed");
      /* Remove the pref of the account, and remove it from libpurple */
      mAccounts[i]->Remove();
      /* Remove it from our local array */
      mAccounts.RemoveObjectAt(i);
      break;
    }
  }

  NS_ENSURE_TRUE(i >= 0, NS_ERROR_FAILURE);

  /*Update the list of accounts in the prefs */

  /* Get the list of accounts */
  nsCString accountList;
  nsresult rv = mPrefBranch->GetCharPref(PREF_MESSENGER_ACCOUNTS,
                                         getter_Copies(accountList));
  NS_ENSURE_SUCCESS(rv, rv);

  // reconstruct the new account list, re-adding all accounts except
  // the one with 'key'
  nsCString newAccountList;
  char *newStr = accountList.BeginWriting();

  for (char *token = NS_strtok(",", &newStr);
       token;
       token = NS_strtok(",", &newStr)) {
    nsCString testKey(token);
    testKey.StripWhitespace();

    // re-add the candidate key only if it's not the key we're looking for
    if (!testKey.IsEmpty() && !testKey.Equals(aAccountId)) {
      if (!newAccountList.IsEmpty())
        newAccountList.Append(',');
      newAccountList += testKey;
    }
  }

  // now write the new account list back to the prefs
  rv = mPrefBranch->SetCharPref(PREF_MESSENGER_ACCOUNTS,
                                newAccountList.get());
  NS_ENSURE_SUCCESS(rv, rv);

  // ensure the change is written to the disk now
  return mPrefService->SavePrefFile(nsnull);
}

nsresult purpleAccountsService::NotifyObservers(nsISupports *aObj,
                                                const char *aEvent,
                                                const PRUnichar *aData)
{
  nsresult rv;
  nsCOMPtr<nsIObserverService> os =
    do_GetService(NS_OBSERVERSERVICE_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  LOG(("Notifying observers of %s", aEvent));
  return os->NotifyObservers(aObj, aEvent, aData);
}
