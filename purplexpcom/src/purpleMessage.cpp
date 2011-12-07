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

#include "purpleMessage.h"
#include "purpleCoreService.h"
#include "purpleConversation.h"
#include "purpleConvIM.h"
#include "imIContactsService.h"
#include <nsCOMPtr.h>
#include <nsServiceManagerUtils.h>
#include <nsIClassInfoImpl.h>
#include <nsMemory.h>

#ifdef PR_LOGGING
//
// NSPR_LOG_MODULES=purpleMessage:5
//
static PRLogModuleInfo *gPurpleMessageLog = nsnull;
#endif
#define LOG(args) PR_LOG(gPurpleMessageLog, PR_LOG_DEBUG, args)

NS_IMPL_CLASSINFO(purpleMessage, NULL, 0, PURPLE_MESSAGE_CID)
NS_IMPL_ISUPPORTS1_CI(purpleMessage, prplIMessage)

PRUint32 purpleMessage::sLastId = 0;

purpleMessage::purpleMessage()
{
  /* member initializers and constructor code */
#ifdef PR_LOGGING
  if (!gPurpleMessageLog)
    gPurpleMessageLog = PR_NewLogModule("purpleMessage");
#endif
  LOG(("Constructing purpleMessage @%x", this));
  mId = ++sLastId;
}

purpleMessage::~purpleMessage()
{
  /* destructor code */
  LOG(("Destructing purpleMessage @%x", this));
}

nsresult purpleMessage::Init(PurpleConversation *conv, const char *who, const char *alias,
                             const char *message, PurpleMessageFlags flags, time_t mtime)
{
  LOG(("Initializing purpleMessage"));

  mConv = conv;
  mWho = who;
  mAlias = alias;
  mMessage = message;
  mOriginalMessage = mMessage;
  mFlags = flags;
  mTime = mtime;
  return NS_OK;
}

#define PURPLE_IMPL_GETSTRING(aName, aString)                   \
  NS_IMETHODIMP purpleMessage::Get##aName(nsACString& a##aName) \
  {                                                             \
    a##aName = aString;                                         \
    return NS_OK;                                               \
  }

/* readonly attribute PRUint32 id; */
NS_IMETHODIMP purpleMessage::GetId(PRUint32 *aId)
{
  *aId = mId;
  return NS_OK;
}

/* readonly attribute AUTF8String who; */
PURPLE_IMPL_GETSTRING(Who, mWho)

/* readonly attribute AUTF8String alias; */
PURPLE_IMPL_GETSTRING(Alias, mAlias)

/* readonly attribute PRTime time; */
NS_IMETHODIMP purpleMessage::GetTime(PRTime *aTime)
{
  *aTime = mTime;
  return NS_OK;
}

/* readonly attribute AUTF8String originalMessage; */
PURPLE_IMPL_GETSTRING(OriginalMessage, mOriginalMessage)

/*          attribute AUTF8String message; */
PURPLE_IMPL_GETSTRING(Message, mMessage)

NS_IMETHODIMP purpleMessage::SetMessage(nsACString const& aMessage)
{
  mMessage = aMessage;
  return NS_OK;
}

/* readonly attribute AUTF8String iconURL; */
NS_IMETHODIMP purpleMessage::GetIconURL(nsACString& aUserIconURL)
{
  if (!(mFlags & PURPLE_MESSAGE_RECV)) {
    aUserIconURL = "";
    return NS_OK;
  }

  NS_ENSURE_TRUE(mConv, NS_ERROR_NOT_INITIALIZED);
  purpleConversation *conv = purpleConversation::fromPurpleConv(mConv);
  NS_ENSURE_TRUE(conv, NS_ERROR_NOT_INITIALIZED);

  PRBool isChat = PR_TRUE;
  nsresult rv = conv->GetIsChat(&isChat);
  NS_ENSURE_SUCCESS(rv, rv);

  if (!isChat) {
    nsCOMPtr<imIAccountBuddy> buddy;
    rv = static_cast<purpleConvIM *>(conv)->GetBuddy(getter_AddRefs(buddy));
    NS_ENSURE_SUCCESS(rv, rv);

    if (buddy) {
      LOG(("GetIconURL: found a buddy for the message, returning its icon"));
      return buddy->GetBuddyIconFilename(aUserIconURL);
    }
  }

  aUserIconURL = "";
  return NS_OK;
}

/* readonly attribute prplIConversation conversation; */
NS_IMETHODIMP purpleMessage::GetConversation(prplIConversation * *aConversation)
{
  NS_ENSURE_TRUE(mConv, NS_ERROR_NOT_INITIALIZED);

  NS_IF_ADDREF(*aConversation = purpleConversation::fromPurpleConv(mConv));
  return NS_OK;
}

/* attribute AUTF8String color; */
NS_IMETHODIMP purpleMessage::GetColor(nsACString & aColor)
{
  aColor = mColor;
  return NS_OK;
}
NS_IMETHODIMP purpleMessage::SetColor(const nsACString & aColor)
{
  mColor = aColor;
  return NS_OK;
}

#define PURPLE_IMPL_GETBOOL(aName, aFlag)                               \
  NS_IMETHODIMP purpleMessage::Get##aName(PRBool *a##aName)             \
  {                                                                     \
    *a##aName = (mFlags & PURPLE_MESSAGE_##aFlag) ? PR_TRUE : PR_FALSE; \
    return NS_OK;                                                       \
  }

/* readonly attribute boolean outgoing; */
PURPLE_IMPL_GETBOOL(Outgoing, SEND)

/* readonly attribute boolean incoming; */
PURPLE_IMPL_GETBOOL(Incoming, RECV)

/* readonly attribute boolean system; */
PURPLE_IMPL_GETBOOL(System, SYSTEM)

/* readonly attribute boolean autoResponse; */
PURPLE_IMPL_GETBOOL(AutoResponse, AUTO_RESP)

/* readonly attribute boolean containsNick; */
PURPLE_IMPL_GETBOOL(ContainsNick, NICK)

/* readonly attribute boolean noLog; */
PURPLE_IMPL_GETBOOL(NoLog, NO_LOG)

/* readonly attribute boolean whispered; */
PURPLE_IMPL_GETBOOL(Whispered, WHISPER)

/* readonly attribute boolean error; */
PURPLE_IMPL_GETBOOL(Error, ERROR)

/* readonly attribute boolean delayed; */
PURPLE_IMPL_GETBOOL(Delayed, DELAYED)

/* readonly attribute boolean noFormat; */
PURPLE_IMPL_GETBOOL(NoFormat, RAW)

/* readonly attribute boolean containsImages; */
PURPLE_IMPL_GETBOOL(ContainsImages, IMAGES)

/* readonly attribute boolean notification; */
PURPLE_IMPL_GETBOOL(Notification, NOTIFY)

/* readonly attribute boolean noLinkification; */
PURPLE_IMPL_GETBOOL(NoLinkification, NO_LINKIFY)

/* void getActions ([optional] out unsigned long actionCount,
                    [array, retval, size_is (actionCount)] out prplIMessageAction actions); */
NS_IMETHODIMP purpleMessage::GetActions(PRUint32 *actionCount,
                                        prplIMessageAction ***actions)
{
  *actionCount = 0;
  *actions = nsnull;
  return NS_OK;
}
