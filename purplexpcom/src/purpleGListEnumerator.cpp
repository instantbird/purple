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
 * 2008.
 *
 * The Initial Developer of the Original Code is
 * Alexandre Abraham <goulagman@instantbird.org>.
 * Portions created by the Initial Developer are Copyright (C) 2008
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

NS_IMETHODIMP purpleGListEnumerator::Init(GList *aGList,
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

NS_IMETHODIMP purpleGListEnumerator::HasMoreElements(PRBool *aResult)
{
  if (!mGList) {
    Cleanup();
    *aResult = PR_FALSE;
  }
  else
    *aResult = PR_TRUE;

  return NS_OK;
}
