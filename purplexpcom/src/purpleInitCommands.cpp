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

#include "imICommandsService.h"
#include "purpleConvIM.h"
#include "purpleConvChat.h"

#pragma GCC visibility push(default)
#include <libpurple/cmds.h>
#pragma GCC visibility pop

#include <nsIClassInfo.h>
#include <nsServiceManagerUtils.h>
#include <nsStringAPI.h>

#ifdef PR_LOGGING
//
// NSPR_LOG_MODULES=purpleInit:5
//
extern PRLogModuleInfo *gPurpleInitLog;
#endif
#define LOG(args) PR_LOG(gPurpleInitLog, PR_LOG_DEBUG, args)

NS_DEFINE_NAMED_CID(PURPLE_CONV_CHAT_CID);
NS_DEFINE_NAMED_CID(PURPLE_CONV_IM_CID);

class purpleCommand : public imICommand
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_IMICOMMAND

  purpleCommand(const gchar *name, const gchar *helpstr, PurpleCmdPriority p,
                PurpleCmd *cmd)
    : mName(name),
      mHelpString(helpstr),
      mPriority(p - PURPLE_CMD_P_DEFAULT),
      mCmdData(cmd)
  {}

private:
  ~purpleCommand() {}
  nsCString mName;
  nsCString mHelpString;
  PRInt32 mPriority;
  PurpleCmd *mCmdData;
};

NS_IMPL_ISUPPORTS1(purpleCommand, imICommand)

/* readonly attribute AUTF8String name; */
NS_IMETHODIMP purpleCommand::GetName(nsACString & aName)
{
  aName = mName;
  return NS_OK;
}

/* readonly attribute AUTF8String helpString; */
NS_IMETHODIMP purpleCommand::GetHelpString(nsACString & aHelpString)
{
  aHelpString = mHelpString;
  return NS_OK;
}

/* readonly attribute PRInt32 priority; */
NS_IMETHODIMP purpleCommand::GetPriority(PRInt32 *aPriority)
{
  *aPriority = mPriority;
  return NS_OK;
}

/* boolean run (in AUTF8String aMessage,
                [optional] in purpleIConversation aConversation); */
NS_IMETHODIMP purpleCommand::Run(const nsACString & aMessage,
                                 purpleIConversation *aConversation,
                                 PRBool *aResult NS_OUTPARAM)
{
  // libpurple commands don't work without a conversation.
  NS_ENSURE_TRUE(aConversation, NS_ERROR_INVALID_ARG);

  // Get the nsIClassInfo implementation to ensure we are on a libpurple conv.
  nsresult rv;
  nsCOMPtr<nsIClassInfo> convCI = do_QueryInterface(aConversation, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCID cid;
  rv = convCI->GetClassIDNoAlloc(&cid);
  if (NS_FAILED(rv) ||
      (!cid.Equals(kPURPLE_CONV_IM_CID) &&
       !cid.Equals(kPURPLE_CONV_CHAT_CID))) {
    LOG(("ignoring command because not in a libpurple based conversation"));
    *aResult = PR_FALSE;
    return NS_OK;
  }

  PurpleConversation *conv =
    static_cast<purpleConversation *>(aConversation)->GetConv();
  *aResult = purple_cmd_execute(mCmdData, conv,
                                PromiseFlatCString(aMessage).get());
  LOG(("running libpurple command with priority %i: %s", mPriority,
       *aResult ? "OK" : "Failed"));
  return NS_OK;
}

/*** Commands uiops ***/
static void register_command(const gchar *name, PurpleCmdPriority p,
                             const gchar *prpl_id, const gchar *helpstr,
                             PurpleCmd *cmd)
{
  NS_ENSURE_TRUE(cmd && name, );
  LOG(("register_command %s (%s)\n", name, prpl_id));

  nsCOMPtr<imICommandsService> commands =
    do_GetService(IM_COMMANDS_SERVICE_CONTRACTID);
  NS_ENSURE_TRUE(commands, );

  nsresult rv =
    commands->RegisterCommand(new purpleCommand(name, helpstr, p, cmd),
                              prpl_id ? nsDependentCString(prpl_id)
                                      : EmptyCString());
  NS_ENSURE_SUCCESS(rv, );
}

static void unregister_command(const gchar *name, const gchar *prpl_id)
{
  NS_ENSURE_TRUE(name, );
  LOG(("unregister_command %s\n", name));

  nsCOMPtr<imICommandsService> commands =
    do_GetService(IM_COMMANDS_SERVICE_CONTRACTID);
  NS_ENSURE_TRUE(commands, );

  nsresult rv = commands->UnregisterCommand(nsDependentCString(name),
                                            prpl_id ? nsDependentCString(prpl_id)
                                                    : EmptyCString());
  NS_ENSURE_SUCCESS(rv, );
}

static PurpleCommandsUiOps cmds_uiops =
{
  register_command,          /* register_command   */
  unregister_command,        /* unregister_command */

  NULL, NULL, NULL, NULL
};

void init_libpurple_commands()
{
  purple_cmds_set_ui_ops(&cmds_uiops);
}
