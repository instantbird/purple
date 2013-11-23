/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef PURPLE_PROTOCOL_H_
# define PURPLE_PROTOCOL_H_

#include "purpleProxyInfo.h"
#include "prplIProtocol.h"
#include "prplIPref.h"
#pragma GCC visibility push(default)
#include <libpurple/account.h>
#pragma GCC visibility pop

// 63ee9306-8615-4896-9f1f-6e6fafe17d1e
#define PURPLE_PROTOCOL_CID \
  { 0x63ee9306, 0x8615, 0x4896,                                 \
      { 0x9f, 0x1f, 0x6e, 0x6f, 0xaf, 0xe1, 0x7d, 0x1e }        \
  }

#define PURPLE_PROTOCOL_CONTRACTID \
    "@instantbird.org/purple/protocol;1"

class purpleProtocol MOZ_FINAL : public prplIProtocol
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_PRPLIPROTOCOL

  purpleProtocol();

private:
  ~purpleProtocol();

  PurplePluginInfo *mInfo;

protected:
  /* additional members */
};

#endif /* !PURPLE_PROTOCOL_H_ */
