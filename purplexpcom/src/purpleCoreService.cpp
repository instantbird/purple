#include "purpleCoreService.h"
#include "purpleProtocol.h"
#include "nsXPCOM.h"
#include <glib.h>
#include <libpurple/core.h>
#include <libpurple/account.h>

#include<nsServiceManagerUtils.h>
#include<nsComponentManagerUtils.h>
#include<nsIObserverService.h>
#include<nsCOMPtr.h>
#include<stdio.h>

#include<nsArrayEnumerator.h>

//#define DEBUG_PURPLE_CORE_SERVICE

NS_IMPL_ISUPPORTS1(purpleCoreService, purpleICoreService)

extern nsresult init_libpurple();

purpleCoreService::purpleCoreService()
  :mInitialized(PR_FALSE),
   mConversations(nsnull),
   mProtocols(nsnull)
{
}

purpleCoreService::~purpleCoreService()
{
  this->Quit();
}

/* boolean Init (); */
NS_IMETHODIMP purpleCoreService::Init()
{
  if (mInitialized) {
    return NS_ERROR_ALREADY_INITIALIZED;
  }

  nsresult rv = init_libpurple();
  NS_ENSURE_SUCCESS(rv, rv);

  mConversations = do_CreateInstance(NS_ARRAY_CONTRACTID);

  InitProtocols();
  //FIXME InitAccounts();

  mInitialized = PR_TRUE;

#ifdef DEBUG_PURPLE_CORE_SERVICE
  printf("libpurple initialized\n");
#endif

  return NS_OK;
}

void purpleCoreService::InitProtocols()
{
#ifdef DEBUG_PURPLE_CORE_SERVICE
  printf("start init protocols\n");

#if 1
  //this code demonstrate how to enumerate protocols from libpurple
  GList *iter = purple_plugins_get_protocols();
  for (int i = 0; iter; iter = iter->next) {
    PurplePlugin *plugin = (PurplePlugin *) iter->data;
    PurplePluginInfo *info = plugin->info;
    if (info && info->name) {
      printf("\t%d: %s (%s)\n", i++, info->name, info->id);
    }
  }
#endif

#endif
  for (GList *iter = purple_plugins_get_protocols();
       iter; iter = iter->next) {
    PurplePlugin *plugin = (PurplePlugin *) iter->data;
    PurplePluginInfo *info = plugin->info;
    if (info && info->name) {
#ifdef DEBUG_PURPLE_CORE_SERVICE
      printf("trying to create %s :\t (%s)\n", info->name, info->id);
#endif
      nsCOMPtr<purpleProtocol> proto = do_CreateInstance(PURPLE_PROTOCOL_CONTRACTID);
      proto->Init(info);
      mProtocols.AppendObject(proto);
    }
  }
#ifdef DEBUG_PURPLE_CORE_SERVICE
  printf("end init protocols\n");
#endif
}

/* void Quit (); */
NS_IMETHODIMP purpleCoreService::Quit()
{
  if (!mInitialized) {
    return NS_ERROR_FAILURE;
  }

  purple_core_quit();
  mConversations = nsnull;
  mAccounts.Clear();
  mProtocols.Clear();
  mInitialized = PR_FALSE;
  printf("purple_core_quit\n");

  return NS_OK;
}

/* readonly attribute string version; */
NS_IMETHODIMP purpleCoreService::GetVersion(char * *aVersion)
{
  const char* version = purple_core_get_version();
  if (!version)
    return NS_ERROR_FAILURE;

  size_t len = strlen(version) + 1;

  *aVersion = (char*)NS_Alloc(len);
  if (!*aVersion) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  memcpy(*aVersion, version, len);

  return NS_OK;
}

/* readonly attribute nsIArray conversations; */
NS_IMETHODIMP
purpleCoreService::GetConversations(nsIArray * *aConversations)
{
  *aConversations = mConversations;
  NS_ADDREF(*aConversations);

  return NS_OK;
}

/* purpleIConversation findConversationById (in long id); */
NS_IMETHODIMP
purpleCoreService::FindConversationById(PRInt32 aId,
					purpleIConversation **_retval)
{
  if (!aId)
    return NS_ERROR_NULL_POINTER;

  PRUint32 length;
  mConversations->GetLength(&length);
  for (PRUint32 i = 0; i < length; ++i) {
    nsCOMPtr<purpleIConversation> conv;
    mConversations->QueryElementAt(i, NS_GET_IID(purpleIConversation),
				   getter_AddRefs(conv));
    PRInt32 id;
    conv->GetIdConv(&id);
    if (id == aId) {
      *_retval = conv;
      return NS_OK;
    }
  }

  return NS_ERROR_FAILURE;
}

void purpleCoreService::AddConversation(purpleIConversation* aConv)
{
  mConversations->AppendElement(aConv, PR_FALSE);
}

/* nsISimpleEnumerator getProtocols (); */
NS_IMETHODIMP purpleCoreService::GetProtocols(nsISimpleEnumerator **aResult)
{
  if (!mInitialized) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  return NS_NewArrayEnumerator(aResult, mProtocols);
}

/* purpleIProtocol getProtocolById (in string aProtocolId); */
NS_IMETHODIMP purpleCoreService::GetProtocolById(const char *aProtocolId, purpleIProtocol **aResult)
{
  if (!mInitialized) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  nsCString id(aProtocolId);
  for (PRInt32 i = mProtocols.Count() - 1; i >= 0; --i) {
    if (id.Equals(mProtocols[i]->GetId())) {
      *aResult = mProtocols[i];
      NS_ADDREF(*aResult);
      return NS_OK;
    }
  }

  *aResult = nsnull;
  return NS_OK;
}

/* purpleIAccount getAccountById (in string accountId); */
NS_IMETHODIMP purpleCoreService::GetAccountById(const char *aAccountId, purpleIAccount **aResult)
{
  if (!mInitialized) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  for (PRInt32 i = mAccounts.Count() - 1; i >= 0; --i) {
    if (mAccounts[i]->GetKey().Equals(aAccountId)) {
      *aResult = mAccounts[i];
      NS_ADDREF(*aResult);
      return NS_OK;
    }
  }

  *aResult = nsnull;
  return NS_OK;
}

/* nsISimpleEnumerator getAccounts (); */
NS_IMETHODIMP purpleCoreService::GetAccounts(nsISimpleEnumerator **aResult)
{
  if (!mInitialized) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  return NS_NewArrayEnumerator(aResult, mAccounts);
}

/* purpleIAccount createAccount (in string name, in purpleIProtocol prpl); */
NS_IMETHODIMP purpleCoreService::CreateAccount(const char *name,
					       purpleIProtocol *prpl,
					       purpleIAccount **aResult)
{
  nsCOMPtr<purpleAccount> account = do_CreateInstance(PURPLE_ACCOUNT_CONTRACTID);
  purpleProtocol *proto = dynamic_cast<purpleProtocol *> (prpl); // :-(
  nsresult rv = account->create(name, proto);
  NS_ENSURE_SUCCESS(rv, rv);
  mAccounts.AppendObject(account);

  *aResult = account;
  NS_ADDREF(*aResult);
  return NS_OK;
}

/* void deleteAccount (in string accountId); */
NS_IMETHODIMP purpleCoreService::DeleteAccount(const char *accountId)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

//FIXME: save new accounts. Load accounts at startup.
