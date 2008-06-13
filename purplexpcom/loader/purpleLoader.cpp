/* ***** BEGIN LICENSE BLOCK *****
 * The original file is licensed under the MIT License
 *
 * Copyright (c) 2005 Benjamin Smedberg <benjamin@smedbergs.us>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * Modifications are copyright (c) 2007 Florian QUEZE <florian@instantbird.org>
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
//	"zlib1" DLL_SUFFIX,
//	"iconv" DLL_SUFFIX,
	 "xml2" DLL_SUFFIX,
//     "intl" DLL_SUFFIX,
	  "glib" DLL_SUFFIX,
//      "libgmodule-2.0-0" DLL_SUFFIX,
//	  "libgthread-2.0-0" DLL_SUFFIX,
       "purple" DLL_SUFFIX,
//        "liboscar" DLL_SUFFIX,
//        "libjabber" DLL_SUFFIX,
//		"dnssd" DLL_SUFFIX,
//		"libmeanwhile-1" DLL_SUFFIX,
//		"libsilc-1-1-2" DLL_SUFFIX,
//		"libsilcclient-1-1-2" DLL_SUFFIX,
#endif
#ifdef XP_MACOSX
        // libz and libiconv are in the system, we don't need to load them
        //    DLL_PREFIX "z.1" DLL_SUFFIX,
        //    DLL_PREFIX "iconv.2" DLL_SUFFIX,
    DLL_PREFIX "xml2" DLL_SUFFIX,
    DLL_PREFIX "glib" DLL_SUFFIX,
    DLL_PREFIX "purple" DLL_SUFFIX,
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
