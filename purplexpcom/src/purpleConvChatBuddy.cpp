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
 * Florian QUEZE <florian@instantbird.org>.
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

#include "purpleConvChatBuddy.h"

NS_IMPL_ISUPPORTS1(purpleConvChatBuddy, purpleIConvChatBuddy)

purpleConvChatBuddy::purpleConvChatBuddy()
  : mFlags(PURPLE_CBFLAGS_NONE)
{
  /* member initializers and constructor code */
}

purpleConvChatBuddy::~purpleConvChatBuddy()
{
  /* destructor code */
}

void purpleConvChatBuddy::Init(PurpleConvChatBuddy *aBuddy)
{
  mName = aBuddy->name;
  mAlias = aBuddy->alias;
  mBuddy = aBuddy->buddy;
  mFlags = aBuddy->flags;
}

/* readonly attribute AUTF8String name; */
NS_IMETHODIMP purpleConvChatBuddy::GetName(nsACString & aName)
{
  aName = mName;
  return NS_OK;
}

/* readonly attribute AUTF8String alias; */
NS_IMETHODIMP purpleConvChatBuddy::GetAlias(nsACString & aAlias)
{
  aAlias = mAlias;
  return NS_OK;
}

/* readonly attribute boolean buddy; */
NS_IMETHODIMP purpleConvChatBuddy::GetBuddy(PRBool *aBuddy)
{
  *aBuddy = mBuddy;
  return NS_OK;
}


/* Get flag from PurpleConvChatBuddyFlags */
#define PURPLE_IMPL_GETFLAG(aName, aFlag)				\
  NS_IMETHODIMP purpleConvChatBuddy::Get##aName(PRBool *a##aName)	\
  {									\
    *a##aName = (mFlags & PURPLE_CBFLAGS_##aFlag) ? PR_TRUE : PR_FALSE; \
    return NS_OK;							\
  }

/* readonly attribute boolean noFlags; */
PURPLE_IMPL_GETFLAG(NoFlags, NONE)

/* readonly attribute boolean voiced; */
PURPLE_IMPL_GETFLAG(Voiced, VOICE)

/* readonly attribute boolean halfOp; */
PURPLE_IMPL_GETFLAG(HalfOp, HALFOP)

/* readonly attribute boolean op; */
PURPLE_IMPL_GETFLAG(Op, OP)

/* readonly attribute boolean founder; */
PURPLE_IMPL_GETFLAG(Founder, FOUNDER)

/* readonly attribute boolean typing; */
PURPLE_IMPL_GETFLAG(Typing, TYPING)
