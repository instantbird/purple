/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "purpleTooltipInfo.h"
#include <nsIClassInfoImpl.h>
#include <nsIProgrammingLanguage.h>
#include <nsMemory.h>

// 630ABD2A-D50B-4F32-81E7-E7D3981D9296
#define PURPLE_TOOLTIP_INFO_CID                        \
  { 0x630ABD2A, 0xD50B, 0x4F32,                        \
    { 0x81, 0xE7, 0xE7, 0xD3, 0x98, 0x1D, 0x92, 0x96 } \
  }

NS_IMPL_CLASSINFO(purpleTooltipInfo, NULL, 0, PURPLE_TOOLTIP_INFO_CID)
NS_IMPL_THREADSAFE_CI(purpleTooltipInfo)
NS_IMPL_ISUPPORTS1_CI(purpleTooltipInfo, prplITooltipInfo)

purpleTooltipInfo::purpleTooltipInfo()
{
  /* member initializers and constructor code */
  mType = -1;
}

purpleTooltipInfo::~purpleTooltipInfo()
{
  /* destructor code */
}

void purpleTooltipInfo::Init(PurpleNotifyUserInfoEntry *aEntry)
{
  mType = purple_notify_user_info_entry_get_type(aEntry);
  mLabel = purple_notify_user_info_entry_get_label(aEntry);
  char *value =
    purple_unescape_html(purple_notify_user_info_entry_get_value(aEntry));
  mValue = value;
  g_free(value);
}

/* readonly attribute short type; */
NS_IMETHODIMP purpleTooltipInfo::GetType(PRInt16 *aType)
{
  NS_ENSURE_TRUE(mType != -1, NS_ERROR_NOT_INITIALIZED);

  *aType = mType;
  return NS_OK;
}

/* readonly attribute AUTF8String label; */
NS_IMETHODIMP purpleTooltipInfo::GetLabel(nsACString& aLabel)
{
  NS_ENSURE_TRUE(mType != -1, NS_ERROR_NOT_INITIALIZED);

  aLabel = mLabel;
  return NS_OK;
}

/* readonly attribute AUTF8String value; */
NS_IMETHODIMP purpleTooltipInfo::GetValue(nsACString& aValue)
{
  NS_ENSURE_TRUE(mType != -1, NS_ERROR_NOT_INITIALIZED);

  aValue = mValue;
  return NS_OK;
}
