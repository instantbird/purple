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
