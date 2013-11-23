/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <nsCOMPtr.h>
#include <nsCOMArray.h>

#pragma GCC visibility push(default)
#include <glib.h>
#include <libpurple/dnsquery.h>
#pragma GCC visibility pop

#include <nsIDNSService.h>
#include <nsIDNSRecord.h>
#include <nsIDNSListener.h>

class purpleDNSRequest MOZ_FINAL : public nsIDNSListener
{
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIDNSLISTENER

  purpleDNSRequest();
  ~purpleDNSRequest();
  PRUint32 mAccountId;
  PurpleDnsQueryData *query_data;
  PurpleDnsQueryResolvedCallback resolved_cb;
  PurpleDnsQueryFailedCallback failed_cb;
  nsCOMPtr<nsICancelable> asyncResolv;

 private:
  inline void Failed(const char *aMsg);
};

class purpleDNS
{
 public:
  static void init();
  static void unInit();

  static gboolean Resolve(PurpleDnsQueryData *query_data,
                          PurpleDnsQueryResolvedCallback resolved_cb,
                          PurpleDnsQueryFailedCallback failed_cb);
  static void Cancel(PurpleDnsQueryData *query_data);

  static nsresult Remove(PurpleDnsQueryData *query_data);

 private:
  static nsCOMArray<purpleDNSRequest> *sRequests;
};
