/* -*- Mode: C++; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsDockTile.h"
#import <Cocoa/Cocoa.h>

NS_IMPL_THREADSAFE_ISUPPORTS1(nsDockTile, nsIDockBadgeService)

nsDockTile::nsDockTile()
{
}

nsDockTile::~nsDockTile()
{
}

NS_IMETHODIMP nsDockTile::SetBadgeText(const nsAString& aBadgeText)
{
  id tile = [[NSApplication sharedApplication] dockTile];
  mBadgeText = aBadgeText;
  if (aBadgeText.IsEmpty())
    [tile setBadgeLabel: nil];
  else
    [tile setBadgeLabel:[NSString stringWithFormat:@"%S", mBadgeText.get()]];
  return NS_OK;
}

NS_IMETHODIMP nsDockTile::GetBadgeText(nsAString& aBadgeText)
{
  aBadgeText = mBadgeText;
  return NS_OK;
}
