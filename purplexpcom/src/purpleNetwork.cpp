/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "purpleNetwork.h"
#include <nsCOMPtr.h>
#include <nsNetCID.h>
#include <nsServiceManagerUtils.h>
#include <nsIObserverService.h>
#include <nsINetworkLinkService.h>

#include <glib.h>
#pragma GCC visibility push(default)
#include <libpurple/network.h>
#pragma GCC visibility pop

purpleNetworkObserver *purpleNetworkObserver::mInstance = nullptr;

NS_IMPL_ISUPPORTS1(purpleNetworkObserver, nsIObserver)

purpleNetworkObserver::purpleNetworkObserver()
{
  purple_network_set_available_callback(purpleNetworkObserver::IsNetworkAvailable);

  nsCOMPtr<nsIObserverService> observerService =
    do_GetService("@mozilla.org/observer-service;1");
  observerService->AddObserver(this, NS_NETWORK_LINK_TOPIC, PR_FALSE);
}

purpleNetworkObserver::~purpleNetworkObserver()
{
  purple_network_set_available_callback(NULL);
}

void
purpleNetworkObserver::init()
{
  if (!mInstance)
    mInstance = new purpleNetworkObserver();
}

void
purpleNetworkObserver::unInit()
{
  if (!mInstance)
    return;

  nsCOMPtr<nsIObserverService> observerService =
    do_GetService("@mozilla.org/observer-service;1");
  observerService->RemoveObserver(mInstance, NS_NETWORK_LINK_TOPIC);

  mInstance = nullptr;
}

gboolean
purpleNetworkObserver::IsNetworkAvailable()
{
  nsCOMPtr<nsINetworkLinkService> networkLinkService =
    do_GetService(NS_NETWORK_LINK_SERVICE_CONTRACTID);
  NS_ENSURE_TRUE(networkLinkService, TRUE);

  bool isUp;
  nsresult rv = networkLinkService->GetIsLinkUp(&isUp);
  NS_ENSURE_SUCCESS(rv, TRUE);
  return isUp;
}

NS_IMETHODIMP
purpleNetworkObserver::Observe(nsISupports *aSubject,
                               const char *aTopic,
                               const PRUnichar *aData)
{
  purple_network_configuration_changed();

  return NS_OK;
}
