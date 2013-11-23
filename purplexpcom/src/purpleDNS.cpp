/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "purpleDNS.h"
#include "purpleAccountScoper.h"
#include "mozilla/net/DNS.h"
#include <nsComponentManagerUtils.h>
#include <nsServiceManagerUtils.h>
#include <nsNetUtil.h>
#include <nsIThread.h>
#include <nsThreadUtils.h>
#include <nsICancelable.h>
#include <prio.h>
#ifdef XP_WIN
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <sys/un.h>
#endif

#ifdef PR_LOGGING
#include <prnetdb.h>

//
// NSPR_LOG_MODULES=purpleDNS:5
//
static PRLogModuleInfo *gPurpleDNSLog = nullptr;
#endif
#define LOG(args) PR_LOG(gPurpleDNSLog, PR_LOG_DEBUG, args)

/* Init static members */
nsCOMArray<purpleDNSRequest> *purpleDNS::sRequests = nullptr;

void purpleDNS::init()
{
#ifdef PR_LOGGING
  if (!gPurpleDNSLog)
    gPurpleDNSLog = PR_NewLogModule("purpleDNS");
#endif
  sRequests = new nsCOMArray<purpleDNSRequest>();
}

void purpleDNS::unInit()
{
  LOG(("purpleDNS:unInit: Canceling %i pending DNS requests",
       sRequests->Count()));
  for (PRInt32 i = 0; i < sRequests->Count(); ++i)
    (*sRequests)[i]->asyncResolv->Cancel(NS_ERROR_FAILURE);
  delete sRequests;
  sRequests = nullptr;
}

gboolean purpleDNS::Resolve(PurpleDnsQueryData *query_data,
                            PurpleDnsQueryResolvedCallback resolved_cb,
                            PurpleDnsQueryFailedCallback failed_cb)
{
  NS_ENSURE_TRUE(!NS_IsOffline(), false);
  nsCString host(purple_dnsquery_get_host(query_data));
  LOG(("Resolving with moz: %s", host.get()));
  NS_ENSURE_TRUE(sRequests, false);

  nsCOMPtr<nsIDNSService> dns = do_GetService("@mozilla.org/network/dns-service;1");
  nsCOMPtr<nsIDNSRecord> record;
  nsCOMPtr<nsICancelable> cancelable;
  nsCOMPtr<nsIThread> thread = do_GetMainThread();
  nsCOMPtr<purpleDNSRequest> listener;
  listener = new purpleDNSRequest();
  listener->mAccountId = purpleAccountScoper::GetCurrentAccountId();
  listener->query_data = query_data;
  listener->resolved_cb = resolved_cb;
  listener->failed_cb = failed_cb;

  nsresult rv = dns->AsyncResolve(host, 0, listener, thread,
                                  getter_AddRefs(listener->asyncResolv));
  NS_ENSURE_SUCCESS(rv, false);// The request wasn't handled.

  sRequests->AppendObject(listener);
  return true; // We handle the request, libpurple shouldn't try to do it.
}

nsresult purpleDNS::Remove(PurpleDnsQueryData *query_data)
{
  LOG(("purpleDNS::Remove query_data=@%x", query_data));
  NS_ENSURE_TRUE(sRequests, NS_ERROR_FAILURE);

  for (PRInt32 i = sRequests->Count() - 1; i >= 0; --i) {
    if ((*sRequests)[i]->query_data == query_data) {
      sRequests->RemoveObjectAt(i);
      LOG(("Remove by query_data: found at %i", i));
      return NS_OK;
    }
  }
  LOG(("Remove by query_data: not found"));
  return NS_ERROR_FAILURE;
}

void purpleDNS::Cancel(PurpleDnsQueryData *query_data)
{
  LOG(("purpleDNS::Cancel query_data=@%x", query_data));
  NS_ENSURE_TRUE(sRequests, );

  for (PRInt32 i = sRequests->Count() - 1; i >= 0; --i) {
    if ((*sRequests)[i]->query_data == query_data) {
      (*sRequests)[i]->asyncResolv->Cancel(NS_ERROR_FAILURE);
      sRequests->RemoveObjectAt(i);
      LOG(("Canceling by query_data: found at %i", i));
      return;
    }
  }
  LOG(("Canceling by query_data: not found"));
}

NS_IMPL_ISUPPORTS1(purpleDNSRequest, nsIDNSListener)

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

  LOG(("purpleDNSRequest::Failed with msg: %s", aMsg));

  if (NS_FAILED(purpleDNS::Remove(query_data))) {
    LOG(("purpleDNSRequest::Failed, not calling callback because already cancelled"));
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

  purpleAccountScoper scoper(mAccountId);

  if (NS_FAILED(status)) {
    Failed("DNS query failed\n");
    return NS_OK;
  }

  GSList *hosts = NULL;
  bool more;
  record->HasMore(&more);
  if (!more) {
    Failed("Not found\n");
    return NS_OK;
  }

  if (NS_FAILED(purpleDNS::Remove(query_data))) {
    LOG(("DNS resolution completed but result discarded because it was cancelled"));
    return NS_OK;
  }

  mozilla::net::NetAddr netAddr;
  for (; more; record->HasMore(&more))
  {
    unsigned short port = purple_dnsquery_get_port(query_data);
    if (NS_FAILED(record->GetNextAddr(port, &netAddr))) {
      Failed("GetNextAddr failed\n");
      return NS_OK;
    }

    socklen_t addrlen;
    struct sockaddr *addr;

    if (netAddr.raw.family == AF_INET) {
      addrlen = sizeof(struct sockaddr_in);
      struct sockaddr_in *s_in = (struct sockaddr_in *)g_malloc0(addrlen);
      s_in->sin_port = netAddr.inet.port;
      memcpy(&s_in->sin_addr, &netAddr.inet.ip, sizeof(netAddr.inet.ip));
      addr = (struct sockaddr *)s_in;
      LOG(("Found ip v4 address"));
    }
    else if (netAddr.raw.family == AF_INET6) {
      addrlen = sizeof(struct sockaddr_in6);
      struct sockaddr_in6 *s_in6 = (struct sockaddr_in6 *)g_malloc0(addrlen);
      s_in6->sin6_port = netAddr.inet6.port;
      s_in6->sin6_flowinfo = netAddr.inet6.flowinfo;
      memcpy(&s_in6->sin6_addr, &netAddr.inet6.ip, sizeof(netAddr.inet6.ip.u8));
      s_in6->sin6_scope_id = netAddr.inet6.scope_id;
      addr = (struct sockaddr *)s_in6;
      LOG(("Found ip v6 address"));
    }
#ifndef XP_WIN
    else if (netAddr.raw.family == AF_LOCAL) {
      addrlen = sizeof(struct sockaddr_un);
      struct sockaddr_un *s_un = (struct sockaddr_un *)g_malloc0(addrlen);
      memcpy(&s_un->sun_path, netAddr.local.path, sizeof(netAddr.local.path));
      addr = (struct sockaddr *)s_un;
      LOG(("Found local address"));
    }
#endif
    else {
      NS_NOTREACHED("Unknown address type...");
      continue;
    }

    addr->sa_family = netAddr.raw.family;

    hosts = g_slist_append(hosts, GINT_TO_POINTER(addrlen));
    hosts = g_slist_append(hosts, addr);
  }
  LOG(("DNS resolution done"));
  resolved_cb(query_data, hosts);
  return NS_OK;
}
