/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef PURPLE_TOOLTIP_INFO_H_
# define PURPLE_TOOLTIP_INFO_H_

#include "prplITooltipInfo.h"
#include <nsCOMPtr.h>
#include <nsStringAPI.h>
#include <nsIClassInfo.h>

#pragma GCC visibility push(default)
#include <libpurple/signals.h>
#include <libpurple/notify.h>
#pragma GCC visibility pop

// dd535741-4b04-49ca-8df6-08f8577fe52b
#define PURPLE_TOOLTIPINFO_CID                                  \
  { 0xdd535741, 0x4b04, 0x49ca,                                 \
      { 0x8d, 0xf6, 0x08, 0xf8, 0x57, 0x7f, 0xe5, 0x2b }        \
  }

class purpleTooltipInfo MOZ_FINAL : public prplITooltipInfo,
                                    public nsIClassInfo
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSICLASSINFO
  NS_DECL_PRPLITOOLTIPINFO

  purpleTooltipInfo();
  void Init(PurpleNotifyUserInfoEntry *aEntry);

private:
  ~purpleTooltipInfo();

protected:
  /* additional members */
  PRInt16 mType;
  nsCString mLabel;
  nsCString mValue;
};

#endif /* !PURPLE_TOOLTIP_INFO_H_ */
