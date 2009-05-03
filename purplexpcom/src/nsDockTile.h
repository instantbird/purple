/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is WebRunner
 *
 * The Initial Developer of the Original Code is
 * Matthew Gertner.
 * Portions created by the Initial Developer are Copyright (C) 2007
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Matthew Gertner <matthew.gertner@gmail.com> (Original author)
 *   Florian Quèze <florian@instantbird.org>
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
