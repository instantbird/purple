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

#include "purpleTagsService.h"
#include "purpleStorage.h"
#include "purpleTag.h"
#include <nsIClassInfoImpl.h>
#include <nsMemory.h>

#ifdef PR_LOGGING
#include <prprf.h>
//
// NSPR_LOG_MODULES=purpleTagsService:5
//
static PRLogModuleInfo *gPurpleTagsServiceLog = nsnull;
#endif
#define LOG(args) PR_LOG(gPurpleTagsServiceLog, PR_LOG_DEBUG, args)

NS_IMPL_ISUPPORTS1_CI(purpleTagsService, purpleITagsService)

purpleTagsService::purpleTagsService()
  :mInitialized(PR_FALSE)
{
#ifdef PR_LOGGING
  if (!gPurpleTagsServiceLog)
    gPurpleTagsServiceLog = PR_NewLogModule("purpleTagsService");
#endif
  mTagById.Init();
}

purpleTagsService::~purpleTagsService()
{
  if (mInitialized)
    UnInitTags();
}

NS_IMETHODIMP purpleTagsService::InitTags()
{
  /* We shouldn't load the list twice. */
  NS_ENSURE_TRUE(!mInitialized, NS_ERROR_ALREADY_INITIALIZED);

  mInitialized = PR_TRUE;
  purpleStorage *storageInstance = purpleStorage::GetInstance();
  NS_ENSURE_TRUE(storageInstance, NS_ERROR_OUT_OF_MEMORY);

  mozIStorageConnection *DBConn = storageInstance->GetConnection();
  nsCOMPtr<mozIStorageStatement> statement;
  nsresult rv = DBConn->CreateStatement(NS_LITERAL_CSTRING(
    "SELECT id, name FROM tags"),
    getter_AddRefs(statement));
  NS_ENSURE_SUCCESS(rv, rv);

  PRBool hasMoreData;
  while (NS_SUCCEEDED(statement->ExecuteStep(&hasMoreData)) && hasMoreData) {
    PRInt32 id = statement->AsInt32(0);
    nsCString name;
    rv = statement->GetUTF8String(1, name);
    NS_ENSURE_SUCCESS(rv, rv);

    LOG(("pts::InitTags: reading tag %s (id %i)",
         name.get(), id));
    PurpleGroup *group = purple_group_new(name.get());
    purpleTag *tag = new purpleTag();
    tag->Init(id, group);
    mTagById.Put(id, tag);
    purple_blist_add_group(group, NULL);
  }

  LOG(("Tags service initialized"));
  return NS_OK;
}

NS_IMETHODIMP purpleTagsService::UnInitTags()
{
  NS_ENSURE_TRUE(mInitialized, NS_ERROR_NOT_INITIALIZED);

  mTagById.Clear();
  mInitialized = PR_FALSE;

  return NS_OK;
}

/* purpleITag createTag (in AUTF8String aName); */
NS_IMETHODIMP purpleTagsService::CreateTag(const nsACString& aName,
                                           purpleITag **aResult)
{
  NS_ENSURE_ARG(!aName.IsEmpty());

  // if the tag already exists, we don't want to create a duplicate
  nsresult rv = GetTagByName(aName, aResult);
  if (NS_SUCCEEDED(rv) && *aResult)
    return rv;

  purpleStorage *storageInstance = purpleStorage::GetInstance();
  mozIStorageStatement *statement = storageInstance->mInsertGroup;
  mozStorageStatementScoper scoper(statement);
  rv = statement->BindUTF8StringParameter(0, aName);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = statement->BindInt32Parameter(1, 0); // FIXME (0 = not visible);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = statement->Execute();
  NS_ENSURE_SUCCESS(rv, rv);
  LOG(("pts::createTag: stored group %s", PromiseFlatCString(aName).get()));
  PRInt32 id;
  rv = GetTagIdFromName(aName, &id);
  NS_ENSURE_SUCCESS(rv, rv);

  // creating an already existing group will return the existing group
  PurpleGroup *group = purple_group_new(PromiseFlatCString(aName).get());
  PRBool isNew = !GET_NODE_UI_DATA(group);
  purpleTag *tag = new purpleTag();
  tag->Init(id, group);
  mTagById.Put(id, tag);
  if (isNew)
    purple_blist_add_group(group, NULL);

  NS_ADDREF(*aResult = tag);
  return NS_OK;
}

