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
 * 2007.
 *
 * The Initial Developer of the Original Code is
 * Florian QUEZE <florian@instantbird.org>.
 * Portions created by the Initial Developer are Copyright (C) 2007
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

#include "nscore.h"
#include "nsModule.h"
#include "prlink.h"
#include "nsILocalFile.h"
#ifdef XP_MACOSX
# include <mach-o/dyld.h>
#endif
#include "nsStringAPI.h"

#include "nsCOMPtr.h"

//#define DEBUG_PURPLE_LOADER

#define PRPL_PREFIX ""

static char const *const kDependentLibraries[] =
  {
    // dependent1.dll on windows, libdependent1.so on linux

#ifdef XP_WIN
	"zlib1" DLL_SUFFIX,
	"iconv" DLL_SUFFIX,
	 "libxml2" DLL_SUFFIX,
     "intl" DLL_SUFFIX,
	  "libglib-2.0-0" DLL_SUFFIX,
      "libgmodule-2.0-0" DLL_SUFFIX,
	  "libgthread-2.0-0" DLL_SUFFIX,
       "libpurple" DLL_SUFFIX,
        "liboscar" DLL_SUFFIX,
        "libjabber" DLL_SUFFIX,
		"dnssd" DLL_SUFFIX,
		"libmeanwhile-1" DLL_SUFFIX,
		"libsilc-1-1-2" DLL_SUFFIX,
		"libsilcclient-1-1-2" DLL_SUFFIX,
#endif
#ifdef XP_MACOSX
    DLL_PREFIX "z.1" DLL_SUFFIX,
    DLL_PREFIX "iconv.2" DLL_SUFFIX,
    DLL_PREFIX "xml2.2" DLL_SUFFIX,
    DLL_PREFIX "intl.8" DLL_SUFFIX,
    DLL_PREFIX "glib-2.0.0" DLL_SUFFIX,
    DLL_PREFIX "gobject-2.0.0" DLL_SUFFIX,
    DLL_PREFIX "gmodule-2.0.0" DLL_SUFFIX,
    DLL_PREFIX "gthread-2.0.0" DLL_SUFFIX,
    DLL_PREFIX "purple" DLL_SUFFIX,
    DLL_PREFIX "jabber.0" DLL_SUFFIX,
    DLL_PREFIX "oscar.0" DLL_SUFFIX,
#endif
#if !(defined(XP_WIN)) && !(defined(XP_MACOSX))
    DLL_PREFIX "purple" DLL_SUFFIX,
#endif
  nsnull

    // NOTE: if the dependent libs themselves depend on other libs, the subdependencies
    // should be listed first.
  };

// component.dll on windows, libcomponent.dll on linux
static char kRealComponent[] = DLL_PREFIX "purplexpcom" DLL_SUFFIX;

nsresult
NSGetModule(nsIComponentManager* aCompMgr,
            nsIFile* aLocation,
            nsIModule* *aResult)
{
  nsresult rv;

  // This is not the real component. We want to load the dependent libraries
  // of the real component, then the component itself, and call NSGetModule on
  // the component.

  // Assume that we're in <extensiondir>/components, and we want to find
  // <extensiondir>/libraries

  nsCOMPtr<nsIFile> libraries;
  rv = aLocation->GetParent(getter_AddRefs(libraries));
  if (NS_FAILED(rv))
    return rv;

  nsCOMPtr<nsILocalFile> library(do_QueryInterface(libraries));
  if (!library)
    return NS_ERROR_UNEXPECTED;

  library->SetNativeLeafName(NS_LITERAL_CSTRING("libraries"));
  library->AppendNative(NS_LITERAL_CSTRING("dummy"));

  // loop through and load dependent libraries
  for (char const *const *dependent = kDependentLibraries;
       *dependent;
       ++dependent) {
    library->SetNativeLeafName(nsDependentCString(*dependent));

    nsCString pathName;
    library->GetNativePath(pathName);

#ifdef DEBUG_PURPLE_LOADER
    printf("opening lib %s: ", pathName.get());
    PRBool exists;
    library->Exists(&exists);
    if (exists)
      printf("found ");
#endif

#ifdef XP_MACOSX
# ifdef DEBUG_PURPLE_LOADER
    void* lib = 
# endif
      (void *) NSAddImage(pathName.get(),
                          NSADDIMAGE_OPTION_RETURN_ON_ERROR |
                          NSADDIMAGE_OPTION_MATCH_FILENAME_BY_INSTALLNAME);
# ifdef DEBUG_PURPLE_LOADER
    printf(lib ? "ok\n" : "KO\n");
# endif
#else // not XP_MACOSX
    PRLibSpec libSpec;
    libSpec.type = PR_LibSpec_Pathname;
    libSpec.value.pathname = pathName.get();

# ifdef DEBUG_PURPLE_LOADER
    PRLibrary *lib = 
# endif
      PR_LoadLibraryWithFlags(libSpec, PR_LD_NOW|PR_LD_GLOBAL);
    //    rv = library->Load(&lib);
# ifdef DEBUG_PURPLE_LOADER
    rv = lib ? NS_OK : NS_ERROR_FAILURE;
    if (NS_FAILED(rv))
      printf("KO\n");
    else
      printf("ok\n");
# endif
#endif
    // 1) We don't care if this failed!
    // 2) We are going to leak this library. We don't care about that either.
  }

  library->SetNativeLeafName(NS_LITERAL_CSTRING(kRealComponent));

  PRLibrary *lib;
  rv = library->Load(&lib);
  if (NS_FAILED(rv)) {
    printf("couldn't load %s\n", kRealComponent);
    return rv;
  }

  nsGetModuleProc getmoduleproc = (nsGetModuleProc)
    PR_FindFunctionSymbol(lib, NS_GET_MODULE_SYMBOL);

  if (!getmoduleproc)
    return NS_ERROR_FAILURE;


#ifdef DEBUG_PURPLE_LOADER
  rv = getmoduleproc(aCompMgr, aLocation, aResult);
  if (NS_FAILED(rv))
    printf("GETMODULEPROC FAILED !!!!!!\n");
  else
    printf("getmoduleproc OK\n");
  return rv;
#endif

  return getmoduleproc(aCompMgr, aLocation, aResult);
}
