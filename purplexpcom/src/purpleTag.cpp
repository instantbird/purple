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

#include "purpleTag.h"
#include "purpleBuddy.h"
#include "nsStringAPI.h"
#include <nsIClassInfoImpl.h>
#include <nsMemory.h>

#ifdef PR_LOGGING
//
// NSPR_LOG_MODULES=purpleTag:5
//
PRLogModuleInfo *gPurpleTagLog = nsnull;
#endif
#define LOG(args) PR_LOG(gPurpleTagLog, PR_LOG_DEBUG, args)

NS_IMPL_CLASSINFO(purpleTag, NULL, 0, PURPLE_TAG_CID)
NS_IMPL_ISUPPORTS1_CI(purpleTag, purpleITag)

purpleTag::purpleTag()
: mId(0),
  mGroup(NULL)
{
  /* member initializers and constructor code */
#ifdef PR_LOGGING
  if (!gPurpleTagLog)
    gPurpleTagLog = PR_NewLogModule("purpleTag");
#endif
}

purpleTag::~purpleTag()
{
  /* destructor code */
}

void purpleTag::Init(PRUint32 aId, PurpleGroup *aGroup)
{
  mId = aId;
  mGroup = aGroup;
  purple_blist_node_set_ui_data(PURPLE_BLIST_NODE(aGroup), this);
}

void purpleTag::addBuddy(PurpleBuddy *aBuddy)
{
  if (!mGroup)
    return;

  purple_blist_add_buddy(aBuddy, NULL, mGroup, NULL);
}

/* readonly attribute long id; */
NS_IMETHODIMP purpleTag::GetId(PRInt32 *aId)
{
  NS_ENSURE_TRUE(mId, NS_ERROR_NOT_INITIALIZED);

  *aId = mId;
  return NS_OK;
}

/* readonly attribute AUTF8String name; */
NS_IMETHODIMP purpleTag::GetName(nsACString &aName)
{
  NS_ENSURE_TRUE(mGroup, NS_ERROR_NOT_INITIALIZED);

  aName = purple_group_get_name(mGroup);
  return NS_OK;
}

/* readonly attribute boolean visible; */
NS_IMETHODIMP purpleTag::GetVisible(PRBool *aVisible)
{
  *aVisible = mGroup != NULL;

  return NS_OK;
}

/* void getBuddies ([optional] out unsigned long buddyCount,
                    [array, retval, size_is (buddyCount)] out purpleIBuddy buddies); */
NS_IMETHODIMP purpleTag::GetBuddies(PRUint32 *buddyCount,
				    purpleIBuddy ***buddies)
{
  NS_ENSURE_TRUE(mGroup, NS_ERROR_NOT_INITIALIZED);

  PRUint32 count = 0;
  PurpleBlistNode *cnode;
  for (cnode = PURPLE_BLIST_NODE(mGroup)->child; cnode; cnode = cnode->next)
    if (PURPLE_BLIST_NODE_IS_CONTACT(cnode) && PURPLE_BLIST_NODE_IS_BUDDY(cnode->child))
      ++count;
  *buddies = (purpleIBuddy **)nsMemory::Alloc(count * sizeof(purpleIBuddy *));

  count = 0;
  for (cnode = PURPLE_BLIST_NODE(mGroup)->child; cnode; cnode = cnode->next)
    if (PURPLE_BLIST_NODE_IS_CONTACT(cnode) && PURPLE_BLIST_NODE_IS_BUDDY(cnode->child)) {
      purpleBuddy *buddy = purpleBuddy::fromPurpleContact(PURPLE_CONTACT(cnode));
      for (PRUint32 j = 0; j < count; ++j)
	if ((*buddies)[j] == buddy) {
	  buddy = nsnull;
	  break;
	}
      if (!buddy)
	continue;

      NS_ADDREF(buddy);
      (*buddies)[count++] = buddy;
    }

  *buddyCount = count;
  return NS_OK;
}

/* void addObserver (in nsIObserver aObserver); */
NS_IMETHODIMP purpleTag::AddObserver(nsIObserver *aObserver)
{
  NS_ENSURE_ARG_POINTER(aObserver);
  NS_ENSURE_TRUE(mObservers.IndexOfObject(aObserver) == -1,
                 NS_ERROR_ALREADY_INITIALIZED);
  LOG(("purpleTag::AddObserver (mId = %i, observer = @%x)",
      mId, aObserver));

  mObservers.AppendObject(aObserver);
  return NS_OK;
}

/* void removeObserver (in nsIObserver aObserver); */
NS_IMETHODIMP purpleTag::RemoveObserver(nsIObserver *aObserver)
{
  NS_ENSURE_ARG_POINTER(aObserver);

  LOG(("purpleTag::RemoveObserver (mId = %i, observer = @%x)",
       mId, aObserver));
  NS_ENSURE_TRUE(mObservers.RemoveObject(aObserver), NS_ERROR_FAILURE);
  return NS_OK;
}

/* void notifyObservers (in nsISupports aObj, in string aEvent, in wstring aData); */
NS_IMETHODIMP purpleTag::NotifyObservers(nsISupports *aSubject,
                                         const char *aTopic,
                                         const PRUnichar *aData)
{
  LOG(("purpleTag::NotifyObservers (mId = %i, topic = %s)",
       mId, aTopic));
  for (PRInt32 i = 0; i < mObservers.Count(); ++i)
    mObservers[i]->Observe(aSubject, aTopic, aData);

  return NS_OK;
}
