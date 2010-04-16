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
 * 2009.
 *
 * The Initial Developer of the Original Code is
 * Florian QUEZE <florian@instantbird.org>.
 * Portions created by the Initial Developer are Copyright (C) 2009
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

#include "purpleOverrideProtocol.h"
#include <prlog.h>
#include <nsIXPConnect.h>
#include <nsIClassInfoImpl.h>
#include <nsMemory.h>

#ifdef PR_LOGGING
//
// NSPR_LOG_MODULES=purpleOverrideProtocol:5
//
static PRLogModuleInfo *gPurpleOverrideProtocolLog = nsnull;
#endif
#define LOG(args) PR_LOG(gPurpleOverrideProtocolLog, PR_LOG_DEBUG, args)

/* Implementation file */
NS_IMPL_ISUPPORTS2_CI(purpleOverrideProtocol, purpleIOverrideProtocol, purpleIProtocol)

purpleOverrideProtocol::purpleOverrideProtocol()
{
  /* member initializers and constructor code */
#ifdef PR_LOGGING
  if (!gPurpleOverrideProtocolLog)
    gPurpleOverrideProtocolLog = PR_NewLogModule("purpleOverrideProtocol");
#endif
  LOG(("Creating override protocol @%x", this));
}

/* void Init (in purpleIProtocol aBase, in purpleIProtocol aOverride); */
NS_IMETHODIMP purpleOverrideProtocol::Init(purpleIProtocol *aBase,
                                           purpleIProtocol *aOverride)
{
  LOG(("Initializing override protocol @%x", this));
  NS_ENSURE_ARG_POINTER(aBase);
  NS_ENSURE_ARG_POINTER(aOverride);

  mBase = aBase;
  mOverride = aOverride;

  return NS_OK;
}

/* readonly attribute purpleIProtocol basePrpl; */
NS_IMETHODIMP purpleOverrideProtocol::GetBasePrpl(purpleIProtocol * *aBasePrpl)
{
  NS_ENSURE_TRUE(mBase, NS_ERROR_NOT_INITIALIZED);

  NS_ADDREF(*aBasePrpl = mBase);
  return NS_OK;
}

/* readonly attribute purpleIProtocol override; */
NS_IMETHODIMP purpleOverrideProtocol::GetOverride(purpleIProtocol * *aOverride)
{
  NS_ENSURE_TRUE(mOverride, NS_ERROR_NOT_INITIALIZED);

  NS_ADDREF(*aOverride = mOverride);
  return NS_OK;
}

purpleOverrideProtocol::~purpleOverrideProtocol()
{
  /* destructor code */
  LOG(("Destructing override protocol (@%x)", this));
}

#define IMPL_FORWARD(aName, aProto, aParams)                            \
  NS_IMETHODIMP purpleOverrideProtocol::aName aProto                    \
  {                                                                     \
    NS_ENSURE_TRUE(mOverride && mBase, NS_ERROR_NOT_INITIALIZED);       \
    nsresult rv = mOverride->aName aParams;                             \
    if (rv != NS_ERROR_NOT_IMPLEMENTED)                                 \
      return rv;                                                        \
                                                                        \
    return mBase->aName aParams;                                        \
  }


IMPL_FORWARD(GetName, (nsACString & aName), (aName))
IMPL_FORWARD(GetId, (nsACString & aId), (aId))
IMPL_FORWARD(GetIconBaseURI, (nsACString & aIconBaseURI), (aIconBaseURI))
IMPL_FORWARD(GetOptions, (nsISimpleEnumerator **_retval), (_retval))
IMPL_FORWARD(GetUsernameSplit, (nsISimpleEnumerator **_retval), (_retval))
IMPL_FORWARD(GetUsernameEmptyText, (nsACString &aUsernameEmptyText), (aUsernameEmptyText))
IMPL_FORWARD(AccountExists, (const nsACString & aName, PRBool *_retval),
             (aName, _retval))

#define IMPL_FORWARD_GETBOOLOPT(aName) \
  IMPL_FORWARD(Get##aName, (PRBool *a##aName), (a##aName))

IMPL_FORWARD_GETBOOLOPT(UniqueChatName)
IMPL_FORWARD_GETBOOLOPT(ChatHasTopic)
IMPL_FORWARD_GETBOOLOPT(NoPassword)
IMPL_FORWARD_GETBOOLOPT(NewMailNotification)
IMPL_FORWARD_GETBOOLOPT(ImagesInIM)
IMPL_FORWARD_GETBOOLOPT(PasswordOptional)
IMPL_FORWARD_GETBOOLOPT(UsePointSize)
IMPL_FORWARD_GETBOOLOPT(RegisterNoScreenName)
IMPL_FORWARD_GETBOOLOPT(SlashCommandsNative)

// Forward non scriptable methods directly to the base
#define IMPL_FORWARD_BASE(aName, aProto, aParams)                       \
  NS_IMETHODIMP purpleOverrideProtocol::aName aProto                    \
  {                                                                     \
    NS_ENSURE_TRUE(mBase, NS_ERROR_NOT_INITIALIZED);                    \
    return mBase->aName aParams;                                        \
  }

IMPL_FORWARD_BASE(GetInfo, (PurplePluginProtocolInfo * *_retval), (_retval))
IMPL_FORWARD_BASE(LoadAccount, (const nsACString & aKey, purpleIAccount **aResult), (aKey, aResult))
IMPL_FORWARD_BASE(CreateAccount, (const nsACString & aName, const nsACString & aKey, purpleIAccount **aResult), (aName, aKey, aResult))
IMPL_FORWARD_BASE(CreatePurpleAccount,
                  (const nsACString & aName,
                   purpleIAccount *aAccount,
                   PurpleAccount * *_retval),
                  (aName, aAccount, _retval))
