/* ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is the Instantbird messenging client, released
 * 2009.
 *
 * The Initial Developer of the Original Code is
 * Florian QUEZE <florian@instantbird.org>.
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

#ifndef PURPLE_LOG_H_
# define PURPLE_LOG_H_

#include "purpleILog.h"
#include <nsIClassInfo.h>

#pragma GCC visibility push(default)
#include <libpurple/log.h>
#pragma GCC visibility pop

// bb4e8cfa-fe9d-4784-af4c-309ac5b468f4
#define PURPLE_LOG_CID \
{ 0xbb4e8cfa, 0xfe9d, 0x4784, \
  { 0xaf, 0x4c, 0x30, 0x9a, 0xc5, 0xb4, 0x68, 0xf4 } \
 }

#define PURPLE_LOG_CONTRACTID     "@instantbird.org/purple/log;1"

class purpleLog : public purpleILog,
                  public nsIClassInfo
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSICLASSINFO
  NS_DECL_PURPLEILOG

  purpleLog();
  void Init(PurpleLog *aLog);

private:
  ~purpleLog();

protected:
  PurpleLog *mLog;
};

#endif /* !PURPLE_LOG_H_ */
