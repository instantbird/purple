#include "purpleUtils.h"
#include<stdio.h>
#include<string.h>

nsresult returnString(char * *aDest, const char *aStr)
{
#ifdef DEBUG_PURPLE_UTILS
  printf("returnString(%s)\n", aStr);
#endif
  if (!aStr)
    return NS_ERROR_NULL_POINTER;

  size_t len = strlen(aStr) + 1;

  *aDest = (char*)NS_Alloc(len);
  if (!*aDest) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  memcpy(*aDest, aStr, len);

  return NS_OK;
}
