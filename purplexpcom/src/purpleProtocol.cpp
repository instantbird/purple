#include "purpleProtocol.h"
#include "purplePref.h"
#include "purpleUtils.h"
#include "nsCOMPtr.h"
#include<nsComponentManagerUtils.h>
#include<nsArrayEnumerator.h>

//#define DEBUG_PURPLE_PROTOCOL

/* Implementation file */
NS_IMPL_ISUPPORTS1(purpleProtocol, purpleIProtocol)

purpleProtocol::purpleProtocol()
{
  mInfo = nsnull;
#ifdef DEBUG_PURPLE_PROTOCOL
  printf("creating protocol\n");
#endif
  /* member initializers and constructor code */
}

void purpleProtocol::Init(PurplePluginInfo *aInfo)
{
  mInfo = aInfo;
#ifdef DEBUG_PURPLE_PROTOCOL
  printf("setting mInfo to %i (%s, %s)\n",
	 (PRInt32) mInfo, mInfo->name, mInfo->id);

  printf("start init protocol options\n");
#endif
  PurplePluginProtocolInfo *prpl_info = (PurplePluginProtocolInfo *) mInfo->extra_info;
  GList *l;
  for (l = prpl_info->protocol_options; l; l = l->next) {
    nsCOMPtr<purplePref> pref = do_CreateInstance(PURPLE_PREF_CONTRACTID);
    pref->Init((PurpleAccountOption *) l->data);
    mOptions.AppendObject(pref);
  }

#if 0
  // this code demonstrate how to get info about userSplits.
  printf("proto %s\n", mInfo->name);
  for (l = prpl_info->user_splits; l; l = l->next) {
    PurpleAccountUserSplit *us = (PurpleAccountUserSplit *) l->data;
    printf(" %s (= %s), separator = '%c'\n",
	   us->text, us->default_value, us->field_sep);
  }
#endif

#ifdef DEBUG_PURPLE_PROTOCOL
  printf("end init protocol options\n");
#endif
}

purpleProtocol::~purpleProtocol()
{
#ifdef DEBUG_PURPLE_PROTOCOL
  printf("destructing protocol : %s\n", mInfo ? mInfo->name : "not initialized");
#endif
  /* destructor code */
}

/* readonly attribute string name; */
NS_IMETHODIMP purpleProtocol::GetName(char * *aName)
{
  if (!mInfo) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  return returnString(aName, mInfo->name);
}

/* readonly attribute string id; */
NS_IMETHODIMP purpleProtocol::GetId(char * *aId)
{
  if (!mInfo) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  return returnString(aId, mInfo->id);
}

/* nsISimpleEnumerator getOptionEnumerator (); */
NS_IMETHODIMP purpleProtocol::GetOptions(nsISimpleEnumerator **aResult)
{ 
  if (!mInfo) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  return NS_NewArrayEnumerator(aResult, mOptions);
}

inline nsresult purpleProtocol::GetBoolOpt(PRBool *aRes,
					   PurpleProtocolOptions aOpt)
{
  if (!mInfo) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  PurplePluginProtocolInfo *prpl_info =
    (PurplePluginProtocolInfo *) mInfo->extra_info;
  
  *aRes = prpl_info->options & aOpt ? PR_TRUE : PR_FALSE;

  return NS_OK;
}

/* readonly attribute boolean uniqueChatName; */
NS_IMETHODIMP purpleProtocol::GetUniqueChatName(PRBool *aUniqueChatName)
{
  return GetBoolOpt(aUniqueChatName, OPT_PROTO_UNIQUE_CHATNAME);
}

/* readonly attribute boolean chatHasTopic; */
NS_IMETHODIMP purpleProtocol::GetChatHasTopic(PRBool *aChatHasTopic)
{
  return GetBoolOpt(aChatHasTopic, OPT_PROTO_CHAT_TOPIC);
}

/* readonly attribute boolean noPassword; */
NS_IMETHODIMP purpleProtocol::GetNoPassword(PRBool *aNoPassword)
{
  return GetBoolOpt(aNoPassword, OPT_PROTO_NO_PASSWORD);
}

/* readonly attribute boolean newMailNotification; */
NS_IMETHODIMP purpleProtocol::GetNewMailNotification(PRBool *aNewMailNotification)
{
  return GetBoolOpt(aNewMailNotification, OPT_PROTO_MAIL_CHECK);
}

/* readonly attribute boolean imagesInIM; */
NS_IMETHODIMP purpleProtocol::GetImagesInIM(PRBool *aImagesInIM)
{
  return GetBoolOpt(aImagesInIM, OPT_PROTO_IM_IMAGE);
}

/* readonly attribute boolean passwordOptional; */
NS_IMETHODIMP purpleProtocol::GetPasswordOptional(PRBool *aPasswordOptional)
{
  return GetBoolOpt(aPasswordOptional, OPT_PROTO_PASSWORD_OPTIONAL);
}

/* readonly attribute boolean usePointSize; */
NS_IMETHODIMP purpleProtocol::GetUsePointSize(PRBool *aUsePointSize)
{
  return GetBoolOpt(aUsePointSize, OPT_PROTO_USE_POINTSIZE);
}

/* readonly attribute boolean registerNoScreenName; */
NS_IMETHODIMP purpleProtocol::GetRegisterNoScreenName(PRBool *aRegisterNoScreenName)
{
  return GetBoolOpt(aRegisterNoScreenName, OPT_PROTO_REGISTER_NOSCREENNAME);
}
