/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "purpleCoreService.h"
#include "purpleAccountBuddy.h"
#include "purpleDNS.h"
#include "purpleGetText.h"
#include "purpleNetwork.h"
#include "purpleSockets.h"
#include "purpleTimer.h"
#include "imICommandsService.h"
#include "imIConversationsService.h"

#pragma GCC visibility push(default)
#include <libpurple/core.h>
#include <libpurple/idle.h>
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

#ifdef PR_LOGGING
#include <prprf.h>
//
// NSPR_LOG_MODULES=purpleCoreService:5
//
static PRLogModuleInfo *gPurpleCoreServiceLog = nullptr;
#endif
#define LOG(args) PR_LOG(gPurpleCoreServiceLog, PR_LOG_DEBUG, args)

NS_IMPL_CLASSINFO(purpleCoreService, NULL, nsIClassInfo::SINGLETON,
                  PURPLE_CORE_SERVICE_CID)
NS_IMPL_ISUPPORTS2_CI(purpleCoreService, purpleICoreService, nsIObserver)

extern nsresult init_libpurple();
extern void init_libpurple_accounts();
extern void init_libpurple_blist();
extern void init_libpurple_commands();
extern void init_libpurple_conversations();
extern void connect_to_blist_signals();
extern void disconnect_blist_signals();

purpleCoreService::purpleCoreService()
  :mInitialized(PR_FALSE),
   mQuitting(PR_FALSE)
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

  if (!strcmp("prpl-quit", aTopic))
    return Quit();

  if (!strcmp("idle-time-changed", aTopic)) {
    purple_idle_set(strtol(NS_ConvertUTF16toUTF8(aData).get(), NULL, 10));
    return NS_OK;
  }

  return NS_ERROR_UNEXPECTED;
}

/* boolean Init (); */
NS_IMETHODIMP purpleCoreService::Init()
{
  /* We shouldn't init the core twice, even though each purpleProtocol
     instantiation will call this method. */
  if (mInitialized)
    return NS_OK;

  /* Make sure we will uninitialize libpurple before all services are
     destroyed */
  nsresult rv;
  nsCOMPtr<nsIObserverService> os =
    do_GetService(NS_OBSERVERSERVICE_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = os->AddObserver(this, "prpl-quit", PR_FALSE);
  NS_ENSURE_SUCCESS(rv, rv);

  /* Ensure that NSS is initialized */
  nsCOMPtr<nsISupports> nssComponent
    = do_GetService("@mozilla.org/psm;1");
  NS_ASSERTION(nssComponent, "Failed to initialize NSS");

  purpleSocketWatcher::init();
  purpleNetworkObserver::init();
  purpleDNS::init();
  purpleTimer::init();

  init_libpurple_commands();

  /* Init the core of libpurple */
  rv = init_libpurple();
  NS_ENSURE_SUCCESS(rv, rv);

  /* Init libpurple subsystems */
  init_libpurple_conversations();

  /*Load the list of proxies. Should be done before loading the accounts */
  InitProxies();

  /* Set to true before the end of the initialization because
     InitAccounts calls GetProtoById that ensures the core is
     initialized */
  mInitialized = PR_TRUE;

  /* Set the ui ops only after the accounts are loaded to avoid useless
     set_* ops calls during the load of account specific preferences. */
  init_libpurple_accounts();

  init_libpurple_blist();

  os->AddObserver(this, "idle-time-changed", PR_FALSE);

  LOG(("Libpurple initialized"));
  return NS_OK;
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

  rv = os->RemoveObserver(this, "prpl-quit");
  NS_ENSURE_SUCCESS(rv, rv);

  os->RemoveObserver(this, "idle-time-changed");

  disconnect_blist_signals();

  PurpleBlistNode *gnode;
  for (gnode = purple_blist_get_root(); gnode; gnode = gnode->next) {
    if (!PURPLE_BLIST_NODE_IS_GROUP(gnode))
      continue;
    imITag *tag =
      purpleAccountBuddy::GetTagFromPurpleGroup(PURPLE_GROUP(gnode));
    NS_IF_RELEASE(tag);
  }

  mProxies.Clear();
  mInitialized = PR_FALSE;
  purple_core_quit();
  LOG(("purple_core_quit"));
  purpleSocketWatcher::unInit();
  purpleNetworkObserver::unInit();
  purpleDNS::unInit();
  purpleTimer::unInit();
  purpleGetText::unInit();

  mQuitting = PR_FALSE;
  return NS_OK;
}

/* readonly attribute AUTF8String version; */
NS_IMETHODIMP purpleCoreService::GetVersion(nsACString& aVersion)
{
  aVersion = purple_core_get_version();
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
  nsAutoCString key;
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
  bool found = false;
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
