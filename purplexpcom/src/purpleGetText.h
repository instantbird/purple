/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <nsStringAPI.h>
#include <nsDataHashtable.h>
#include <nsIObserver.h>

class purpleGetText MOZ_FINAL : public nsIObserver
{
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIOBSERVER

  static const char *GetText(const char *package,
                             const char *string);
  static const char *GetPluralText(const char *package,
                                   const char *singular,
                                   const char *plural,
                                   unsigned long int number);

  static nsresult init();
  static void unInit();

  typedef PRUint32 (*PluralFormIndexCallback) (unsigned long int n);

  struct PluralRule {
    PRUint32 nbForms;
    PluralFormIndexCallback func;
  };

 private:
  // can only be called by init();
  purpleGetText();

  static purpleGetText *sInstance;

  struct LangStringCache {
    nsCString packageName;
    nsDataHashtable<nsCStringHashKey, nsTArray<nsCString> *> table;
  };

  nsTArray<LangStringCache> mStringCache;

  const PluralRule *mPluralRule;

  // returns the plural rule that is to be used.
  // chrome://global/locale/intl.properties is used to decide which one is right
  const PluralRule *GetPluralRule();

  // Compute the identifier for a string.
  // This takes any string as input, and returns a camelcased
  // identifier derived from it.
  const char *GetTextIdentifier(const char *string);

  // Get a string from a .properties file.
  nsCString *GetStringFromProperties(const char *package,
                                     const char *ident);

  // Get a string from a package.  Attempt first with only the identifier;
  // if it fails, retry with the 8 first hexadecimal digits of the md5
  // hash of the string appended to the identifier
  nsCString *GetString(const char *package, const char *string);

  // the package here is for example 'purple' or 'oscar'
  // this returns the index of the package in the mStringCache array
  PRInt32 GetPackageIndex(const char *package);
};
