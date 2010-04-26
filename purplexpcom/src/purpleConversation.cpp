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

#include "purpleConversation.h"
#include "purpleGListEnumerator.h"
#include "purpleIAccount.h"

#pragma GCC visibility push(default)
#include <libpurple/cmds.h>
#include <libpurple/connection.h>
#pragma GCC visibility pop

#include <nsXPCOM.h>
#include <nsStringAPI.h>
#include <nsCOMPtr.h>
#include <nsIDocument.h>
#include <nsIDOMDocument.h>

#ifdef PR_LOGGING
//
// NSPR_LOG_MODULES=purpleConversation:5
//
PRLogModuleInfo *gPurpleConvLog = nsnull;
#endif
#define LOG(args) PR_LOG(gPurpleConvLog, PR_LOG_DEBUG, args)

PRUint32 purpleConversation::sLastId = 0;

purpleConversation::purpleConversation()
  : mUninitialized(PR_FALSE)
{
  /* member initializers and constructor code */
  mId = ++sLastId;
#ifdef PR_LOGGING
  if (!gPurpleConvLog)
    gPurpleConvLog = PR_NewLogModule("purpleConversation");
#endif
  LOG(("new purpleConversation @%x with id %i", this, mId));
}

purpleConversation::~purpleConversation()
{
  /* destructor code */
  LOG(("destructing conversation @%x with id %i", this, mId));
}

/* readonly attribute boolean isChat; */
NS_IMETHODIMP purpleConversation::GetIsChat(PRBool *aIsChat)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

void purpleConversation::SetConv(PurpleConversation *aConv)
{
  mConv = aConv;
  LOG(("conversion ptr set to: @%x", mConv));
  aConv->ui_data = this;
}

/* call this only from the destroy_conv uiop or when exiting */
void purpleConversation::UnInit()
{
  LOG(("purpleConversation::unInit (mId == %i)", mId));
  mConv->ui_data = NULL;
  mConv = NULL;
  mUninitialized = PR_TRUE;
}

/* readonly attribute purpleIAccount account; */
NS_IMETHODIMP purpleConversation::GetAccount(purpleIAccount * *aAccount)
{
  NS_ENSURE_TRUE(mConv, NS_ERROR_NOT_INITIALIZED);

  PurpleAccount *account = purple_conversation_get_account(mConv);
  NS_ENSURE_TRUE(account, NS_ERROR_FAILURE);

  NS_IF_ADDREF(*aAccount = (purpleIAccount *)account->ui_data);
  return NS_OK;
}

/* readonly attribute AUTF8String name; */
NS_IMETHODIMP purpleConversation::GetName(nsACString& aName)
{
  NS_ENSURE_TRUE(mConv, NS_ERROR_NOT_INITIALIZED);

  aName = purple_conversation_get_name(mConv);
  return NS_OK;
}

/* readonly attribute AUTF8String normalizedName; */
NS_IMETHODIMP purpleConversation::GetNormalizedName(nsACString& aNormalizedName)
{
  NS_ENSURE_TRUE(mConv, NS_ERROR_NOT_INITIALIZED);

  return NS_ERROR_NOT_IMPLEMENTED;
}

/* readonly attribute AUTF8String title; */
NS_IMETHODIMP purpleConversation::GetTitle(nsACString& aTitle)
{
  NS_ENSURE_TRUE(mConv, NS_ERROR_NOT_INITIALIZED);

  aTitle = purple_conversation_get_title(mConv);
  return NS_OK;
}

/* readonly attribute long idConv; */
NS_IMETHODIMP purpleConversation::GetId(PRUint32 *aId)
{
  *aId = mId;
  return NS_OK;
}

