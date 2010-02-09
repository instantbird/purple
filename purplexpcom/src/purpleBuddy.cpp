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

#include "purpleBuddy.h"
#include "purpleCoreService.h"
#include "purpleStorage.h"
#include <nsServiceManagerUtils.h>
#include <nsComponentManagerUtils.h>
#include <nsIClassInfoImpl.h>
#include <nsMemory.h>
#include <mozIStoragePendingStatement.h>

NS_IMPL_ISUPPORTS1_CI(purpleBuddy, purpleIBuddy)

#ifdef PR_LOGGING
//
// NSPR_LOG_MODULES=purpleBuddy:5
//
static PRLogModuleInfo *gPurpleBuddyLog = nsnull;
#endif
#define LOG(args) PR_LOG(gPurpleBuddyLog, PR_LOG_DEBUG, args)

purpleBuddy::purpleBuddy()
{
  /* member initializers and constructor code */
  mId = 0;
#ifdef PR_LOGGING
  if (!gPurpleBuddyLog)
    gPurpleBuddyLog = PR_NewLogModule("purpleBuddy");
#endif
  LOG(("Creating purpleBuddy @%x", this));
}

purpleBuddy::~purpleBuddy()
{
  /* destructor code */
  LOG(("Destructing purpleBuddy @%x", this));
}

nsresult purpleBuddy::load(PurpleBuddy *aBuddy, purpleIAccount *aAccount,
                           PRUint32 aId, purpleTag *aTag)
{
  mId = aId;
  AddAccount(aAccount, aBuddy, aTag);
  return NS_OK;
}

nsresult purpleBuddy::Init(PurpleBuddy *aBuddy)
{
#ifdef PR_LOGGING
  PurpleBlistNode *node = aBuddy->node.parent;
  PurpleGroup *group = (PurpleGroup *) node->parent;
  LOG(("purpleBuddy::Init Trying to add %s in group: %s for account %s (%s)",
       purple_normalize(aBuddy->account, aBuddy->name), group->name,
       aBuddy->account->username, aBuddy->account->protocol_id));
#endif

  purpleStorage *storageInstance = purpleStorage::GetInstance();
  {
    mozIStorageStatement *statement = storageInstance->mInsertBuddy;
    mozStorageStatementScoper scoper(statement);
    nsCString name(purple_normalize(aBuddy->account,
                                    purple_buddy_get_name(aBuddy)));
    nsresult rv = statement->BindUTF8StringParameter(0, name);
    NS_ENSURE_SUCCESS(rv, rv);
    nsCString prettyName(purple_buddy_get_name(aBuddy));
    rv = statement->BindUTF8StringParameter(1, prettyName);
    NS_ENSURE_SUCCESS(rv, rv);
    nsCString alias(purple_buddy_get_server_alias(aBuddy));
    rv = statement->BindUTF8StringParameter(2, alias);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = statement->BindInt32Parameter(3, 0); // FIXME (0 = not visible);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = statement->Execute();
    NS_ENSURE_SUCCESS(rv, rv);
  }

  mozIStorageStatement *statement = storageInstance->mLastId;
  mozStorageStatementScoper scoper(statement);
  PRBool ok;
  nsresult rv = statement->ExecuteStep(&ok);
  NS_ENSURE_SUCCESS(rv, rv);
  NS_ASSERTION(ok, "purpleBuddy::Init: couldn't get the id of the stored buddy");
  NS_ENSURE_TRUE(ok, NS_ERROR_FAILURE);

  mId = statement->AsInt32(0);
  LOG(("purpleBuddy::Init: stored buddy with id %i", mId));

  return AddAccount(aBuddy);
}

purpleAccountBuddy *purpleBuddy::AddAccount(purpleIAccount *aAccount,
                                            PurpleBuddy *aBuddy,
                                            purpleTag *aTag)
{
  nsCOMPtr<purpleAccountBuddy> pab = do_CreateInstance(PURPLE_ACCOUNTBUDDY_CONTRACTID);
  pab->Init(aAccount, aBuddy, aTag);
  mBuddies.AppendObject(pab);
  return pab;
}

nsresult purpleBuddy::AddAccount(PurpleBuddy *aBuddy)
{
  purpleAccount *account = (purpleAccount *)aBuddy->account->ui_data;
  NS_ENSURE_TRUE(account, NS_ERROR_UNEXPECTED);

  nsCOMPtr<purpleICoreService> pcs =
    do_GetService(PURPLE_CORE_SERVICE_CONTRACTID);
  nsCOMPtr<purpleTag> tag;
  nsresult rv = NATIVE_PCS(pcs)->GetTagFromPurpleBuddy(aBuddy,
                                                       getter_AddRefs(tag));
  NS_ENSURE_SUCCESS(rv, rv);

  purpleAccountBuddy *pab = AddAccount(account, aBuddy, tag);
  return pab->Store(mId);
}

