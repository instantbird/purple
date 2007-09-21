#include "purpleTag.h"
#include "purpleUtils.h"

NS_IMPL_ISUPPORTS1(purpleTag, purpleITag)

purpleTag::purpleTag()
: mId(0),
  mGroup(NULL)
{
  /* member initializers and constructor code */
}

purpleTag::~purpleTag()
{
  /* destructor code */
}

void purpleTag::Init(PRUint32 aId, PurpleGroup *aGroup)
{
  mId = aId;
  mGroup = aGroup;
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
  if (!mId) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  *aId = mId;
  return NS_OK;
}

/* readonly attribute string name; */
NS_IMETHODIMP purpleTag::GetName(char * *aName)
{
  if (!mGroup) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  return returnString(aName, purple_group_get_name(mGroup));
}

/* readonly attribute boolean visible; */
NS_IMETHODIMP purpleTag::GetVisible(PRBool *aVisible)
{
  *aVisible = mGroup != NULL;

  return NS_OK;
}