/* purpleITag getTagById (in long aId); */
NS_IMETHODIMP purpleTagsService::GetTagById(PRInt32 aId, purpleITag **aResult)
{
  purpleITag *tag;
  PRBool found = mTagById.Get(aId, &tag);
  NS_ENSURE_TRUE(found, NS_ERROR_FAILURE);

  *aResult = tag;
  return NS_OK;
}

nsresult purpleTagsService::GetTagIdFromName(const nsACString& aName,
                                             PRInt32 *aResult)
{
  //FIXME The result should be cached.
  purpleStorage *storageInstance = purpleStorage::GetInstance();
  NS_ENSURE_TRUE(storageInstance, NS_ERROR_OUT_OF_MEMORY);

  mozIStorageStatement *statement = storageInstance->mGetGroupIdFromName;
  mozStorageStatementScoper scoper(statement);
  nsresult rv = statement->BindUTF8StringParameter(0, aName);
  NS_ENSURE_SUCCESS(rv, rv);

  PRBool hasMoreData;
  rv = statement->ExecuteStep(&hasMoreData);
  NS_ENSURE_SUCCESS(rv, rv);

  if (!hasMoreData) {
    /* The query succeeded but there is no data:
       this means the tag doesn't exist yet in mozStorage,
       it's not really an error so we don't use NS_ENSURE_TRUE here */
    return NS_ERROR_FAILURE;
  }

  *aResult = statement->AsInt32(0);
  LOG(("pcs::GetTagIdFromName(name = %s), found at id %i",
       PromiseFlatCString(aName).get(), *aResult));
  return NS_OK;
}

/* purpleITag getTagByName (in AUTF8String aName); */
NS_IMETHODIMP purpleTagsService::GetTagByName(const nsACString & aName,
                                              purpleITag **aResult)
{
  /* First, get the id from mozStorage */
  PRInt32 id;
  nsresult rv = GetTagIdFromName(aName, &id);
  if (NS_SUCCEEDED(rv)) {
    /* We found the id, just get the tag from the hashtable */
    if (mTagById.Get(id, aResult))
      return NS_OK;

    /* if the tag isn't in the hashtable, this means we are still
       loading the buddy list from mozStorage, and this method
       shouldn't have been called */
    NS_NOTREACHED("tag in mozStorage but not in the hashtable");
  }

  /* This tag doesn't exist yet */
  *aResult = nsnull;
  return NS_OK;
}

/* void getTags ([optional] out unsigned long tagCount,
                 [array, retval, size_is (tagCount)] out purpleITag tags); */
NS_IMETHODIMP purpleTagsService::GetTags(PRUint32 *tagCount,
                                         purpleITag ***tags)
{
  purpleStorage *storageInstance = purpleStorage::GetInstance();
  NS_ENSURE_TRUE(storageInstance, NS_ERROR_OUT_OF_MEMORY);

  mozIStorageConnection *DBConn = storageInstance->GetConnection();
  nsCOMPtr<mozIStorageStatement> statement;
  nsresult rv = DBConn->CreateStatement(NS_LITERAL_CSTRING(
    "SELECT id FROM tags"),
    getter_AddRefs(statement));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMArray<purpleITag> array;
  PRBool hasMoreData;
  while (NS_SUCCEEDED(statement->ExecuteStep(&hasMoreData)) && hasMoreData) {
    purpleITag *tag;
    rv = GetTagById(statement->AsInt32(0), &tag);
    if (NS_SUCCEEDED(rv))
      array.AppendObject(tag);
  }

  *tags = (purpleITag **)nsMemory::Alloc(array.Count() * sizeof(purpleITag *));
  PRInt32 i;
  for (i = 0; i < array.Count(); ++i)
    (*tags)[i] = array[i];
  *tagCount = i;
  return NS_OK;
}
