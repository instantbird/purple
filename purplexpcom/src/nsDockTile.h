/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsIDockBadgeService.h"
#include "nsCOMPtr.h"
#include "nsStringAPI.h"

// 6a9516f9-4b9b-4e5e-aa7e-209b4dd0d724
#define NSDOCKBADGESERVICE_CID                            \
  { 0x6a9516f9, 0x4b9b, 0x4e5e,                           \
      { 0xaa, 0x7e, 0x20, 0x9b, 0x4d, 0xd0, 0xd7, 0x24 }  \
  }

#define NSDOCKBADGESERVICE_CONTRACTID     "@instantbird.org/purple/nsdockbadgeservice;1"

// OS X dock tile
class nsDockTile : public nsIDockBadgeService
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIDOCKBADGESERVICE

  nsDockTile();

private:
  ~nsDockTile();

protected:
  nsString mBadgeText;
};
