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

#include "purpleContentPolicy.h"
#include <prlog.h>
#include <nsStringAPI.h>
#include <nsIURI.h>

NS_IMPL_ISUPPORTS1(purpleContentPolicy, nsIContentPolicy)

#ifdef PR_LOGGING
//
// NSPR_LOG_MODULES=purpleContentPolicy:5
//
static PRLogModuleInfo *gPurpleContentPolicyLog = nsnull;
#endif
#define LOG(args) PR_LOG(gPurpleContentPolicyLog, PR_LOG_DEBUG, args)

purpleContentPolicy::purpleContentPolicy()
{
  /* member initializers and constructor code */
#ifdef PR_LOGGING
  if (!gPurpleContentPolicyLog)
    gPurpleContentPolicyLog = PR_NewLogModule("purpleContentPolicy");
#endif
  LOG(("purpleContentPolicy()"));
}

purpleContentPolicy::~purpleContentPolicy()
{
  /* destructor code */
  LOG(("~purpleContentPolicy()"));
}

NS_IMETHODIMP purpleContentPolicy::ShouldLoad(PRUint32 aContentType,
                                              nsIURI *aContentLocation,
                                              nsIURI *aRequestOrigin,
                                              nsISupports *aContext,
                                              const nsACString & aMimeTypeGuess,
                                              nsISupports *aExtra,
                                              PRInt16 *aDecision NS_OUTPARAM)
{
  *aDecision = nsIContentPolicy::ACCEPT;

  if (!aRequestOrigin)
    return NS_OK;

  nsCString from;
  aRequestOrigin->GetSpec(from);
#ifdef PR_LOGGING
  nsCString spec;
  aContentLocation->GetSpec(spec);
  LOG(("purpleContentPolicy::ShouldLoad %s requested by %s", spec.get(), from.get()));
#endif

  if (from.EqualsLiteral("about:blank")) {
    // assume we are in a conversation browser.
    // FIXME: find a real way to check this

    switch (aContentType) {
      case nsIContentPolicy::TYPE_OBJECT: // plugins
      case nsIContentPolicy::TYPE_SCRIPT: // javascript
      case nsIContentPolicy::TYPE_SUBDOCUMENT: // Sub Frames
      case nsIContentPolicy::TYPE_REFRESH: // Meta refresh
        NS_WARNING("blocked bad request");
        *aDecision = nsIContentPolicy::REJECT_TYPE;
        break;

      case nsIContentPolicy::TYPE_IMAGE: // images
        // Allow chrome images (for smileys)
        nsCString scheme;
        aContentLocation->GetScheme(scheme);
        if (!scheme.EqualsLiteral("chrome")) {
          NS_WARNING("blocked loading of image from non-chrome scheme");
          *aDecision = nsIContentPolicy::REJECT_SERVER;
        }
        break;
    }
  }

  return NS_OK;
}

NS_IMETHODIMP purpleContentPolicy::ShouldProcess(PRUint32 aContentType,
                                                 nsIURI *aContentLocation,
                                                 nsIURI *aRequestOrigin,
                                                 nsISupports *aContext,
                                                 const nsACString & aMimeType,
                                                 nsISupports *aExtra,
                                                 PRInt16 *aDecision NS_OUTPARAM)
{
#ifdef PR_LOGGING
  nsCString spec;
  aContentLocation->GetSpec(spec);
  LOG(("purpleContentPolicy::ShouldProcess %s", spec.get()));
#endif
  *aDecision = nsIContentPolicy::ACCEPT;
  return NS_OK;
}
