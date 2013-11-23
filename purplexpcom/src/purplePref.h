/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "prplIPref.h"
#include <nsIClassInfo.h>

#pragma GCC visibility push(default)
#include <libpurple/accountopt.h>
#pragma GCC visibility pop

// fe21354d-2932-4a12-8d5b-15ac975d97e6
#define PURPLE_PREF_CID                                         \
  { 0xfe21354d, 0x2932, 0x4a12,                                 \
      { 0x8d, 0x5b, 0x15, 0xac, 0x97, 0x5d, 0x97, 0xe6 }        \
  }

#define PURPLE_PREF_CONTRACTID \
  "@instantbird.org/purple/pref;1"

class purplePref MOZ_FINAL : public prplIPref,
                             public nsIClassInfo
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSICLASSINFO
  NS_DECL_PRPLIPREF

  purplePref();
  void Init(PurpleAccountOption *aOpt);

private:
  ~purplePref();

  PurpleAccountOption *mOpt;

protected:
  /* additional members */
};
