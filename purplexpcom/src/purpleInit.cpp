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

#pragma GCC visibility push(default)
#include <libpurple/cmds.h>
#include <libpurple/core.h>
#include <libpurple/gettext.h>
#pragma GCC visibility pop

#include "purpleAccount.h"
#include "purpleAccountBuddy.h"
#include "purpleCoreService.h"
#include "purpleGetText.h"
#include "purpleTimer.h"
#include "purpleSockets.h"
#include "purpleDebug.h"
#include "purpleDNS.h"
#include <purpleIPlugin.h>
#include <nsServiceManagerUtils.h>
#include <nsCOMPtr.h>
#include <nsStringAPI.h>
#include <nsNetCID.h>
#include <nsICategoryManager.h>
#include <nsIIdleService.h>
#include <nsIPrefBranch2.h>
#include <nsIPrefService.h>
#include <nsICancelable.h>
#include <nsISupportsPrimitives.h>
#include <nsIXULAppInfo.h>
#include <nsComponentManagerUtils.h>
#include <nsDirectoryServiceUtils.h>
#include <nsAppDirectoryServiceDefs.h>
#include <nsDirectoryServiceDefs.h>

#define PURPLE_PROTOCOL_PLUGIN_CATEGORY   "purple-protocol-plugin"


#ifdef PR_LOGGING
//
// NSPR_LOG_MODULES=purpleInit:5
//
PRLogModuleInfo *gPurpleInitLog = nsnull;
#endif
#define LOG(args) PR_LOG(gPurpleInitLog, PR_LOG_DEBUG, args)

#if defined(PURPLE_PLUGINS) && defined(XP_UNIX) && !defined(XP_MACOSX)
#include <nsILocalFile.h>

static nsresult open_libpurple(void)
{
  nsCOMPtr<nsIFile> folder;
  nsresult rv = NS_GetSpecialDirectory("resource:app",
                                       getter_AddRefs(folder));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsILocalFile> libpurple = do_QueryInterface(folder);
  NS_ENSURE_TRUE(libpurple, rv);

  rv = libpurple->AppendNative(NS_LITERAL_CSTRING("libpurple.so"));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCString pathName;
  libpurple->GetNativePath(pathName);

  PRLibSpec libSpec;
  libSpec.type = PR_LibSpec_Pathname;
  libSpec.value.pathname = pathName.get();

  // We will leak the result...
  PRLibrary *lib =
    PR_LoadLibraryWithFlags(libSpec, PR_LD_NOW | PR_LD_GLOBAL);
  NS_ENSURE_TRUE(lib, NS_ERROR_FAILURE);

  return NS_OK;
}
#else
#define open_libpurple()
#endif


static PurpleEventLoopUiOps eventloops =
{
  purpleTimer::AddTimeout, /* hack to prevent from writting .xml files to disk */
  purpleTimer::CancelTimer,
  purpleSocketWatcher::AddWatch,
  purpleSocketWatcher::CancelWatch,
  NULL,

  /* padding */
  NULL, NULL, NULL, NULL
};
/*** End of the eventloop functions. ***/

static PurpleDebugUiOps debugconsoleuiops = {
  purpleDebug::ReportMessage,
  purpleDebug::ReportMessageWithLocation,
  purpleDebug::Enabled,
  NULL,NULL,NULL,NULL
};

static PurpleDnsQueryUiOps dnsUiOps = {
  purpleDNS::Resolve,
  purpleDNS::Cancel,

  NULL,NULL,NULL,NULL
};

// Taken from pidgin/gtkconv.c (GPL, ...)
static PurpleCmdRet raw_command_cb(PurpleConversation *conv,
                                   const char *cmd, char **args,
                                   char **error, void *data)
{
  if (purple_conversation_get_type(conv) == PURPLE_CONV_TYPE_IM)
    purple_conv_im_send(PURPLE_CONV_IM(conv), args[0]);
  else if (purple_conversation_get_type(conv) == PURPLE_CONV_TYPE_CHAT)
    purple_conv_chat_send(PURPLE_CONV_CHAT(conv), args[0]);

  return PURPLE_CMD_RET_OK;
}

