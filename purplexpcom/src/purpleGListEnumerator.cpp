/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "purpleGListEnumerator.h"

NS_IMPL_ISUPPORTS1(purpleGListEnumerator, nsISimpleEnumerator)

purpleGListEnumerator::purpleGListEnumerator()
{
  mGList = NULL;
  mDataToSupports = NULL;
  mCleanupCallback = NULL;
  mCleanupData = NULL;
}

purpleGListEnumerator::~purpleGListEnumerator()
{
  Cleanup();
}

void purpleGListEnumerator::Cleanup()
{
  if (!mCleanupCallback)
    return;

  mCleanupCallback(mCleanupData);
  mCleanupCallback = NULL;
}

nsresult purpleGListEnumerator::Init(GList *aGList,
                                     converterCallback aDataToSupports,
                                     cleanupCallback aCleanupCallback,
                                     void *aCleanupData)
{
  mGList = aGList;
  mDataToSupports  = aDataToSupports;
  mCleanupCallback = aCleanupCallback;
  mCleanupData     = aCleanupData;

  return NS_OK;
}

NS_IMETHODIMP purpleGListEnumerator::GetNext(nsISupports **aResult)
{
  NS_ENSURE_TRUE(mGList, NS_ERROR_FAILURE);
  *aResult = mDataToSupports(mGList->data);
  mGList = g_list_next(mGList);
  return NS_OK;
}

NS_IMETHODIMP purpleGListEnumerator::HasMoreElements(bool *aResult)
{
  if (!mGList) {
    Cleanup();
    *aResult = PR_FALSE;
  }
  else
    *aResult = PR_TRUE;

  return NS_OK;
}
