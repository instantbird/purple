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

#include "purpleDNS.h"
#include <nsComponentManagerUtils.h>
#include <nsServiceManagerUtils.h>
#include <nsIThread.h>
#include <nsThreadUtils.h>
#include <nsICancelable.h>
#include <prio.h>

//#define DEBUG_PURPLE_DNS

#ifdef DEBUG_PURPLE_DNS
# include<stdio.h>
#endif

/* Init static members */
nsCOMArray<purpleDNSRequest> *purpleDNS::sRequests = nsnull;

void purpleDNS::init()
{
  sRequests = new nsCOMArray<purpleDNSRequest>();
}

void purpleDNS::unInit()
{
  delete sRequests;
}

gboolean purpleDNS::Resolve(PurpleDnsQueryData *query_data,
                            PurpleDnsQueryResolvedCallback resolved_cb,
                            PurpleDnsQueryFailedCallback failed_cb)
{
  nsCString host(purple_dnsquery_get_host(query_data));
#ifdef DEBUG_PURPLE_DNS
  printf("resolving with moz: %s\n", host.get());
#endif
  nsCOMPtr<nsIDNSService> dns = do_GetService("@mozilla.org/network/dns-service;1");
  nsCOMPtr<nsIDNSRecord> record;
  nsCOMPtr<nsICancelable> cancelable;
  nsCOMPtr<nsIThread> thread = do_GetMainThread();
  nsCOMPtr<purpleDNSRequest> listener;
  listener = new purpleDNSRequest();
  listener->query_data = query_data;
  listener->resolved_cb = resolved_cb;
  listener->failed_cb = failed_cb;

  nsresult rv = dns->AsyncResolve(host, 0, listener, thread,
                                  getter_AddRefs(listener->asyncResolv));
  if (NS_FAILED(rv))
    return false; // The request wasn't handled.

  sRequests->AppendObject(listener);
  return true; // We handle the request, libpurple shouldn't try to do it.
}

nsresult purpleDNS::Remove(PurpleDnsQueryData *query_data)
{
#ifdef DEBUG_PURPLE_DNS
  printf("Remove by query_data... ");
#endif
  for (PRInt32 i = sRequests->Count() - 1; i >= 0; --i) {
    if ((*sRequests)[i]->query_data == query_data) {
      sRequests->RemoveObjectAt(i);
#ifdef DEBUG_PURPLE_DNS
      printf("found at %i\n", i);
#endif
      return NS_OK;
    }
  }
#ifdef DEBUG_PURPLE_DNS
  printf("not found\n");
#endif
  return NS_ERROR_FAILURE;
}

void purpleDNS::Cancel(PurpleDnsQueryData *query_data)
{
#ifdef DEBUG_PURPLE_DNS
  printf("Canceling by query_data... ");
#endif

  for (PRInt32 i = sRequests->Count() - 1; i >= 0; --i) {
    if ((*sRequests)[i]->query_data == query_data) {
      (*sRequests)[i]->asyncResolv->Cancel(NS_ERROR_FAILURE);
      sRequests->RemoveObjectAt(i);
#ifdef DEBUG_PURPLE_DNS
      printf("found at %i\n", i);
#endif
      return;
    }
  }
#ifdef DEBUG_PURPLE_DNS
  printf("not found\n");
#endif
}

NS_IMPL_THREADSAFE_ISUPPORTS1(purpleDNSRequest, nsIDNSListener)

purpleDNSRequest::purpleDNSRequest()
{
  MOZ_COUNT_CTOR(purpleDNSRequest);
}

purpleDNSRequest::~purpleDNSRequest()
{
  MOZ_COUNT_DTOR(purpleDNSRequest);
}

void purpleDNSRequest::Failed(const char *aMsg)
{
  NS_ASSERTION(NS_IsMainThread(), "wrong thread");

#ifdef DEBUG_PURPLE_DNS
  printf("purpleDNSRequest::Failed with msg: %s\n", aMsg);
#endif

  if (NS_FAILED(purpleDNS::Remove(query_data))) {
#ifdef DEBUG_PURPLE_DNS
    printf("purpleDNSRequest::Failed, not calling callback because already cancelled\n");
#endif
    return;
  }

  failed_cb(query_data, aMsg);
}

nsresult purpleDNSRequest::OnLookupComplete(nsICancelable *request,
                                            nsIDNSRecord *record,
                                            nsresult status)
{
  NS_ASSERTION(NS_IsMainThread(), "wrong thread");
  NS_ASSERTION(request == asyncResolv, "wrong request");

  if (NS_FAILED(status)) {
    Failed("DNS query failed\n");
    return NS_OK;
  }

  GSList *hosts = NULL;
  PRBool more;
  record->HasMore(&more);
  if (!more) {
    Failed("Not found\n");
    return NS_OK;
  }

  if (NS_FAILED(purpleDNS::Remove(query_data))) {
#ifdef DEBUG_PURPLE_DNS
  printf("dns resolution completed but result discarded because it was cancelled\n");
#endif
    return NS_OK;
  }

  union PRNetAddr addr;
  while (more)
  {
    record->GetNextAddr(purple_dnsquery_get_port(query_data), &addr);
    hosts = g_slist_append(hosts, GINT_TO_POINTER(sizeof(addr.inet)));
#ifndef XP_WIN
    //actually this may only be needed on Mac OS X
    sockaddr *addr2 = (sockaddr *) g_memdup(&addr.inet, sizeof(addr.inet));
    addr2->sa_family = addr.inet.family; //very ugly!!
    hosts = g_slist_append(hosts, addr2);
#else
    hosts = g_slist_append(hosts, g_memdup(&addr.inet, sizeof(addr.inet)));
#endif

#ifdef DEBUG_PURPLE_DNS
    char ipaddr[INET6_ADDRSTRLEN];
    inet_ntop(addr2->sa_family, &((struct sockaddr_in *)addr2)->sin_addr,
              ipaddr, sizeof(ipaddr));
    printf("found ip: %s\n", ipaddr);
#endif

    record->HasMore(&more);
  }
#ifdef DEBUG_PURPLE_DNS
  printf("dns resolution done\n");
#endif
  resolved_cb(query_data, hosts);
  return NS_OK;
}
