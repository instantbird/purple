#include "purplePref.h"
#include "purpleUtils.h"
#include <stdio.h>


NS_IMPL_ISUPPORTS1(purplePref, purpleIPref)

purplePref::purplePref()
{
#ifdef DEBUG_PURPLE_PREF
  printf("  creating purplePref\n");
#endif
  /* member initializers and constructor code */
}

void purplePref::Init(PurpleAccountOption *aOpt)
{
  mOpt = aOpt;
#ifdef DEBUG_PURPLE_PREF
  printf("   purplePref (type %i) %s initialized\n",
	 mOpt->type, mOpt->pref_name);
#endif
}

purplePref::~purplePref()
{
#ifdef DEBUG_PURPLE_PREF
  printf("  destructing purplePref\n");
#endif
  /* destructor code */
}

/* readonly attribute string name; */
NS_IMETHODIMP purplePref::GetName(char * *aName)
{
  if (!mOpt) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  return returnString(aName, mOpt->pref_name);
}

/* readonly attribute short type; */
NS_IMETHODIMP purplePref::GetType(PRInt16 *aType)
{
  if (!mOpt) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  *aType = mOpt->type;

  return NS_OK;
}

/* readonly attribute boolean masked (); */
NS_IMETHODIMP purplePref::GetMasked(PRBool *aMasked)
{
  if (!mOpt) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  *aMasked = mOpt->masked;

  return NS_OK;
}

/* boolean getBool (); */
NS_IMETHODIMP purplePref::GetBool(PRBool *_retval)
{
  if (!mOpt) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  if (mOpt->type != (PurplePrefType) typeBool) {
    return NS_ERROR_FAILURE;
  }

  *_retval = mOpt->default_value.boolean;
  return NS_OK;
}

/* long getInt (); */
NS_IMETHODIMP purplePref::GetInt(PRInt32 *_retval)
{
  if (!mOpt) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  if (mOpt->type != (PurplePrefType) typeInt) {
    return NS_ERROR_FAILURE;
  }

  *_retval = mOpt->default_value.integer;
  return NS_OK;
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* string getString (); */
NS_IMETHODIMP purplePref::GetString(char **_retval)
{
  if (!mOpt) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  if (mOpt->type != (PurplePrefType) typeString) {
    return NS_ERROR_FAILURE;
  }

  const char *res = mOpt->default_value.string;
  return returnString(_retval, res);
}
