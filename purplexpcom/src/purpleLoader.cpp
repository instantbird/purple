#include "nscore.h"
#include "nsModule.h"
#include "prlink.h"
#include "nsILocalFile.h"

// very dirty work around for the error:
// nsStringAPI.h:953: error: size of arg" is negative array
#undef HAVE_CPP_2BYTE_WCHAR_T
#include "nsStringAPI.h"

#include "nsCOMPtr.h"

#define PRPL_PREFIX ""

static char const *const kDependentLibraries[] =
  {
    // dependent1.dll on windows, libdependent1.so on linux

    "libpurple" DLL_SUFFIX,
#if 0
    PRPL_PREFIX "autoaccept" DLL_SUFFIX,
    PRPL_PREFIX "buddynote" DLL_SUFFIX,
    PRPL_PREFIX "dbus-example" DLL_SUFFIX,
    PRPL_PREFIX "idle" DLL_SUFFIX,
    PRPL_PREFIX "joinpart" DLL_SUFFIX,
    PRPL_PREFIX "liboscar" DLL_SUFFIX,
    PRPL_PREFIX "libaim" DLL_SUFFIX,
    PRPL_PREFIX "libgg" DLL_SUFFIX,
    PRPL_PREFIX "libicq" DLL_SUFFIX,
    PRPL_PREFIX "libirc" DLL_SUFFIX,
    PRPL_PREFIX "libjabber" DLL_SUFFIX,
    PRPL_PREFIX "libmsn" DLL_SUFFIX,
    PRPL_PREFIX "libnovell" DLL_SUFFIX,
    PRPL_PREFIX "libqq" DLL_SUFFIX,
    PRPL_PREFIX "libsilcpurple" DLL_SUFFIX,
    PRPL_PREFIX "libsimple" DLL_SUFFIX,
    PRPL_PREFIX "libxmpp" DLL_SUFFIX,
    PRPL_PREFIX "libyahoo" DLL_SUFFIX,
    PRPL_PREFIX "libzephyr" DLL_SUFFIX,
    PRPL_PREFIX "log_reader" DLL_SUFFIX,
    PRPL_PREFIX "newline" DLL_SUFFIX,
    PRPL_PREFIX "offlinemsg" DLL_SUFFIX,
    PRPL_PREFIX "psychic" DLL_SUFFIX,
    PRPL_PREFIX "ssl-gnutls" DLL_SUFFIX,
    PRPL_PREFIX "ssl-nss" DLL_SUFFIX,
    PRPL_PREFIX "ssl" DLL_SUFFIX,
    PRPL_PREFIX "statenotify" DLL_SUFFIX,
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
    PRLibrary *lib;
#ifdef DEBUG_PURPLE_LOADER
    printf("opening lib %s: ", *dependent);
    PRBool exists;
    library->Exists(&exists);
    if (exists)
      printf("found ");
#endif
    rv = library->Load(&lib);
#ifdef DEBUG_PURPLE_LOADER
    if (NS_FAILED(rv))
      printf("KO\n");
    else
      printf("ok\n");
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