/* boolean doCommand (in AUTF8String aMsg); */
NS_IMETHODIMP purpleConversation::DoCommand(const nsACString &aMsg,
                                            PRBool *aHandled)
{
  NS_ASSERTION(!aMsg.IsEmpty(), "Looking for commands in an empty message?");
  NS_ENSURE_TRUE(mConv, NS_ERROR_NOT_INITIALIZED);

  PromiseFlatCString flatMsg(aMsg);
  const char *msg = flatMsg.get();

  if (*msg != '/' || *(msg + 1) == '/') {
    *aHandled = PR_FALSE;
    return NS_OK;
  }

  char *error = NULL;
  PurpleCmdStatus status = purple_cmd_do_command(mConv, msg + 1,
                                                 msg, &error);
  g_free(error);
  switch (status) {
    case PURPLE_CMD_STATUS_OK:
      LOG(("Command executed in conversation with mId %i", mId));
      *aHandled = PR_TRUE;
      return NS_OK;

    case PURPLE_CMD_STATUS_NOT_FOUND:
      {
        // Check if '/' is native to the protocol. If it is, a
        // non-existing command is likely an error, don't send the
        // message
        nsCOMPtr<purpleIAccount> account;
        nsresult rv = GetAccount(getter_AddRefs(account));
        NS_ENSURE_SUCCESS(rv, rv);
        NS_ENSURE_TRUE(account, NS_ERROR_NULL_POINTER);

        nsCOMPtr<purpleIProtocol> proto;
        rv = account->GetProtocol(getter_AddRefs(proto));
        NS_ENSURE_SUCCESS(rv, rv);

        PRBool slashCommandsNative = PR_FALSE;
        rv = proto->GetSlashCommandsNative(&slashCommandsNative);
        NS_ENSURE_SUCCESS(rv, rv);

        if (slashCommandsNative) {
          LOG(("The '/' is native on this protocol, a non-existing command is an error"));
          return NS_ERROR_FAILURE;
        }
      }

    case PURPLE_CMD_STATUS_WRONG_TYPE:
    case PURPLE_CMD_STATUS_WRONG_PRPL:
      *aHandled = PR_FALSE;
      return NS_OK;

    case PURPLE_CMD_STATUS_WRONG_ARGS:
    case PURPLE_CMD_STATUS_FAILED:
    default: // only to work around the warning
             // 'control reaches end of non-void function'
      LOG(("Error while attempting to execute the command"));
      return NS_ERROR_FAILURE;
  }
}

/* void sendMsg (in AUTF8String aMsg); */
NS_IMETHODIMP purpleConversation::SendMsg(const nsACString& aMsg)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* void close (); */
NS_IMETHODIMP purpleConversation::Close()
{
  NS_ENSURE_TRUE(mConv || mUninitialized, NS_ERROR_NOT_INITIALIZED);

  LOG(("purpleConversation::Close (mId = %i)", mId));

  if (!mUninitialized) {
    purple_conversation_destroy(mConv);
    mConv = NULL;
  }

  return NS_OK;
}

/* void addObserver (in nsIObserver aObserver); */
NS_IMETHODIMP purpleConversation::AddObserver(nsIObserver *aObserver)
{
  NS_ENSURE_ARG_POINTER(aObserver);
  NS_ENSURE_TRUE(mObservers.IndexOfObject(aObserver) == -1,
                 NS_ERROR_ALREADY_INITIALIZED);
  LOG(("purpleConversation::AddObserver (mId = %i, observer = @%x)",
      mId, aObserver));

  mObservers.AppendObject(aObserver);
  return NS_OK;
}

/* void removeObserver (in nsIObserver aObserver); */
NS_IMETHODIMP purpleConversation::RemoveObserver(nsIObserver *aObserver)
{
  NS_ENSURE_ARG_POINTER(aObserver);

  LOG(("purpleConversation::RemoveObserver (mId = %i, observer = @%x)",
       mId, aObserver));
  NS_ENSURE_TRUE(mObservers.RemoveObject(aObserver), NS_ERROR_FAILURE);
  return NS_OK;
}

void purpleConversation::NotifyObservers(nsISupports *aSubject,
                                         const char *aTopic,
                                         const PRUnichar *aData)
{
  LOG(("purpleConversation::NotifyObservers (mId = %i, topic = %s)",
       mId, aTopic));
  for (PRInt32 i = 0; i < mObservers.Count(); ++i)
    mObservers[i]->Observe(aSubject, aTopic, aData);
}

/* void setBaseURI (in nsIDOMDocument aDocument, in nsIURI aURI); */
NS_IMETHODIMP purpleConversation::SetBaseURI(nsIDOMDocument *aDocument, nsIURI *aURI)
{
  NS_ENSURE_ARG_POINTER(aDocument);
  NS_ENSURE_ARG_POINTER(aURI);

  nsCOMPtr<nsIDocument> doc = do_QueryInterface(aDocument);
  NS_ENSURE_TRUE(doc, NS_ERROR_UNEXPECTED);

  nsresult rv = doc->SetBaseURI(aURI);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}