#define IMPL_STATUS_COMMAND(aName, aStatus)                             \
  static PurpleCmdRet aName##_command_cb(PurpleConversation *conv,      \
                                         const char *cmd, char **args,  \
                                         char **error, void *data)      \
  {                                                                     \
    nsCString message(args[0]);                                         \
                                                                        \
    nsCOMPtr<purpleICoreService> pcs =                                  \
      do_GetService(PURPLE_CORE_SERVICE_CONTRACTID);                    \
    pcs->SetStatus(purpleICoreService::STATUS_##aStatus, message);      \
                                                                        \
    return PURPLE_CMD_RET_OK;                                           \
  }

IMPL_STATUS_COMMAND(back, AVAILABLE)
IMPL_STATUS_COMMAND(away, AWAY)
IMPL_STATUS_COMMAND(busy, UNAVAILABLE)
IMPL_STATUS_COMMAND(offline, OFFLINE)

// GetText ui ops
PurpleGetTextUiOps gettext_uiops = {
  purpleGetText::GetText,
  purpleGetText::GetPluralText,

  NULL, NULL, NULL, NULL
};

class prefsObserver : public nsIObserver
{
  public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIOBSERVER
    prefsObserver(void *aCallback) : callback(aCallback) {}

  private:
    gpointer callback;
};

NS_IMPL_ISUPPORTS1(prefsObserver, nsIObserver)

NS_IMETHODIMP prefsObserver::Observe(nsISupports *aSubject,
                                     const char *aTopic,
                                     const PRUnichar *someData)
{
  LOG(("prefsObserver::Observe topic = %s\n", aTopic));
  purple_prefs_observe(callback);

  return NS_OK;
}

// Prefs ui ops
#define DEFINE_mozName_from_name(name)          \
  NS_ENSURE_TRUE(name && *name == '/',);        \
                                                \
  char *mozName = g_strdup(name + 1);           \
  for (int i = 0; mozName[i]; ++i)              \
    if (mozName[i] == '/')                      \
      mozName[i] = '.';

#define DEFINE_mozName_from_name_with_default(name, default)    \
  NS_ENSURE_TRUE(name && *name == '/', default);                \
                                                                \
  char *mozName = g_strdup(name + 1);                           \
  for (int i = 0; mozName[i]; ++i)                              \
    if (mozName[i] == '/')                                      \
      mozName[i] = '.';

void prefs_add_none(const char *name)
{
  LOG(("add pref none %s\n", name));
}

void prefs_add_bool(const char *name, gboolean value)
{
  LOG(("add pref bool %s=%i\n", name, value));
  DEFINE_mozName_from_name(name);

  nsCOMPtr<nsIPrefBranch> prefs =
    do_GetService(NS_PREFSERVICE_CONTRACTID);
  PRInt32 type = nsIPrefBranch::PREF_INVALID;
  prefs->GetPrefType(mozName, &type);

  if (type == nsIPrefBranch::PREF_INVALID) {
#ifdef DEBUG
    printf("pref(\"%s\", %s);\n", mozName, value ? "true" : "false");
#endif
    prefs->SetBoolPref(mozName, value);
  }
  g_free(mozName);
}

void prefs_add_int(const char *name, int value)
{
  LOG(("add pref int %s=%i\n", name, value));
  DEFINE_mozName_from_name(name);

  nsCOMPtr<nsIPrefBranch> prefs =
    do_GetService(NS_PREFSERVICE_CONTRACTID);
  PRInt32 type = nsIPrefBranch::PREF_INVALID;
  prefs->GetPrefType(mozName, &type);

  if (type == nsIPrefBranch::PREF_INVALID) {
#ifdef DEBUG
    printf("pref(\"%s\", %i);\n", mozName, value);
#endif
    prefs->SetIntPref(mozName, value);
  }
  g_free(mozName);
}

void prefs_add_string(const char *name, const char *value)
{
  LOG(("add pref string %s=%s\n", name, value));
  DEFINE_mozName_from_name(name);

  nsCOMPtr<nsIPrefBranch> prefs =
    do_GetService(NS_PREFSERVICE_CONTRACTID);
  PRInt32 type = nsIPrefBranch::PREF_INVALID;
  prefs->GetPrefType(mozName, &type);

  if (type == nsIPrefBranch::PREF_INVALID) {
#ifdef DEBUG
    printf("pref(\"%s\", \"%s\");\n", mozName, value);
#endif
    prefs->SetCharPref(mozName, value);
  }
  g_free(mozName);
}

void prefs_remove(const char *name)
{
  LOG(("remove pref %s\n", name));
  NS_ENSURE_TRUE(name && *name, );
  DEFINE_mozName_from_name(name);
  nsCOMPtr<nsIPrefBranch> prefs =
    do_GetService(NS_PREFSERVICE_CONTRACTID);
  prefs->DeleteBranch(mozName);
  g_free(mozName);
}

void prefs_set_bool(const char *name, gboolean value)
{
  LOG(("set bool pref %s=%i\n", name, value));
  DEFINE_mozName_from_name(name);

  nsCOMPtr<nsIPrefBranch> prefs =
    do_GetService(NS_PREFSERVICE_CONTRACTID);
  prefs->SetBoolPref(mozName, value);
  g_free(mozName);
}

void prefs_set_int(const char *name, int value)
{
  LOG(("set int pref %s=%i\n", name, value));
  DEFINE_mozName_from_name(name);

  nsCOMPtr<nsIPrefBranch> prefs =
    do_GetService(NS_PREFSERVICE_CONTRACTID);
  prefs->SetIntPref(mozName, value);
  g_free(mozName);
}

void prefs_set_string(const char *name, const char *value)
{
  LOG(("set string pref %s=%s\n", name, value));
  DEFINE_mozName_from_name(name);

  nsCOMPtr<nsIPrefBranch> prefs =
    do_GetService(NS_PREFSERVICE_CONTRACTID);
  prefs->SetCharPref(mozName, value);
  g_free(mozName);
}


gboolean prefs_exists(const char *name)
{
  LOG(("pref exists *%s\n", name));
  DEFINE_mozName_from_name_with_default(name, false);

  nsCOMPtr<nsIPrefBranch> prefs =
    do_GetService(NS_PREFSERVICE_CONTRACTID);
  PRInt32 type = nsIPrefBranch::PREF_INVALID;
  prefs->GetPrefType(mozName, &type);
  g_free(mozName);

  return type != nsIPrefBranch::PREF_INVALID;
}

PurplePrefType prefs_get_type(const char *name)
{
  LOG(("pref get type %s\n", name));
  DEFINE_mozName_from_name_with_default(name, PURPLE_PREF_NONE);

  nsCOMPtr<nsIPrefBranch> prefs =
    do_GetService(NS_PREFSERVICE_CONTRACTID);
  PRInt32 type = nsIPrefBranch::PREF_INVALID;
  prefs->GetPrefType(mozName, &type);
  g_free(mozName);

  switch (type) {
    case nsIPrefBranch::PREF_INT:
      return PURPLE_PREF_INT;
    case nsIPrefBranch::PREF_BOOL:
      return PURPLE_PREF_BOOLEAN;
    case nsIPrefBranch::PREF_STRING:
      return PURPLE_PREF_STRING;
    default:
      return PURPLE_PREF_NONE;
  }
}


gboolean prefs_get_bool(const char *name)
{
  LOG(("get bool pref %s\n", name));
  DEFINE_mozName_from_name_with_default(name, PR_FALSE);

  nsCOMPtr<nsIPrefBranch> prefs =
    do_GetService(NS_PREFSERVICE_CONTRACTID);
  PRBool result = PR_FALSE;
  prefs->GetBoolPref(mozName, &result);
  g_free(mozName);
  return result;
}

int prefs_get_int(const char *name)
{
  LOG(("get int pref %s\n", name));
  DEFINE_mozName_from_name_with_default(name, 0);

  nsCOMPtr<nsIPrefBranch> prefs =
    do_GetService(NS_PREFSERVICE_CONTRACTID);
  PRInt32 result = 0;
  prefs->GetIntPref(mozName, &result);
  g_free(mozName);
  return result;
}

const char *prefs_get_string(const char *name)
{
  LOG(("get string pref %s\n", name));
  DEFINE_mozName_from_name_with_default(name, 0);

  nsCOMPtr<nsIPrefBranch> prefs =
    do_GetService(NS_PREFSERVICE_CONTRACTID);
  char *result = NULL;
  prefs->GetCharPref(mozName, &result);
  g_free(mozName);

#ifdef DEBUG
  char warnMsg[1024];
  if (PR_snprintf(warnMsg, 1024, "prefs_get_string: leaking the value of %s: %s", name, result) > 0)
    NS_WARNING(warnMsg);
#endif
  return result;
}

GList *prefs_get_children_names(const char *name)
{
  LOG(("prefs get children name %s\n", name));
  purple_debug_error("prefs", "call to get_children_names, not implemented yet\n");

  return NULL;
}

void *prefs_add_observer(const char *name, gpointer data)
{
  LOG(("prefs add observer %s\n", name));

  prefsObserver *observer = new prefsObserver(data);
  nsCOMPtr<nsIPrefBranch2> prefs =
    do_GetService(NS_PREFSERVICE_CONTRACTID);
  NS_ENSURE_TRUE(prefs, NULL);

  DEFINE_mozName_from_name_with_default(name, NULL);
  prefs->AddObserver(mozName, observer, false);
  g_free(mozName);

  return (void *)observer;
}

void prefs_remove_observer(const char *name, void *observer)
{
  LOG(("remove observer @%x\n", observer));
  nsCOMPtr<nsIPrefBranch2> prefs =
    do_GetService(NS_PREFSERVICE_CONTRACTID);
  NS_ENSURE_TRUE(prefs, );

  DEFINE_mozName_from_name(name);
  prefs->RemoveObserver(mozName, (nsIObserver *)observer);
  g_free(mozName);
}

void prefs_save()
{
  LOG(("prefs save\n"));
  nsCOMPtr<nsIPrefService> prefs =
    do_GetService(NS_PREFSERVICE_CONTRACTID);
  NS_ENSURE_TRUE(prefs, );
  prefs->SavePrefFile(nsnull);
}

PurplePrefsUiOps prefs_uiops = {
  prefs_add_none,
  prefs_add_bool,
  prefs_add_int,
  prefs_add_string,

  prefs_set_bool,
  prefs_set_int,
  prefs_set_string,

  prefs_get_bool,
  prefs_get_int,
  prefs_get_string,

  prefs_get_type,
  prefs_get_children_names,

  prefs_exists,
  prefs_remove,

  prefs_save,

  prefs_add_observer,
  prefs_remove_observer,

  NULL, NULL, NULL, NULL
};

// Core ui ops (ui info)
static GHashTable *ui_info = NULL;
static nsCString *ui_name = NULL;
static nsCString *ui_version = NULL;

static void ui_quit()
{
  if (ui_info) {
    g_hash_table_destroy(ui_info);
    ui_info = NULL;
    delete ui_name;
    delete ui_version;
  }
}

static GHashTable *ui_get_info(void)
{
  if (!ui_info) {
    ui_info = g_hash_table_new(g_str_hash, g_str_equal);

    nsCOMPtr<nsIXULAppInfo> xai =
      do_GetService("@mozilla.org/xre/app-info;1");

    // Ugly, ugly, ugly...
    ui_name = new nsCString();
    if (xai && NS_SUCCEEDED(xai->GetName(*ui_name)))
      g_hash_table_insert(ui_info, const_cast<char *>("name"),
                          const_cast<char *>(ui_name->get()));
    else
      g_hash_table_insert(ui_info, const_cast<char *>("name"),
                          const_cast<char *>("Instantbird"));

    ui_version = new nsCString();
    if (xai && NS_SUCCEEDED(xai->GetVersion(*ui_version)))
      g_hash_table_insert(ui_info, const_cast<char *>("version"),
                          const_cast<char *>(ui_version->get()));
    else
      g_hash_table_insert(ui_info, const_cast<char *>("version"),
                          const_cast<char *>("Unknown"));

    g_hash_table_insert(ui_info, const_cast<char *>("website"),
                        const_cast<char *>("http://www.instantbird.com"));
    g_hash_table_insert(ui_info, const_cast<char *>("dev_website"),
                        const_cast<char *>("http://www.instantbird.org"));
  }

  return ui_info;
}

static void ui_register_plugins()
{
  LOG(("ui_register_plugins"));

  nsCOMPtr<nsICategoryManager> catMgr =
    do_GetService(NS_CATEGORYMANAGER_CONTRACTID);
  NS_ENSURE_TRUE(catMgr, );

  // Get contract IDs of all purple protocol plugin registrated as
  // XPCOM components and register them with libpurple
  nsCOMPtr<nsISimpleEnumerator> catEntries;
  nsresult rv = catMgr->EnumerateCategory(PURPLE_PROTOCOL_PLUGIN_CATEGORY,
                                          getter_AddRefs(catEntries));
  NS_ENSURE_SUCCESS(rv, );

  PRBool hasMore;
  while (NS_SUCCEEDED(catEntries->HasMoreElements(&hasMore)) && hasMore) {
    nsCOMPtr<nsISupports> elem;
    rv = catEntries->GetNext(getter_AddRefs(elem));
    NS_ENSURE_SUCCESS(rv, );

    nsCOMPtr<nsISupportsCString> entry = do_QueryInterface(elem, &rv);
    NS_ENSURE_SUCCESS(rv, );

    nsCString contractId;
    rv = entry->GetData(contractId);
    NS_ENSURE_SUCCESS(rv, );

    LOG(("Attempting to register %s", contractId.get()));
    nsCOMPtr<purpleIPlugin> plugin = do_CreateInstance(contractId.get());
    if (!plugin) {
      NS_WARNING("Failed attempt to register a protocol plugin");
      continue;
    }

    plugin->RegisterSelf();
  }
}

static PurpleCoreUiOps core_uiops =
{
  NULL, NULL, NULL,
  ui_quit,
  ui_get_info,
  ui_register_plugins,

  /* padding */
  NULL, NULL, NULL
};

nsresult
init_libpurple()
{
  /* Attempt to put libpurple symbols in the global namespace, and
     ignore failures... */
  open_libpurple();

  /* First initialize the translation stuff, it can probably be
     needed early be potential error messages */
  purple_gettext_set_ui_ops(&gettext_uiops);

  purple_prefs_set_ui_ops(&prefs_uiops);

  /* Set the instantbird profile folder as libpurple custom user
     directory */

  nsCOMPtr<nsIFile> folder;
  nsresult rv = NS_GetSpecialDirectory(NS_APP_USER_PROFILE_50_DIR,
                                       getter_AddRefs(folder));
  NS_ENSURE_SUCCESS(rv, rv);
  nsString path;
  rv = folder->GetPath(path);
  NS_ENSURE_SUCCESS(rv, rv);

#ifdef PR_LOGGING
  if (!gPurpleInitLog)
    gPurpleInitLog = PR_NewLogModule("purpleInit");
#endif

  nsCString cPath;
  CopyUTF16toUTF8(path, cPath);
  LOG(("Setting custom user directory to:  %s", cPath.get()));
  purple_util_set_user_dir(cPath.get());

  purple_core_set_ui_ops(&core_uiops);
  purple_debug_set_ui_ops(&debugconsoleuiops);
  purple_dnsquery_set_ui_ops(&dnsUiOps);

  /* The eventloop uiops use nsTimer and
   * nsSocketTransportService. purpleTimer and purpleSocket are proxy
   * so that we can have the right prototypes for libpurple.
   */
  purple_eventloop_set_ui_ops(&eventloops);

  /* Here "instantbird" stands for the UI_ID. libpurple
   * sometimes need this id (for example for some plugins and
   * some preferences)
   */
  gboolean purpleCoreInit = purple_core_init("instantbird");
  NS_ASSERTION(purpleCoreInit, "purple_core_init failed!!!");
  NS_ENSURE_TRUE(purpleCoreInit, NS_ERROR_FAILURE);

  purple_cmd_register("raw", "s", PURPLE_CMD_P_DEFAULT,
                      (PurpleCmdFlag) (PURPLE_CMD_FLAG_CHAT | PURPLE_CMD_FLAG_IM),
                      NULL, raw_command_cb,
                      "raw &lt;message&gt;:  Send a raw message (without escaping or handling of formatting).",
                      NULL);

#define REGISTER_STATUS_COMMAND(aName, aCallback, aDescription) \
  purple_cmd_register(#aName, "s", PURPLE_CMD_P_HIGH,\
                      (PurpleCmdFlag) (PURPLE_CMD_FLAG_CHAT | PURPLE_CMD_FLAG_IM | PURPLE_CMD_FLAG_ALLOW_WRONG_ARGS),\
                      NULL, aCallback, aDescription, NULL)

  REGISTER_STATUS_COMMAND(back, back_command_cb,
                          "back &lt;message&gt;:  Set the status to Available and use |message| as status message.");
  REGISTER_STATUS_COMMAND(away, away_command_cb,
                          "away &lt;message&gt;:  Set the status to Away and use |message| as status message.");
  REGISTER_STATUS_COMMAND(busy, busy_command_cb,
                          "busy &lt;message&gt;:  Set the status to Busy and use |message| as status message.");
  REGISTER_STATUS_COMMAND(dnd, busy_command_cb,
                          "dnd &lt;message&gt;:  Set the status to Busy and use |message| as status message.");
  REGISTER_STATUS_COMMAND(offline, offline_command_cb,
                          "offline:  Set the status to Offline.");

  /* Create the purple buddylist. Required before any account is created. */
  purple_set_blist(purple_blist_new());

  return NS_OK;
}
