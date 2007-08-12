#include "nsXPCOM.h"

nsresult returnString(char * *aDest, const char *aStr);

#define PURPLE_ENSURE_INIT(mMember)		\
  if (!mMember) {				\
    return NS_ERROR_NOT_INITIALIZED;		\
  }
