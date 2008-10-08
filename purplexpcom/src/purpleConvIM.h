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
 * 2008.
 *
 * The Initial Developer of the Original Code is
 * Florian QUEZE <florian@instantbird.org>.
 * Portions created by the Initial Developer are Copyright (C) 2008
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

#ifndef PURPLE_CONV_IM_H_
# define PURPLE_CONV_IM_H_

#include "purpleConversation.h"

// 1646fccc-d3cb-488a-81fe-1b2aa60482a5
#define PURPLE_CONV_IM_CID \
{ 0x1646fccc, 0xd3cb, 0x488a, \
  { 0x81, 0xfe, 0x1b, 0x2a, 0xa6, 0x04, 0x82, 0xa5 } \
 }

#define PURPLE_CONV_IM_CONTRACTID     "@instantbird.org/purple/convim;1"

class purpleConvIM : public purpleIConvIM, public purpleConversation
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_PURPLEICONVIM

  // Forward most methods to purpleConversation
  NS_SCRIPTABLE NS_IMETHOD GetAccount(purpleIAccount * *aAccount) {
    return purpleConversation::GetAccount(aAccount);
  }
  NS_SCRIPTABLE NS_IMETHOD GetName(nsACString & aName) {
    return purpleConversation::GetName(aName);
  }
  NS_SCRIPTABLE NS_IMETHOD GetTitle(nsACString & aTitle) {
    return purpleConversation::GetTitle(aTitle);
  }
  NS_SCRIPTABLE NS_IMETHOD GetId(PRUint32 *aId) {
    return purpleConversation::GetId(aId);
  }
  NS_SCRIPTABLE NS_IMETHOD Close(void) {
    return purpleConversation::Close();
  }
  NS_SCRIPTABLE NS_IMETHOD AddObserver(nsIObserver *aObserver) {
    return purpleConversation::AddObserver(aObserver);
  }
  NS_SCRIPTABLE NS_IMETHOD RemoveObserver(nsIObserver *aObserver) {
    return purpleConversation::RemoveObserver(aObserver);
  }

  // Keep the SendMsg method here
  NS_SCRIPTABLE NS_IMETHOD SendMsg(const nsACString & aMsg);

  purpleConvIM();

private:
  ~purpleConvIM();

protected:
  PRBool mSentTyping;
};

#endif /* !PURPLE_CONV_IM_H_ */
