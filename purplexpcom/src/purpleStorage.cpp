#include "purpleStorage.h"
#include "nsDirectoryServiceUtils.h"
#include "nsAppDirectoryServiceDefs.h"

// very dirty work around for the error:
// nsStringAPI.h:953: error: size of arg" is negative array
#undef HAVE_CPP_2BYTE_WCHAR_T
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

  CREATE_STATEMENT(mGetGroupIdFromName,
                   "SELECT id FROM tags WHERE name = ?1");

  CREATE_STATEMENT(mGetBuddyIdFromNameAndPrpl,
                   "SELECT b.id FROM buddies b "
                               "JOIN account_buddy ab ON buddy_id = b.id "
                               "JOIN accounts a ON account_id = a.id "
                               "WHERE b.key = ?1 and a.prpl = ?2");

  CREATE_STATEMENT(mInsertGroup,
                   "INSERT INTO tags (name, position) VALUES(?1, ?2)");

  CREATE_STATEMENT(mInsertBuddy,
                   "INSERT INTO buddies (key, name, srv_alias, position) VALUES(?1, ?2, ?3, ?4)");

  CREATE_STATEMENT(mInsertAccountBuddy,
                   "INSERT INTO account_buddy (account_id, buddy_id, tag_id) VALUES(?1, ?2, ?3)");

  CREATE_STATEMENT(mLastId, "SELECT last_insert_rowid()");

  return NS_OK;
}
