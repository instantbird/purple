/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "purpleConvChatBuddy.h"
#include <nsIClassInfoImpl.h>
#include <nsIProgrammingLanguage.h>
#include <nsMemory.h>

// B1724DEA-3ED4-432A-931C-0014FC1FBA27
#define PURPLE_CONV_CHAT_BUDDY_CID                            \
  { 0xB1724DEA, 0x3ED4, 0x432A,                               \
    { 0x93, 0x1C, 0x00, 0x14, 0xFC, 0x1F, 0xBA, 0x27 }        \
 }

NS_IMPL_CLASSINFO(purpleConvChatBuddy, NULL, 0, PURPLE_CONV_CHAT_BUDDY_CID)
NS_IMPL_THREADSAFE_CI(purpleConvChatBuddy)
NS_IMPL_ISUPPORTS1_CI(purpleConvChatBuddy, prplIConvChatBuddy)

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
NS_IMETHODIMP purpleConvChatBuddy::GetBuddy(bool *aBuddy)
{
  *aBuddy = mBuddy;
  return NS_OK;
}


/* Get flag from PurpleConvChatBuddyFlags */
#define PURPLE_IMPL_GETFLAG(aName, aFlag)                               \
  NS_IMETHODIMP purpleConvChatBuddy::Get##aName(bool *a##aName)         \
  {                                                                     \
    *a##aName = (mFlags & PURPLE_CBFLAGS_##aFlag) ? PR_TRUE : PR_FALSE; \
    return NS_OK;                                                       \
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
