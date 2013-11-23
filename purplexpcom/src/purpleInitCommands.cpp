/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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

class purpleCommand MOZ_FINAL : public imICommand
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_IMICOMMAND

  purpleCommand(const gchar *name, const gchar *helpstr, PurpleCmdFlag f,
                PurpleCmdPriority p, PurpleCmd *cmd)
    : mName(name),
      mHelpString(helpstr),
      mUsageContext(f & 3),
      mPriority(p - PURPLE_CMD_P_DEFAULT),
      mCmdData(cmd)
  {}

private:
  ~purpleCommand() {}
  nsCString mName;
  nsCString mHelpString;
  PRInt32 mUsageContext;
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

/* readonly attribute PRInt32 usageContext; */
NS_IMETHODIMP purpleCommand::GetUsageContext(PRInt32 *aUsageContext)
{
  *aUsageContext = mUsageContext;
  return NS_OK;
}

/* readonly attribute PRInt32 priority; */
NS_IMETHODIMP purpleCommand::GetPriority(PRInt32 *aPriority)
{
  *aPriority = mPriority;
  return NS_OK;
}

/* boolean run (in AUTF8String aMessage,
                [optional] in prplIConversation aConversation); */
NS_IMETHODIMP purpleCommand::Run(const nsACString & aMessage,
                                 prplIConversation *aConversation,
                                 bool *aResult)
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
                             PurpleCmdFlag f, const gchar *prpl_id,
                             const gchar *helpstr, PurpleCmd *cmd)
{
  NS_ENSURE_TRUE(cmd && name, );
  LOG(("register_command %s (%s)\n", name, prpl_id));

  nsCOMPtr<imICommandsService> commands =
    do_GetService(IM_COMMANDS_SERVICE_CONTRACTID);
  NS_ENSURE_TRUE(commands, );

  nsresult rv =
    commands->RegisterCommand(new purpleCommand(name, helpstr, f, p, cmd),
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
