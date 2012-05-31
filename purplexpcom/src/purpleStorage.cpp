/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "purpleStorage.h"
#include "nsDirectoryServiceUtils.h"
#include "nsAppDirectoryServiceDefs.h"
#include<nsStringAPI.h>

#define MOZ_STORAGE_SERVICE_CONTRACTID "@mozilla.org/storage/service;1"

purpleStorage *purpleStorage::mInstance = nsnull;

purpleStorage *purpleStorage::GetInstance()
{
  if (!mInstance) {
    mInstance = new purpleStorage();
    if (NS_FAILED(mInstance->Init())) {
      delete mInstance;
      mInstance = nsnull;
    }
  }

  return mInstance;
}

#define DO_SQL(aRequest)                                        \
  rv = mDBConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(aRequest)); \
  NS_ENSURE_SUCCESS(rv, rv)

#define CREATE_STATEMENT(aName, aStatement)                     \
  rv = mDBConn->CreateStatement(NS_LITERAL_CSTRING(aStatement), \
                                getter_AddRefs(aName));         \
  NS_ENSURE_SUCCESS(rv, rv)

nsresult purpleStorage::Init()
{
  nsCOMPtr<nsIFile> dbFile;
  nsresult rv = NS_GetSpecialDirectory(NS_APP_USER_PROFILE_50_DIR,
                                       getter_AddRefs(dbFile));
  NS_ENSURE_SUCCESS(rv, rv);
  rv = dbFile->Append(NS_LITERAL_STRING("blist.sqlite"));
  NS_ENSURE_SUCCESS(rv, rv);

  mDBService = do_GetService(MOZ_STORAGE_SERVICE_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = mDBService->OpenDatabase(dbFile, getter_AddRefs(mDBConn));
  NS_ENSURE_SUCCESS(rv, rv);
  bool ready;
  mDBConn->GetConnectionReady(&ready);
  NS_ENSURE_TRUE(ready, NS_ERROR_UNEXPECTED);

  // Grow blist db in 512KB increments.
  mDBConn->SetGrowthIncrement(512 * 1024, EmptyCString());

  DO_SQL("CREATE TABLE IF NOT EXISTS accounts ("
         "id INTEGER PRIMARY KEY, "
         "name VARCHAR, "
         "prpl VARCHAR)");

  DO_SQL("CREATE TABLE IF NOT EXISTS contacts ("
         "id INTEGER PRIMARY KEY, "
         "firstname VARCHAR, "
         "lastname VARCHAR, "
         "alias VARCHAR)");

  DO_SQL("CREATE TABLE IF NOT EXISTS buddies ("
         "id INTEGER PRIMARY KEY, "
         "key VARCHAR NOT NULL, "
         "name VARCHAR NOT NULL, "
         "srv_alias VARCHAR, "
         "position INTEGER, "
         "icon BLOB, "
         "contact_id INTEGER)");
  DO_SQL("CREATE INDEX IF NOT EXISTS buddies_contactindex "
         "ON buddies (contact_id)");

  DO_SQL("CREATE TABLE IF NOT EXISTS tags ("
         "id INTEGER PRIMARY KEY, "
         "name VARCHAR UNIQUE NOT NULL, "
         "position INTEGER)");

  DO_SQL("CREATE TABLE IF NOT EXISTS contact_tag ("
         "contact_id INTEGER NOT NULL, "
         "tag_id INTEGER NOT NULL)");
  DO_SQL("CREATE INDEX IF NOT EXISTS contact_tag_contactindex "
         "ON contact_tag (contact_id)");
  DO_SQL("CREATE INDEX IF NOT EXISTS contact_tag_tagindex "
         "ON contact_tag (tag_id)");

  DO_SQL("CREATE TABLE IF NOT EXISTS account_buddy ("
         "account_id INTEGER NOT NULL, "
         "buddy_id INTEGER NOT NULL, "
         "status VARCHAR, "
         "tag_id INTEGER)");
  DO_SQL("CREATE INDEX IF NOT EXISTS account_buddy_accountindex "
         "ON account_buddy (account_id)");
  DO_SQL("CREATE INDEX IF NOT EXISTS account_buddy_buddyindex "
         "ON account_buddy (buddy_id)");

  CREATE_STATEMENT(mEnsureAccountStoredStatement,
                   "SELECT name, prpl FROM accounts WHERE id = ?1");
  return NS_OK;
}
