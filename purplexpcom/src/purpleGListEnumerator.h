/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef PURPLE_G_LIST_ENUMERATOR_H
# define PURPLE_G_LIST_ENUMERATOR_H

# include <glib.h>
# include "nsISimpleEnumerator.h"

class purpleGListEnumerator MOZ_FINAL : public nsISimpleEnumerator
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSISIMPLEENUMERATOR

  // Ctor
  purpleGListEnumerator();

  // Dtor
  ~purpleGListEnumerator();

  typedef nsISupports* (*converterCallback) (void *);
  typedef void (*cleanupCallback) (void *);

  nsresult Init(GList *aGList, converterCallback aDataToSupports,
                cleanupCallback aCleanupCallback = NULL,
                void *aCleanupData = NULL);

private:
  void Cleanup();

  GList *mGList;
  converterCallback mDataToSupports;
  cleanupCallback mCleanupCallback;
  void *mCleanupData;
};

template <typename implclass, typename iface, typename PurpleType>
nsISupports* purpleTypeToInterface(void *aData)
{
  implclass *instance = new implclass();
  instance->Init((PurpleType *)aData);

  NS_ADDREF(instance);
  return static_cast<iface *>(instance);
}

#endif /* !PURPLE_G_LIST_ENUMERATOR_H */
