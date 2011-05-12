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

#include "purpleCoreService.h"
#include "purpleAccountBuddy.h"
#include "purpleDNS.h"
#include "purpleGetText.h"
#include "purpleNetwork.h"
#include "purpleStorage.h"
#include "purpleSockets.h"
#include "purpleTimer.h"
#include "imICommandsService.h"
#include "imIConversationsService.h"

#pragma GCC visibility push(default)
#include <libpurple/core.h>
#include <libpurple/savedstatuses.h>
#pragma GCC visibility pop

#include <nsServiceManagerUtils.h>
#include <nsComponentManagerUtils.h>
#include <nsICategoryManager.h>
#include <nsIClassInfoImpl.h>
#include <nsIIOService2.h>
#include <nsIObserverService.h>
#include <nsIPrefService.h>
#include <nsISupportsPrimitives.h>
#include <nsIXULRuntime.h>
#include <nsCRTGlue.h>
#include <nsXPCOM.h>
#include <nsArrayEnumerator.h>
#include <nsNetUtil.h>
#include <nsAppDirectoryServiceDefs.h>
#include <nsDirectoryServiceUtils.h>

#ifdef MOZ_CRASHREPORTER
#include <nsICrashReporter.h>
#endif

#ifdef PR_LOGGING
#include <prprf.h>
//
// NSPR_LOG_MODULES=purpleCoreService:5
//
static PRLogModuleInfo *gPurpleCoreServiceLog = nsnull;
#endif
#define LOG(args) PR_LOG(gPurpleCoreServiceLog, PR_LOG_DEBUG, args)

#define PREF_STARTUP_ACTION     "messenger.startup.action"
#define PREF_REPORT_IDLE        "messenger.status.reportIdle"
#define PREF_USER_ICON_FILENAME "messenger.status.userIconFileName"
#define PREF_USER_DISPLAY_NAME  "messenger.status.userDisplayName"

#define QUIT_APPLICATION_GRANTED_NOTIFICATION "quit-application-granted"

NS_IMPL_CLASSINFO(purpleCoreService, NULL, nsIClassInfo::SINGLETON,
                  PURPLE_CORE_SERVICE_CID)
NS_IMPL_ISUPPORTS3_CI(purpleCoreService, purpleICoreService,
                      nsIObserver, purpleIAccountsService)

extern nsresult init_libpurple();
extern void init_libpurple_accounts();
extern void init_libpurple_blist();
extern void init_libpurple_commands();
extern void init_libpurple_conversations();
extern void connect_to_blist_signals();
extern void disconnect_blist_signals();

purpleCoreService::purpleCoreService()
  :mInitialized(PR_FALSE),
   mQuitting(PR_FALSE),
   mAutoLoginStatus(AUTOLOGIN_ENABLED),
   mProtocols(nsnull),
   mProxies(nsnull)
{
#ifdef PR_LOGGING
  if (!gPurpleCoreServiceLog)
    gPurpleCoreServiceLog = PR_NewLogModule("purpleCoreService");
#endif
  mPrefService = do_GetService(NS_PREFSERVICE_CONTRACTID);
  mPrefBranch2 = do_QueryInterface(mPrefService);
}

purpleCoreService::~purpleCoreService()
{
  if (mInitialized)
    Quit();
}

/* nsIObserver implementation */
NS_IMETHODIMP purpleCoreService::Observe(nsISupports *aSubject,
                                         const char *aTopic,
                                         const PRUnichar *aData)
{
  NS_ASSERTION(mInitialized, "Observing notification after uninitialization");

  if (!strcmp(QUIT_APPLICATION_GRANTED_NOTIFICATION, aTopic))
    return Quit();

  if (!strcmp(NS_PREFBRANCH_PREFCHANGE_TOPIC_ID, aTopic)) {
    if (NS_LITERAL_STRING(PREF_REPORT_IDLE).Equals(aData)) {
      PRBool reportIdle;
      nsresult rv = mPrefBranch2->GetBoolPref(PREF_REPORT_IDLE, &reportIdle);
      NS_ENSURE_SUCCESS(rv, rv);

      if (reportIdle && !mIdleObserver)
        return addIdleObserver();
      else if (!reportIdle && mIdleObserver)
        return removeIdleObserver();

      return NS_OK;
    }
    return NS_ERROR_UNEXPECTED;
  }

  if (!strcmp(NS_IOSERVICE_GOING_OFFLINE_TOPIC, aTopic)) {
    purple_savedstatus_set_offline(TRUE);
    return NotifyObservers(static_cast<purpleICoreService *>(this),
                           "status-changed");
  }

  if (!strcmp(NS_IOSERVICE_OFFLINE_STATUS_TOPIC, aTopic)) {
    if (nsDependentString(aData).EqualsLiteral(NS_IOSERVICE_ONLINE) &&
        purple_savedstatus_is_offline()) {
      purple_savedstatus_set_offline(FALSE);
      // Get the "new" current status message for the notification...
      const char *message =
        purple_savedstatus_get_message(purple_savedstatus_get_current());
      nsString wideMessage;
      NS_CStringToUTF16(nsDependentCString(message), NS_CSTRING_ENCODING_UTF8,
                        wideMessage);
      return NotifyObservers(static_cast<purpleICoreService *>(this),
                             "status-changed", wideMessage.get());
    }
    return NS_OK;
  }

  return NS_ERROR_UNEXPECTED;
}

