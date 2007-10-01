#include <nsCOMPtr.h>
#include <storage/mozIStorageService.h>
#include <storage/mozIStorageConnection.h>
#include <storage/mozIStorageStatement.h>
#include <storage/mozStorageHelper.h>

class purpleStorage
{
 public:
  static purpleStorage *GetInstance();
  static void unInit() { delete mInstance; }
  mozIStorageConnection *GetConnection() {
    return mDBConn;
  }
  nsCOMPtr<mozIStorageStatement> mEnsureAccountStoredStatement;
  nsCOMPtr<mozIStorageStatement> mGetGroupIdFromName;
  nsCOMPtr<mozIStorageStatement> mGetBuddyIdFromNameAndPrpl;
  nsCOMPtr<mozIStorageStatement> mInsertGroup;
  nsCOMPtr<mozIStorageStatement> mInsertBuddy;
  nsCOMPtr<mozIStorageStatement> mInsertAccountBuddy;
  nsCOMPtr<mozIStorageStatement> mLastId;

 private:
  static purpleStorage *mInstance;
  nsresult Init();
  nsCOMPtr<mozIStorageService> mDBService;
  nsCOMPtr<mozIStorageConnection> mDBConn;
};