nsresult purpleBuddy::RemoveAccount(PurpleBuddy *aBuddy)
{
  LOG(("purpleBuddy::RemoveAccount buddy = %s, account = %s",
       aBuddy->name, purple_account_get_username(aBuddy->account)));

  for (PRInt32 i = 0; i < mBuddies.Count(); ++i) {
    if (mBuddies[i]->GetBuddy() == aBuddy) {
      mBuddies[i]->Uninit();
      mBuddies.RemoveObjectAt(i);
      return NS_OK;
    }
  }

  return NS_ERROR_FAILURE;
}

nsresult purpleBuddy::UpdateServerAlias(const char *aAlias)
{
  PURPLE_ENSURE_INIT(mId);

  purpleStorage *storageInstance = purpleStorage::GetInstance();
  {
    mozIStorageStatement *statement = storageInstance->mUpdateBuddyServerAlias;
    mozStorageStatementScoper scoper(statement);
    nsresult rv;
    if (aAlias)
      rv = statement->BindUTF8StringParameter(0, nsDependentCString(aAlias));
    else
      rv = statement->BindNullParameter(0);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = statement->BindInt32Parameter(1, mId);
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<mozIStoragePendingStatement> pendingStatement;
    rv = statement->ExecuteAsync(nsnull, getter_AddRefs(pendingStatement));
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return NS_OK;
}

/* readonly attribute long id; */
NS_IMETHODIMP purpleBuddy::GetId(PRInt32 *aId)
{
  PURPLE_ENSURE_INIT(mId);

  *aId = mId;
  return NS_OK;
}

/* readonly attribute AUTF8String name; */
NS_IMETHODIMP purpleBuddy::GetName(nsACString& aName)
{
  PURPLE_ENSURE_INIT(mId);

  return mBuddies[0]->GetBuddyName(aName);
}

/* readonly attribute AUTF8String name; */
NS_IMETHODIMP purpleBuddy::GetLoggedIn(nsACString& aLoggedIn)
{
  PURPLE_ENSURE_INIT(mId);

  return mBuddies[0]->GetLoggedIn(aLoggedIn);
}

/* readonly attribute AUTF8String buddyIconFilename; */
NS_IMETHODIMP purpleBuddy::GetBuddyIconFilename(nsACString& aBuddyIconFilename)
{
  PURPLE_ENSURE_INIT(mId);

  return mBuddies[0]->GetBuddyIconFilename(aBuddyIconFilename);
}

/*  nsISimpleEnumerator getTooltipInfo(); */
NS_IMETHODIMP purpleBuddy::GetTooltipInfo(nsISimpleEnumerator** aTooltipInfo)
{
  PURPLE_ENSURE_INIT(mId);

  return mBuddies[0]->GetTooltipInfo(aTooltipInfo);
}

/* attribute AUTF8String alias; */
NS_IMETHODIMP purpleBuddy::GetAlias(nsACString& aAlias)
{
  PURPLE_ENSURE_INIT(mId);

  if (!mAlias.IsEmpty()) {
    aAlias = mAlias;
    return NS_OK;
  }

  return mBuddies[0]->GetBuddyAlias(aAlias);
}
NS_IMETHODIMP purpleBuddy::SetAlias(const nsACString& aAlias)
{
  PURPLE_ENSURE_INIT(mId);

  mAlias = aAlias;
  for (PRInt32 i = 0; i < mBuddies.Count(); ++i)
    mBuddies[i]->SetBuddyAlias(mAlias);

  /* Save the alias in blist.sqlite */
  purpleStorage *storageInstance = purpleStorage::GetInstance();
  NS_ENSURE_TRUE(storageInstance, NS_ERROR_OUT_OF_MEMORY);

  mozIStorageConnection *DBConn = storageInstance->GetConnection();
  NS_ENSURE_TRUE(DBConn, NS_ERROR_UNEXPECTED);

  /* Get the contact_id if one already exists for this buddy */
  nsCOMPtr<mozIStorageStatement> statement;
  nsresult rv = DBConn->CreateStatement(NS_LITERAL_CSTRING(
    "SELECT contact_id FROM buddies WHERE id = ?1"),
    getter_AddRefs(statement));
  NS_ENSURE_SUCCESS(rv, rv);
  rv = statement->BindInt32Parameter(0, mId);
  NS_ENSURE_SUCCESS(rv, rv);
  PRBool contactExists = PR_FALSE;
  rv = statement->ExecuteStep(&contactExists);
  NS_ENSURE_SUCCESS(rv, rv);
  PRInt64 contactId = 0;
  if (contactExists)
    contactId = statement->AsInt64(0);

  if (contactId) {
    /* The contact exists, update the alias */
    rv = DBConn->CreateStatement(NS_LITERAL_CSTRING(
      "UPDATE contacts SET alias = ?1 WHERE id = ?2"),
      getter_AddRefs(statement));
    NS_ENSURE_SUCCESS(rv, rv);
    rv = statement->BindUTF8StringParameter(0, mAlias);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = statement->BindInt64Parameter(1, contactId);
    NS_ENSURE_SUCCESS(rv, rv);
    return statement->Execute();
  }

  /* At this point, we know the contact doesn't exist yet, create it */

  /* This is in 2 steps, so use a transaction */
  mozStorageTransaction transaction(DBConn, PR_FALSE);

  rv = DBConn->CreateStatement(NS_LITERAL_CSTRING(
    "INSERT INTO contacts (alias) VALUES(?1)"),
    getter_AddRefs(statement));
  NS_ENSURE_SUCCESS(rv, rv);
  rv = statement->BindUTF8StringParameter(0, mAlias);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = statement->Execute();
  NS_ENSURE_SUCCESS(rv, rv);

  /* Get the id of the new contact */
  rv = DBConn->GetLastInsertRowID(&contactId);
  NS_ENSURE_SUCCESS(rv, rv);

  /* Store the contact_id in the buddies table */
  rv = DBConn->CreateStatement(NS_LITERAL_CSTRING(
    "UPDATE buddies SET contact_id = ?1 WHERE id = ?2"),
    getter_AddRefs(statement));
  NS_ENSURE_SUCCESS(rv, rv);
  rv = statement->BindInt64Parameter(0, contactId);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = statement->BindInt32Parameter(1, mId);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = statement->Execute();
  NS_ENSURE_SUCCESS(rv, rv);

  return transaction.Commit();
}

/* readonly attribute short nbOnline; */
NS_IMETHODIMP purpleBuddy::GetNbOnline(PRInt16 *aNbOnline)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* readonly attribute short accountCount; */
NS_IMETHODIMP purpleBuddy::GetAccountCount(PRInt16 *aAccountCount)
{
  PURPLE_ENSURE_INIT(mId);

  *aAccountCount = mBuddies.Count();
  return NS_OK;
}

/* purpleIAccount getAccount (in short aAccountIndex); */
NS_IMETHODIMP purpleBuddy::GetAccount(PRUint16 aAccountIndex,
                                      purpleIAccount **aResult)
{
  PURPLE_ENSURE_INIT(mId);
  NS_ENSURE_TRUE(aAccountIndex < mBuddies.Count(), NS_ERROR_FAILURE);

  return mBuddies[aAccountIndex]->GetAccount(aResult);
}

/* nsISimpleEnumerator getTags (); */
NS_IMETHODIMP purpleBuddy::GetTags(nsISimpleEnumerator **_retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* void getAccounts ([optional] out unsigned long accountCount,
                     [array, retval, size_is (accountCount)] out purpleIAccount accounts); */
NS_IMETHODIMP purpleBuddy::GetAccounts(PRUint32 *accountCount,
                                       purpleIAccount ***accounts)
{
  *accounts =
    (purpleIAccount **)nsMemory::Alloc(mBuddies.Count() * sizeof(purpleIAccount *));

  PRInt32 i;
  for (i = 0; i < mBuddies.Count(); ++i)
    mBuddies[i]->GetAccount(&(*accounts)[i]);
  *accountCount = i;

  return NS_OK;
}

/* void getAccountBuddies ([optional] out unsigned long accountBuddyCount,
                           [array, retval, size_is (accountBuddyCount)] out purpleIAccountBuddy accountBuddies); */
NS_IMETHODIMP purpleBuddy::GetAccountBuddies(PRUint32 *accountBuddyCount,
                                             purpleIAccountBuddy ***accountBuddies)
{
  *accountBuddies =
    (purpleIAccountBuddy **)nsMemory::Alloc(mBuddies.Count() * sizeof(purpleIAccountBuddy *));

  PRInt32 i;
  for (i = 0; i < mBuddies.Count(); ++i)
    NS_ADDREF((*accountBuddies)[i] = mBuddies[i]);
  *accountBuddyCount = i;

  return NS_OK;
}

/* purpleIConversation createConversation (in unsigned short aAccountIndex); */
NS_IMETHODIMP purpleBuddy::CreateConversation(PRUint16 aAccountIndex,
                                              purpleIConversation **aResult)
{
  PURPLE_ENSURE_INIT(mId);
  NS_ENSURE_TRUE(aAccountIndex < mBuddies.Count(), NS_ERROR_FAILURE);

  purpleAccountBuddy *pab = mBuddies[aAccountIndex];
  return pab->CreateConversation(aResult);
}

/* [noscript] purpleIAccountBuddy getAccountBuddyForPurpleBuddy (in PurpleNativeBuddy aBuddy); */
NS_IMETHODIMP purpleBuddy::GetAccountBuddyForPurpleBuddy(PurpleBuddy * aBuddy,
                                                         purpleIAccountBuddy **aResult)
{
  for (PRInt32 i = 0; i < mBuddies.Count(); ++i) {
    if (mBuddies[i]->GetBuddy() == aBuddy) {
      NS_ADDREF(*aResult = mBuddies[i]);
      return NS_OK;
    }
  }

  return NS_ERROR_FAILURE;
}