/* boolean Init (); */
NS_IMETHODIMP purpleCoreService::Init()
{
  /* We shouldn't init the core twice. If the core service is already
     initialized, return with an error */
  NS_ENSURE_TRUE(!mInitialized, NS_ERROR_ALREADY_INITIALIZED);

  /* Make sure we will uninitialize libpurple before all services are
     destroyed */
  nsresult rv;
  nsCOMPtr<nsIObserverService> os =
    do_GetService(NS_OBSERVERSERVICE_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = os->AddObserver(this, QUIT_APPLICATION_GRANTED_NOTIFICATION, PR_FALSE);
  NS_ENSURE_SUCCESS(rv, rv);

  /* Ensure that NSS is initialized */
  nsCOMPtr<nsISupports> nssComponent
    = do_GetService("@mozilla.org/psm;1");
  NS_ASSERTION(nssComponent, "Failed to initialize NSS");

  purpleSocketWatcher::init();
  purpleNetworkObserver::init();
  purpleDNS::init();
  purpleTimer::init();

  nsCOMPtr<imICommandsService> commands =
    do_GetService(IM_COMMANDS_SERVICE_CONTRACTID);
  if (commands)
    commands->InitCommands();
  init_libpurple_commands();

  /* Init the core of libpurple */
  rv = init_libpurple();
  NS_ENSURE_SUCCESS(rv, rv);

  /* Init libpurple subsystems */
  init_libpurple_conversations();

  /* Load the list of protocol plugins and their specific options */
  InitProtocols();

  /*Load the list of proxies. Should be done before loading the accounts */
  InitProxies();

  /* Set to true before the end of the initialization because
     InitAccounts calls GetProtoById that ensures the core is
     initialized */
  mInitialized = PR_TRUE;

  /* Load saved accounts */
  mAccountsService = new purpleAccountsService();
  NS_ENSURE_TRUE(mAccountsService, NS_ERROR_OUT_OF_MEMORY);

  rv = mAccountsService->InitAccounts();
  NS_ENSURE_SUCCESS(rv, rv);

  /* Set the ui ops only after the accounts are loaded to avoid useless
     set_* ops calls during the load of account specific preferences. */
  init_libpurple_accounts();

  /* Load the buddy list from mozStorage */
  rv = InitBuddyList();
  NS_ENSURE_SUCCESS(rv, rv);

  /* Check if we should disable auto-login */
  InitAutoLoginStatus();

  /* If we are not offline, set the initial status to available */
  if (mAutoLoginStatus != AUTOLOGIN_START_OFFLINE) {
    nsCOMPtr<nsIIOService> IOService = do_GetIOService(&rv);
    NS_ENSURE_SUCCESS(rv, rv);
    PRBool offline = PR_TRUE;
    IOService->GetOffline(&offline);
    if (!offline)
      purple_savedstatus_set_offline(FALSE);
  }
  os->AddObserver(this, NS_IOSERVICE_GOING_OFFLINE_TOPIC, PR_FALSE);
  os->AddObserver(this, NS_IOSERVICE_OFFLINE_STATUS_TOPIC, PR_FALSE);

  /* Connect accounts that should be connected at startup */
  if (mAutoLoginStatus == AUTOLOGIN_ENABLED)
    ProcessAutoLogin();

  /* Initialize idle stuff */
  PRBool reportIdle;
  rv = mPrefBranch2->GetBoolPref(PREF_REPORT_IDLE, &reportIdle);
  NS_ENSURE_SUCCESS(rv, rv);
  if (reportIdle) {
    rv = addIdleObserver();
    NS_ENSURE_SUCCESS(rv, rv);
  }
  mPrefBranch2->AddObserver(PREF_REPORT_IDLE, this, PR_FALSE);
  NS_ENSURE_SUCCESS(rv, rv);

  LOG(("Libpurple initialized"));
  return NS_OK;
}

void purpleCoreService::InitAutoLoginStatus()
{
  /* If auto-login is already disabled, do nothing */
  if (mAutoLoginStatus != AUTOLOGIN_ENABLED)
    return;

  PRInt32 startupAction;
  nsresult rv = mPrefService->GetIntPref(PREF_STARTUP_ACTION, &startupAction);
  if (NS_SUCCEEDED(rv) && !startupAction) {
    // the value 0 means that we start without connecting the accounts
    mAutoLoginStatus = AUTOLOGIN_USER_DISABLED;
    return;
  }

  /* Disable auto-login if we are running in safe mode */
  nsCOMPtr<nsIXULRuntime> xai =
    do_GetService("@mozilla.org/xre/app-info;1");
  PRBool inSafeMode;
  if (xai && NS_SUCCEEDED(xai->GetInSafeMode(&inSafeMode)) && inSafeMode) {
    mAutoLoginStatus = AUTOLOGIN_SAFE_MODE;
    return;
  }

  /* If the application was started offline, disable auto-login */
  nsCOMPtr<nsIIOService2> io =
    do_GetService("@mozilla.org/network/io-service;1");
  NS_ENSURE_TRUE(io, );
  PRBool offlineStatusManaged = PR_TRUE;
  io->GetManageOfflineStatus(&offlineStatusManaged);
  if (!offlineStatusManaged) {
    PRBool offlineState = PR_FALSE;
    io->GetOffline(&offlineState);
    if (offlineState) {
      mAutoLoginStatus = AUTOLOGIN_START_OFFLINE;
      return;
    }
  }

  /* Check if we crashed at the last startup during autologin */
  PRInt32 autoLoginPending;
  rv = mPrefService->GetIntPref(PREF_AUTOLOGIN_PENDING, &autoLoginPending);
  if (NS_FAILED(rv) || !autoLoginPending) {
    // if the pref isn't set, then we haven't crashed: keep autologin enabled
    return;
  }

  // Last autologin hasn't finished properly.
  // For now, assume it's because of a crash
  mAutoLoginStatus = AUTOLOGIN_CRASH;
  mPrefService->DeleteBranch(PREF_AUTOLOGIN_PENDING);

#ifdef MOZ_CRASHREPORTER
  // Try to get more info with breakpad
  PRInt32 lastCrashTime = 0;
  // NS_ERROR_FILE_NOT_FOUND is totally acceptable, it just means that
  // either we never crashed or breakpad is not enabled
  // In this case, lastCrashTime will keep its 0 initialization value
  rv = GetLastCrashTime(&lastCrashTime);
  if (NS_FAILED(rv) && rv != NS_ERROR_FILE_NOT_FOUND) {
    // if we failed to get the last crash time, then keep the
    // AUTOLOGIN_CRASH value in mAutoLoginStatus and return
    return;
  }

  LOG(("autoLoginPending = %i, lastCrash = %i, difference = %i\n",
       autoLoginPending, lastCrashTime, lastCrashTime - autoLoginPending));

  if (lastCrashTime < autoLoginPending) {
    // the last crash caught by breakpad is older than our last autologin
    // attempt.
    // If breakpad is currently enabled, we can be confident that
    // autologin was interrupted for an exterior reason
    // (application killed by the user, power outage, ...)
    nsCOMPtr<nsICrashReporter> cr = do_GetService("@mozilla.org/xre/app-info;1");
    if (cr) {
      // This should fail with NS_ERROR_INVALID_ARG if breakpad is enabled,
      // and NS_ERROR_NOT_INITIALIZED if it is not.
      rv = cr->AnnotateCrashReport(NS_LITERAL_CSTRING("="), EmptyCString());
      LOG(("Breadpad is%s enabled", rv == NS_ERROR_NOT_INITIALIZED ? " not" : ""));
      if (rv != NS_ERROR_NOT_INITIALIZED) {
        // crash report is enabled, re-activate autologin
        LOG(("autologin failed from an exterior cause, don't disable it"));
        mAutoLoginStatus = AUTOLOGIN_ENABLED;
      }
    }
  }
#endif
}

#ifdef MOZ_CRASHREPORTER
nsresult purpleCoreService::GetLastCrashTime(PRInt32 *aLastCrashTime)
{
  /* Look for the LastCrash file */
  nsCOMPtr<nsIFile> lastCrash;
  nsresult rv = NS_GetSpecialDirectory(NS_APP_USER_PROFILES_ROOT_DIR,
                                       getter_AddRefs(lastCrash));
  NS_ENSURE_SUCCESS(rv, rv);

#if defined(XP_MACOSX) || defined(XP_OS2) || defined(XP_WIN)
  rv = lastCrash->SetNativeLeafName(NS_LITERAL_CSTRING("Crash Reports"));
#else
  rv = lastCrash->AppendNative(NS_LITERAL_CSTRING("Crash Reports"));
#endif
  NS_ENSURE_SUCCESS(rv, rv);

  PRBool exists;
  rv = lastCrash->Exists(&exists);
  NS_ENSURE_SUCCESS(rv, rv);
  if (!exists)
    return NS_ERROR_FILE_NOT_FOUND;

  rv = lastCrash->AppendNative(NS_LITERAL_CSTRING("LastCrash"));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = lastCrash->Exists(&exists);
  NS_ENSURE_SUCCESS(rv, rv);
  if (!exists)
    return NS_ERROR_FILE_NOT_FOUND;

  /* Ok, the file exists, now let's try to read it */
  nsCOMPtr<nsILocalFile> localFile = do_QueryInterface(lastCrash);
  NS_ENSURE_TRUE(localFile, NS_ERROR_FAILURE);

  PRFileDesc* fd;
  rv = localFile->OpenNSPRFileDesc(PR_RDONLY, 0, &fd);
  // this would be NS_ERROR_FILE_NOT_FOUND but we already checked before
  // that the file exists, so at this point an error is unexpected
  NS_ENSURE_SUCCESS(rv, NS_ERROR_UNEXPECTED);

  PRInt32 filesize = PR_Available(fd);
  if (filesize <= 0) {
    PR_Close(fd);
    return NS_ERROR_UNEXPECTED;
  }

  nsCString data;
  data.SetLength(filesize);
  if (PR_Read(fd, data.BeginWriting(), filesize) == -1) {
    PR_Close(fd);
    return NS_ERROR_UNEXPECTED;
  }

  LOG(("Last Crash Time found: %s\n", data.get()));
  PR_Close(fd);
  *aLastCrashTime = strtol(data.get(), NULL, 10);
  return NS_OK;
}
#endif

nsresult purpleCoreService::InitBuddyList()
{
  nsCOMPtr<imIContactsService> contacts =
    do_GetService("@instantbird.org/purple/contacts-service;1");
  NS_ENSURE_TRUE(contacts, NS_ERROR_UNEXPECTED);

  nsresult rv = contacts->InitContacts();
  NS_ENSURE_SUCCESS(rv, rv);

  init_libpurple_blist();
  return NS_OK;
}

void purpleCoreService::InitProtocols()
{
  LOG(("Start init protocols"));

  for (GList *iter = purple_plugins_get_protocols();
       iter; iter = iter->next) {
    PurplePlugin *plugin = (PurplePlugin *) iter->data;
    PurplePluginInfo *info = plugin->info;
    if (info && info->name) {
      LOG(("Trying to create %s :\t (%s)", info->name, info->id));
      nsCOMPtr<purpleProtocol> proto = do_CreateInstance(PURPLE_PROTOCOL_CONTRACTID);
      proto->Init(info);
      mProtocols.AppendObject(proto);
    }
  }

  LOG(("Init JS protocols"));
  nsCOMPtr<nsICategoryManager> catMgr =
    do_GetService(NS_CATEGORYMANAGER_CONTRACTID);
  NS_ENSURE_TRUE(catMgr, );

  nsCOMPtr<nsISimpleEnumerator> catEntries;
  nsresult rv = catMgr->EnumerateCategory(JS_PROTOCOL_PLUGIN_CATEGORY,
                                          getter_AddRefs(catEntries));
  NS_ENSURE_SUCCESS(rv, );

  PRBool hasMore;
  while (NS_SUCCEEDED(catEntries->HasMoreElements(&hasMore)) && hasMore) {
    nsCOMPtr<nsISupports> elem;
    rv = catEntries->GetNext(getter_AddRefs(elem));
    NS_ENSURE_SUCCESS(rv, );

    nsCOMPtr<nsISupportsCString> entry = do_QueryInterface(elem, &rv);
    NS_ENSURE_SUCCESS(rv, );

    nsCString entryName;
    rv = entry->GetData(entryName);
    NS_ENSURE_SUCCESS(rv, );

    nsCString contractId;
    rv = catMgr->GetCategoryEntry(JS_PROTOCOL_PLUGIN_CATEGORY,
                                  entryName.get(), getter_Copies(contractId));
    NS_ENSURE_SUCCESS(rv, );

    LOG(("Attempting to register %s", contractId.get()));
    nsCOMPtr<purpleIProtocol> proto = do_CreateInstance(contractId.get());
    if (!proto) {
      NS_WARNING("Failed attempt to register a protocol component");
      continue;
    }

    mProtocols.AppendObject(proto);
  }
  LOG(("End init protocols"));
}

/* void Quit (); */
NS_IMETHODIMP purpleCoreService::Quit()
{
  // This seems to happen when Windows is shutting down, don't know why... :(
  if (!mInitialized)
    return NS_OK;

  // Avoid reentering when called from the unload handler of the buddy list
  if (mQuitting)
    return NS_OK;

  mQuitting = PR_TRUE;

  nsresult rv;
  nsCOMPtr<nsIObserverService> os =
    do_GetService(NS_OBSERVERSERVICE_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = os->RemoveObserver(this, QUIT_APPLICATION_GRANTED_NOTIFICATION);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = os->RemoveObserver(this, NS_IOSERVICE_GOING_OFFLINE_TOPIC);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = os->RemoveObserver(this, NS_IOSERVICE_OFFLINE_STATUS_TOPIC);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = mPrefBranch2->RemoveObserver(PREF_REPORT_IDLE, this);
  NS_ENSURE_SUCCESS(rv, rv);

  if (mIdleObserver) {
    rv = removeIdleObserver();
    NS_ENSURE_SUCCESS(rv, rv);
  }

  os->NotifyObservers(static_cast<purpleICoreService *>(this),
                      "purple-quit", nsnull);

  nsCOMPtr<imIConversationsService> conversations =
    do_GetService("@instantbird.org/purple/conversations-service;1");
  if (conversations)
    conversations->UnInitConversations();

  disconnect_blist_signals();

  // Prevent the cached account icons from being uncached while
  // uninitializing the accounts.
  gboolean caching = purple_buddy_icons_is_caching();
  purple_buddy_icons_set_caching(FALSE);
  mAccountsService->UnInitAccounts();
  purple_buddy_icons_set_caching(caching);

  mAccountsService = nsnull;
  nsCOMPtr<imIContactsService> contacts =
    do_GetService("@instantbird.org/purple/contacts-service;1");
  if (contacts)
    contacts->UnInitContacts();

  PurpleBlistNode *gnode;
  for (gnode = purple_blist_get_root(); gnode; gnode = gnode->next) {
    if (!PURPLE_BLIST_NODE_IS_GROUP(gnode))
      continue;
    imITag *tag =
      purpleAccountBuddy::GetTagFromPurpleGroup(PURPLE_GROUP(gnode));
    NS_IF_RELEASE(tag);
  }

  mProtocols.Clear();
  mProxies.Clear();
  mInitialized = PR_FALSE;
  purple_core_quit();
  LOG(("purple_core_quit"));
  purpleStorage::unInit();
  purpleSocketWatcher::unInit();
  purpleNetworkObserver::unInit();
  purpleDNS::unInit();
  purpleTimer::unInit();
  purpleGetText::unInit();

  nsCOMPtr<imICommandsService> commands =
    do_GetService(IM_COMMANDS_SERVICE_CONTRACTID);
  if (commands)
    commands->UnInitCommands();

  mQuitting = PR_FALSE;
  return NS_OK;
}

/* readonly attribute AUTF8String version; */
NS_IMETHODIMP purpleCoreService::GetVersion(nsACString& aVersion)
{
  aVersion = purple_core_get_version();
  return NS_OK;
}

/* nsISimpleEnumerator getProtocols (); */
NS_IMETHODIMP purpleCoreService::GetProtocols(nsISimpleEnumerator **aResult)
{
  PURPLE_ENSURE_INIT(mInitialized);

  return NS_NewArrayEnumerator(aResult, mProtocols);
}

/* purpleIProtocol getProtocolById (in AUTF8String aProtocolId); */
NS_IMETHODIMP purpleCoreService::GetProtocolById(const nsACString& aProtocolId, purpleIProtocol **aResult)
{
  PURPLE_ENSURE_INIT(mProtocols.Count());

  for (PRInt32 i = mProtocols.Count() - 1; i >= 0; --i) {
    nsCString id;
    nsresult rv = mProtocols[i]->GetId(id);
    NS_ENSURE_SUCCESS(rv, rv);
    if (aProtocolId.Equals(id)) {
      NS_ADDREF(*aResult = mProtocols[i]);
      return NS_OK;
    }
  }

  *aResult = nsnull;
  return NS_OK;
}

nsresult purpleCoreService::InitProxies()
{
  /* Get the list of proxies */
  nsCString proxyList;
  nsresult rv = mPrefService->GetCharPref(PREF_MESSENGER_PROXIES,
                                          getter_Copies(proxyList));

  LOG(("InitProxies list = %s", proxyList.get()));

  nsCOMPtr<purpleProxy> proxy;
  char *newStr = proxyList.BeginWriting();
  nsCAutoString key;
  for (char *token = NS_strtok(",", &newStr);
       token;
       token = NS_strtok(",", &newStr)) {
    key = token;
    key.StripWhitespace();

    if (key.IsEmpty()) {
      continue;
    }

    LOG(("Creating proxy %s", key.get()));

    /* create the purpleProxy object */
    proxy = do_CreateInstance(PURPLE_PROXY_CONTRACTID);
    NS_ENSURE_TRUE(proxy, NS_ERROR_OUT_OF_MEMORY);

    LOG(("Creating proxy %s", key.get()));

    rv = proxy->Init(key);
    if (NS_FAILED(rv)) {
      NS_WARNING("Failed to init proxy!");
      continue;
    }

    /* Keep it in the local proxy list */
    mProxies.AppendObject(proxy);
    LOG(("Adding proxy %s in the list", key.get()));
  }

  /* Read the pref indicating the global proxy settings */
  rv = mPrefService->GetCharPref(PREF_MESSENGER_GLOBAL_PROXY,
                                 getter_Copies(key));
  NS_ENSURE_SUCCESS(rv, rv);

  /* Init mGlobalProxy */
  if (StringBeginsWith(key, NS_LITERAL_CSTRING(PROXY_KEY))) {
    for (PRInt32 i = mProxies.Count() - 1; i >= 0; --i) {
      if (mProxies[i]->GetKey().Equals(key)) {
        mGlobalProxy = mProxies[i];
        break;
      }
    }
  }

  if (!mGlobalProxy) {
    mGlobalProxy = do_CreateInstance(PURPLE_PROXY_INFO_CONTRACTID);
    NS_ENSURE_TRUE(mGlobalProxy, NS_ERROR_OUT_OF_MEMORY);

    if (key.Equals(PROXY_KEY_ENVVAR))
      mGlobalProxy->SetType(purpleIProxyInfo::useEnvVar);
    else
      mGlobalProxy->SetType(purpleIProxyInfo::noProxy);
  }

  /* Give the information to libpurple */
  PurpleProxyInfo *info;
  rv = mGlobalProxy->GetPurpleProxy(&info);
  NS_ENSURE_SUCCESS(rv, rv);
  purple_global_proxy_set_info(info);

  return NS_OK;
}

/* purpleIProxy createProxy (in short aType, in AUTF8String aHost,
                             in unsigned long aPort, in AUTF8String aUsername,
                             in AUTF8String aPassword); */
NS_IMETHODIMP purpleCoreService::CreateProxy(PRInt16 aType, const nsACString & aHost,
                                             PRUint32 aPort, const nsACString & aUsername,
                                             const nsACString & aPassword,
                                             purpleIProxy **aResult)
{
  PURPLE_ENSURE_INIT(mInitialized);

  /* First check that we don't already have an identical proxy */
  for (PRInt32 j = mProxies.Count() - 1; j >= 0; --j) {
    if ((mProxies[j]->Equals(aType, aHost, aPort, aUsername, aPassword))) {
      NS_ADDREF(*aResult = mProxies[j]);
      return NS_OK;
    }
  }

  /* Get the list of proxies */
  nsCString proxyList;
  nsresult rv = mPrefService->GetCharPref(PREF_MESSENGER_PROXIES,
                                          getter_Copies(proxyList));
  NS_ENSURE_SUCCESS(rv, rv);

  /* Get a new unique proxy key */
  nsCString key;
  PRInt32 i = 1;
  PRBool found = PR_FALSE;
  do {
    key = PROXY_KEY;
    key.AppendInt(i);
    for (PRInt32 j = mProxies.Count() - 1; j >= 0; --j) {
      if ((found = mProxies[j]->GetKey().Equals(key))) {
        break;
      }
    }
    ++i;
  } while (found);

  /* Actually create the new proxy */
  nsCOMPtr<purpleProxy> proxy = do_CreateInstance(PURPLE_PROXY_CONTRACTID);
  rv = proxy->Init(key, aType, aHost, aPort, aUsername, aPassword);
  NS_ENSURE_SUCCESS(rv, rv);

  /* Save the proxy list pref */
  if (proxyList.IsEmpty())
    proxyList = key;
  else {
    proxyList.Append(',');
    proxyList.Append(key);
  }
  mPrefService->SetCharPref(PREF_MESSENGER_PROXIES, proxyList.get());

  /* Keep it in the local proxy list */
  mProxies.AppendObject(proxy);

  /* And return the new proxy as result */
  NS_ADDREF(*aResult = proxy);
  return NS_OK;
}

/* nsISimpleEnumerator getProxies (); */
NS_IMETHODIMP purpleCoreService::GetProxies(nsISimpleEnumerator **aResult)
{
  PURPLE_ENSURE_INIT(mInitialized);

  return NS_NewArrayEnumerator(aResult, mProxies);
}

/* attribute purpleIProxyInfo globalProxy; */
NS_IMETHODIMP purpleCoreService::GetGlobalProxy(purpleIProxyInfo * *aGlobalProxy)
{
  PURPLE_ENSURE_INIT(mInitialized);

  NS_ADDREF(*aGlobalProxy = mGlobalProxy);
  return NS_OK;
}
NS_IMETHODIMP purpleCoreService::SetGlobalProxy(purpleIProxyInfo * aGlobalProxy)
{
  PURPLE_ENSURE_INIT(mInitialized);
  NS_ENSURE_ARG_POINTER(aGlobalProxy);

  mGlobalProxy = aGlobalProxy;

  // Save the pref
  nsCString key;
  nsresult rv = mGlobalProxy->GetKey(key);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = mPrefService->SetCharPref(PREF_MESSENGER_GLOBAL_PROXY, key.get());
  NS_ENSURE_SUCCESS(rv, rv);

  // Give it to libpurple;
  PurpleProxyInfo *info;
  rv = mGlobalProxy->GetPurpleProxy(&info);
  NS_ENSURE_SUCCESS(rv, rv);

  purple_global_proxy_set_info(info);

  return NS_OK;
}

/* attribute short autoLoginStatus; */
NS_IMETHODIMP purpleCoreService::GetAutoLoginStatus(PRInt16 *aAutoLoginStatus)
{
  *aAutoLoginStatus = mAutoLoginStatus;
  return NS_OK;
}
NS_IMETHODIMP purpleCoreService::SetAutoLoginStatus(PRInt16 aAutoLoginStatus)
{
  NS_ENSURE_TRUE(!mInitialized, NS_ERROR_ALREADY_INITIALIZED);

  mAutoLoginStatus = aAutoLoginStatus;
  return NS_OK;
}

/* void processAutoLogin (); */
NS_IMETHODIMP purpleCoreService::ProcessAutoLogin()
{
#ifndef DEBUG
  NS_ENSURE_TRUE(!NS_IsOffline(), NS_ERROR_FAILURE);
#endif

  // See if we need to remove an "offline" libpurple status.
  if (mAutoLoginStatus == AUTOLOGIN_START_OFFLINE) {
    PRInt16 currentStatus;
    nsresult rv = GetCurrentStatusType(&currentStatus);
    NS_ENSURE_SUCCESS(rv, rv);
    if (currentStatus == imIStatusInfo::STATUS_OFFLINE) {
      nsresult rv = SetStatus(PURPLE_STATUS_AVAILABLE, EmptyCString());
      NS_ENSURE_SUCCESS(rv, rv);
    }
  }

  nsCOMPtr<nsISimpleEnumerator> accounts;
  nsresult rv = mAccountsService->GetAccounts(getter_AddRefs(accounts));
  NS_ENSURE_SUCCESS(rv, rv);
  PRBool hasMore;
  while (NS_SUCCEEDED(accounts->HasMoreElements(&hasMore)) && hasMore) {
    nsCOMPtr<nsISupports> elem;
    rv = accounts->GetNext(getter_AddRefs(elem));
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<purpleIAccount> account = do_QueryInterface(elem, &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    account->CheckAutoLogin();
  }

  // Make sure autologin is now enabled, so that we don't display a
  // message stating that it is disabled and asking the user if it
  // should be processed now.
  mAutoLoginStatus = AUTOLOGIN_ENABLED;

  // Notify observers so that any message stating that autologin is
  // disabled can be removed
  NotifyObservers(static_cast<purpleICoreService *>(this),
                  "autologin-processed");

  return NS_OK;
}


nsresult purpleCoreService::NotifyObservers(nsISupports *aObj,
                                            const char *aEvent,
                                            const PRUnichar *aData)
{
  nsresult rv;
  nsCOMPtr<nsIObserverService> os =
    do_GetService(NS_OBSERVERSERVICE_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  LOG(("Notifying observers of %s", aEvent));
  return os->NotifyObservers(aObj, aEvent, aData);
}

/* readonly attribute mozIStorageConnection storageConnection; */
NS_IMETHODIMP purpleCoreService::GetStorageConnection(mozIStorageConnection * *aStorageConnection)
{
  purpleStorage *storageInstance = purpleStorage::GetInstance();
  NS_ENSURE_TRUE(storageInstance, NS_ERROR_OUT_OF_MEMORY);

  NS_ADDREF(*aStorageConnection = storageInstance->GetConnection());
  return NS_OK;
}

/* readonly attribute AUTF8String currentStatusMessage; */
NS_IMETHODIMP purpleCoreService::GetCurrentStatusMessage(nsACString & aCurrentStatusMessage)
{
  aCurrentStatusMessage =
    purple_savedstatus_get_message(purple_savedstatus_get_current());
  return NS_OK;
}

/* readonly attribute short currentStatusType; */
NS_IMETHODIMP purpleCoreService::GetCurrentStatusType(PRInt16 *aCurrentStatus)
{
  PurpleSavedStatus *status = purple_savedstatus_get_current();
  NS_ASSERTION(status, "purple_savedstatus_get_current should never return NULL");
  PurpleStatusPrimitive type = purple_savedstatus_get_type(status);
  if (purple_savedstatus_is_idleaway() && type != PURPLE_STATUS_OFFLINE) {
    *aCurrentStatus = imIStatusInfo::STATUS_IDLE;
    return NS_OK;
  }

#define MAP_PURPLE_STATUS(aStatus)                       \
  case PURPLE_STATUS_##aStatus:                          \
    *aCurrentStatus = imIStatusInfo::STATUS_##aStatus;   \
    break

  switch (type) {
    MAP_PURPLE_STATUS(OFFLINE);
    MAP_PURPLE_STATUS(UNAVAILABLE);
    MAP_PURPLE_STATUS(AVAILABLE);
    MAP_PURPLE_STATUS(AWAY);
    MAP_PURPLE_STATUS(INVISIBLE);
    //FIXME more types?
    default:
      return NS_ERROR_UNEXPECTED;
  }
#undef MAP_PURPLE_STATUS
  return NS_OK;
}

nsresult purpleCoreService::SetStatus(PurpleStatusPrimitive aStatus,
                                      const nsACString &aMessage)
{
  PurpleSavedStatus *status = purple_savedstatus_get_default();
  NS_ENSURE_TRUE(status, NS_ERROR_NOT_INITIALIZED);

  if (aStatus != PURPLE_STATUS_UNSET)
    purple_savedstatus_set_type(status, aStatus);
  if (aStatus != PURPLE_STATUS_OFFLINE)
    purple_savedstatus_set_message(status, PromiseFlatCString(aMessage).get());
  purple_savedstatus_activate(status);

  nsString message;
  NS_CStringToUTF16(aMessage, NS_CSTRING_ENCODING_UTF8, message);
  return NotifyObservers(static_cast<purpleICoreService *>(this),
                         "status-changed", message.get());
}

/* void setStatus (in short aStatus, in AUTF8String aMessage); */
NS_IMETHODIMP purpleCoreService::SetStatus(PRInt16 aStatus, const nsACString & aMessage)
{
  PurpleStatusPrimitive status;
#define STATUS_UNSET STATUS_UNKNOWN
#define MAP_PURPLE_STATUS(aStatus)                \
    case imIStatusInfo::STATUS_##aStatus:         \
      status = PURPLE_STATUS_##aStatus;           \
      break

  switch (aStatus) {
    MAP_PURPLE_STATUS(OFFLINE);
    MAP_PURPLE_STATUS(UNAVAILABLE);
    MAP_PURPLE_STATUS(AVAILABLE);
    MAP_PURPLE_STATUS(AWAY);
    MAP_PURPLE_STATUS(UNSET);
    MAP_PURPLE_STATUS(INVISIBLE);

    default:
      return NS_ERROR_INVALID_ARG;
  }
#undef MAP_PURPLE_STATUS

  return SetStatus(status, aMessage);
}

/* void setUserIcon (in nsILocalFile aIconFile); */
NS_IMETHODIMP purpleCoreService::SetUserIcon(nsILocalFile *aIconFile)
{
  // Get the profile folder
  nsCOMPtr<nsIFile> folder;
  nsresult rv = NS_GetSpecialDirectory(NS_APP_USER_PROFILE_50_DIR,
                                       getter_AddRefs(folder));
  NS_ENSURE_SUCCESS(rv, rv);

  nsString newName;
  if (aIconFile) {
    // newName = userIcon-<timestamp(now)>.<aIconFile.extension>
    newName.AssignLiteral("userIcon-");
    newName.AppendInt(PR_Now() / PR_USEC_PER_SEC);

    nsString leafName;
    rv = aIconFile->GetLeafName(leafName);
    NS_ENSURE_SUCCESS(rv, rv);

    // Remove trailing dots (invalid Windows extension)
    leafName.Trim(".", PR_FALSE);
    PRInt32 extPos = leafName.RFindChar(PRUnichar('.'));
    newName.Append(StringTail(leafName, leafName.Length() - extPos));

    rv = aIconFile->CopyTo(folder, newName);
    NS_ENSURE_SUCCESS(rv, rv);

    LOG(("SetUserIcon: copied new icon to %s",
         NS_ConvertUTF16toUTF8(newName).get()));
  }

  // Get the previous file name
  nsCString oldFileName;
  rv = mPrefService->GetCharPref(PREF_USER_ICON_FILENAME,
                                 getter_Copies(oldFileName));
  NS_ENSURE_SUCCESS(rv, rv);

  // Save the new file name
  rv = mPrefService->SetCharPref(PREF_USER_ICON_FILENAME,
                                 NS_ConvertUTF16toUTF8(newName).get());
  NS_ENSURE_SUCCESS(rv, rv);

  // Now that the new icon has been copied to the profile directory
  // and the pref value changed, we can remove the old icon. Ignore
  // failures so that we always fire the user-icon-changed notification.
  if (!oldFileName.IsEmpty()) {
    folder->Append(NS_ConvertUTF8toUTF16(oldFileName));

    PRBool exists;
    if (NS_FAILED(folder->Exists(&exists)) || !exists)
      NS_WARNING("The old icon file doesn't exist!");
    else {
      rv = folder->Remove(PR_FALSE);
      if (NS_FAILED(rv))
        NS_WARNING("Failed to remove the old icon");
      else
        LOG(("SetUserIcon: removed %s", oldFileName.get()));
    }
  }

  return NotifyObservers(static_cast<purpleICoreService *>(this),
                         "user-icon-changed", newName.get());
}

/* nsIFileURL getUserIcon (); */
NS_IMETHODIMP purpleCoreService::GetUserIcon(nsIFileURL **aResult)
{
  nsCString fileName;
  nsresult rv = mPrefService->GetCharPref(PREF_USER_ICON_FILENAME,
                                          getter_Copies(fileName));
  NS_ENSURE_SUCCESS(rv, rv);

  if (fileName.IsEmpty()) {
    // No icon has been set.
    *aResult = nsnull;
    return NS_OK;
  }

  // Get the profile folder
  nsCOMPtr<nsIFile> folder;
  rv = NS_GetSpecialDirectory(NS_APP_USER_PROFILE_50_DIR,
                              getter_AddRefs(folder));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = folder->Append(NS_ConvertUTF8toUTF16(fileName));
  NS_ENSURE_SUCCESS(rv, rv);

  PRBool exists;
  rv = folder->Exists(&exists);
  NS_ENSURE_SUCCESS(rv, rv);

  if (!exists) {
    NS_WARNING("Invalid userIconFileName preference");
    *aResult = nsnull;
    return NS_OK;
  }

  nsCOMPtr<nsIURI> fileURI;
  rv = NS_NewFileURI(getter_AddRefs(fileURI), folder);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIFileURL> fileURL = do_QueryInterface(fileURI, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  fileURL.swap(*aResult);
  return NS_OK;
}

/* attribute AUTF8String userDisplayName; */
NS_IMETHODIMP purpleCoreService::GetUserDisplayName(nsACString & aUserDisplayName)
{
  NS_ENSURE_TRUE(mPrefService, NS_ERROR_NOT_INITIALIZED);

  nsCString displayName;
  nsresult rv = mPrefService->GetCharPref(PREF_USER_DISPLAY_NAME,
                                          getter_Copies(displayName));
  NS_ENSURE_SUCCESS(rv, rv);

  aUserDisplayName = displayName;
  return NS_OK;
}
NS_IMETHODIMP purpleCoreService::SetUserDisplayName(const nsACString & aUserDisplayName)
{
  NS_ENSURE_TRUE(mPrefService, NS_ERROR_NOT_INITIALIZED);

  nsresult rv = mPrefService->SetCharPref(PREF_USER_DISPLAY_NAME,
                                          PromiseFlatCString(aUserDisplayName).get());
  NS_ENSURE_SUCCESS(rv, rv);

  return NotifyObservers(static_cast<purpleICoreService *>(this),
                         "user-display-name-changed",
                         NS_ConvertUTF8toUTF16(aUserDisplayName).get());
}

nsresult purpleCoreService::addIdleObserver()
{
  NS_ENSURE_TRUE(!mIdleObserver, NS_ERROR_ALREADY_INITIALIZED);

  mIdleObserver = new purpleIdleObserver();
  return NS_OK;
}

nsresult purpleCoreService::removeIdleObserver()
{
  NS_ENSURE_TRUE(mIdleObserver, NS_ERROR_NOT_INITIALIZED);

  mIdleObserver->unInit();
  mIdleObserver = nsnull;
  return NS_OK;
}
