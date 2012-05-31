/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <nsCOMPtr.h>
#include <mozIStorageService.h>
#include <mozIStorageConnection.h>
#include <mozIStorageStatement.h>
#include <mozStorageHelper.h>

class purpleStorage
{
 public:
  static purpleStorage *GetInstance();
  static void unInit() { delete mInstance; }
  mozIStorageConnection *GetConnection() {
    return mDBConn;
  }
  nsCOMPtr<mozIStorageStatement> mEnsureAccountStoredStatement;

 private:
  static purpleStorage *mInstance;
  ~purpleStorage() { if (mInstance == this) mInstance = nsnull; }
  nsresult Init();
  nsCOMPtr<mozIStorageService> mDBService;
  nsCOMPtr<mozIStorageConnection> mDBConn;
};
