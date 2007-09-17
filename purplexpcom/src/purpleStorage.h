#include <nsCOMPtr.h>
#include <storage/mozIStorageService.h>
#include <storage/mozIStorageConnection.h>
#include <storage/mozIStorageStatement.h>

class purpleStorage
{
 public:
  static purpleStorage *GetInstance();
  mozIStorageConnection *GetConnection() {
    return mDBConn;
  }
  nsCOMPtr<mozIStorageStatement> mEnsureAccountStoredStatement;

 private:
  static purpleStorage *mInstance;
  nsresult Init();
  nsCOMPtr<mozIStorageService> mDBService;
  nsCOMPtr<mozIStorageConnection> mDBConn;
};
