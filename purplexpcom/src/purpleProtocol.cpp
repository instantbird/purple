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

#include "purpleProtocol.h"
#include "purplePref.h"
#include "nsCOMPtr.h"
#include<nsComponentManagerUtils.h>
#include<nsArrayEnumerator.h>

//#define DEBUG_PURPLE_PROTOCOL
//#define DEBUG_DUMP_PREF_TEXT

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
	 (PRUint32) mInfo, mInfo->name, mInfo->id);

  printf("start init protocol options\n");
#endif
  PurplePluginProtocolInfo *prpl_info = (PurplePluginProtocolInfo *) mInfo->extra_info;
  GList *l;
  for (l = prpl_info->protocol_options; l; l = l->next) {
    nsCOMPtr<purplePref> pref = do_CreateInstance(PURPLE_PREF_CONTRACTID);

#ifdef DEBUG_DUMP_PREF_TEXT   
    PurpleAccountOption *pao = (PurpleAccountOption *) l->data;
    printf("%s.%s=%s\n", mInfo->id, pao->pref_name, pao->text);
#endif
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

/* readonly attribute AUTF8String name; */
NS_IMETHODIMP purpleProtocol::GetName(nsACString& aName)
{
  NS_ENSURE_TRUE(mInfo, NS_ERROR_NOT_INITIALIZED);

  aName = mInfo->name;
  return NS_OK;
}

/* readonly attribute AUTF8String id; */
NS_IMETHODIMP purpleProtocol::GetId(nsACString& aId)
{
  NS_ENSURE_TRUE(mInfo, NS_ERROR_NOT_INITIALIZED);

  aId = mInfo->id;
  return NS_OK;
}

/* nsISimpleEnumerator getOptionEnumerator (); */
NS_IMETHODIMP purpleProtocol::GetOptions(nsISimpleEnumerator **aResult)
{ 
  NS_ENSURE_TRUE(mInfo, NS_ERROR_NOT_INITIALIZED);

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
