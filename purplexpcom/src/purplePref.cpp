/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "purplePref.h"
#include "purpleGListEnumerator.h"
#include <nsIClassInfoImpl.h>
#include <nsIProgrammingLanguage.h>
#include <nsMemory.h>
#include <nsStringAPI.h>

#pragma GCC visibility push(default)
#include <libpurple/util.h>
#pragma GCC visibility pop

#ifdef PR_LOGGING
//
// NSPR_LOG_MODULES=purplePref:5
//
static PRLogModuleInfo *gPurplePrefLog = nullptr;
#endif
#define LOG(args) PR_LOG(gPurplePrefLog, PR_LOG_DEBUG, args)

class purpleKeyValuePair MOZ_FINAL : public prplIKeyValuePair,
                                     public nsIClassInfo
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSICLASSINFO
  NS_DECL_PRPLIKEYVALUEPAIR

  purpleKeyValuePair() {}
  void Init(const PurpleKeyValuePair *aPKVP)
  {
    mKey = aPKVP->key;
    mValue = (const char *)aPKVP->value;
  }

private:
  ~purpleKeyValuePair() {}

protected:
  /* additional members */
  nsCString mKey;
  nsCString mValue;
};

/* Implementation file */

// 175F24B2-DA42-4F24-80E0-CD3CB334EDED
#define PURPLE_KEY_VALUE_PAIR_CID                      \
  { 0x175F24B2, 0xDA42, 0x4F24,                        \
    { 0x80, 0xE0, 0xCD, 0x3C, 0xB3, 0x34, 0xED, 0xED } \
  }

NS_IMPL_CLASSINFO(purpleKeyValuePair, NULL, 0, PURPLE_KEY_VALUE_PAIR_CID)
NS_IMPL_THREADSAFE_CI(purpleKeyValuePair)
NS_IMPL_ISUPPORTS1_CI(purpleKeyValuePair, prplIKeyValuePair)

/* readonly attribute AUTF8String name; */
NS_IMETHODIMP purpleKeyValuePair::GetName(nsACString & aName)
{
  aName = mKey;
  return NS_OK;
}

/* readonly attribute AUTF8String value; */
NS_IMETHODIMP purpleKeyValuePair::GetValue(nsACString & aValue)
{
  aValue = mValue;
  return NS_OK;
}

NS_IMPL_CLASSINFO(purplePref, NULL, 0, PURPLE_PREF_CID)
NS_IMPL_THREADSAFE_CI(purplePref)
NS_IMPL_ISUPPORTS1_CI(purplePref, prplIPref)

purplePref::purplePref()
{
  /* member initializers and constructor code */
#ifdef PR_LOGGING
  if (!gPurplePrefLog)
    gPurplePrefLog = PR_NewLogModule("purplePref");
#endif
  LOG(("Constructing purplePref @%x", this));
}

void purplePref::Init(PurpleAccountOption *aOpt)
{
  mOpt = aOpt;
  LOG(("\tpurplePref (type %i) %s initialized",
       mOpt->type, mOpt->pref_name));
}

purplePref::~purplePref()
{
  /* destructor code */
  LOG(("Destructing purplePref @%x", this));
}

#define PURPLE_IMPL_PREF_GETTER(code)                   \
  {                                                     \
    NS_ENSURE_TRUE(mOpt, NS_ERROR_NOT_INITIALIZED);     \
                                                        \
    code;                                               \
    return NS_OK;                                       \
  }

/* readonly attribute AUTF8String name; */
NS_IMETHODIMP purplePref::GetName(nsACString& aName)
  PURPLE_IMPL_PREF_GETTER(aName = purple_account_option_get_setting(mOpt))

/* readonly attribute AUTF8String label; */
NS_IMETHODIMP purplePref::GetLabel(nsACString& aLabel)
  PURPLE_IMPL_PREF_GETTER(aLabel = purple_account_option_get_text(mOpt))

/* readonly attribute short type; */
NS_IMETHODIMP purplePref::GetType(PRInt16 *aType)
  PURPLE_IMPL_PREF_GETTER(*aType = purple_account_option_get_type(mOpt))

/* readonly attribute boolean masked (); */
NS_IMETHODIMP purplePref::GetMasked(bool *aMasked)
  PURPLE_IMPL_PREF_GETTER(*aMasked = purple_account_option_get_masked(mOpt))

#define PURPLE_IMPL_PREF_GET_VALUE(aType, code)                             \
  PURPLE_IMPL_PREF_GETTER(                                                  \
    NS_ENSURE_TRUE(purple_account_option_get_type(mOpt) ==                  \
                     (PurplePrefType) aType,                                \
                   NS_ERROR_FAILURE);                                       \
                                                                            \
    code;                                                                   \
  )

/* boolean getBool (); */
NS_IMETHODIMP purplePref::GetBool(bool *_retval)
  PURPLE_IMPL_PREF_GET_VALUE(typeBool,
    *_retval = purple_account_option_get_default_bool(mOpt))

/* long getInt (); */
NS_IMETHODIMP purplePref::GetInt(PRInt32 *_retval)
  PURPLE_IMPL_PREF_GET_VALUE(typeInt,
    *_retval = purple_account_option_get_default_int(mOpt))

/* AUTF8String getString (); */
NS_IMETHODIMP purplePref::GetString(nsACString& _retval)
  PURPLE_IMPL_PREF_GET_VALUE(typeString,
    _retval = purple_account_option_get_default_string(mOpt))

/* nsISimpleEnumerator getList (); */
NS_IMETHODIMP purplePref::GetList(nsISimpleEnumerator **_retval)
  PURPLE_IMPL_PREF_GET_VALUE(typeList,
    purpleGListEnumerator *enumerator = new purpleGListEnumerator();
    enumerator->Init(purple_account_option_get_list(mOpt),
                     purpleTypeToInterface<purpleKeyValuePair,
                                           prplIKeyValuePair,
                                           PurpleKeyValuePair>);
    NS_ADDREF(*_retval = enumerator))

/* AUTF8String getListDefault (); */
NS_IMETHODIMP purplePref::GetListDefault(nsACString& _retval)
  PURPLE_IMPL_PREF_GET_VALUE(typeList,
    _retval = purple_account_option_get_default_list_value(mOpt))
