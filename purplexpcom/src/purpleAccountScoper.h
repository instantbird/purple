/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef PURPLE_ACCOUNT_SCOPER_H_
# define PURPLE_ACCOUNT_SCOPER_H_

#include <prtypes.h>
class purpleAccount;

class purpleAccountScoper
{
 public:
  purpleAccountScoper(purpleAccount *aAccount);
  purpleAccountScoper(PRUint32 aId);
  ~purpleAccountScoper();
  static PRUint32 GetCurrentAccountId() { return sCurrentId; }
 private:
  void enterScope(PRUint32 aId);
  static PRUint32 sCurrentId;
  PRUint32 mPreviousId;
};

#endif /* !PURPLE_ACCOUNT_SCOPER_H_ */
