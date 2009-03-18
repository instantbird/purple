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
 * 2009.
 *
 * The Initial Developer of the Original Code is
 * Florian QUEZE <florian@instantbird.org>.
 * Portions created by the Initial Developer are Copyright (C) 2009
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

#include "purpleGetText.h"
#include <nsICryptoHash.h>
#include <nsIStringBundle.h>
#include <nsServiceManagerUtils.h>
#include <nsComponentManagerUtils.h>
#include <nsNetCID.h>

#ifdef PR_LOGGING
//
// NSPR_LOG_MODULES=purpleGetText:5
//
static PRLogModuleInfo *gPurpleGetTextLog = nsnull;
#endif
#define LOG(args) PR_LOG(gPurpleGetTextLog, PR_LOG_DEBUG, args)

purpleGetText *purpleGetText::sInstance = nsnull;

#define IMPL_PLURAL_RULE(aNumber, aRule)                        \
  static PRUint32 plural_rule_##aNumber(unsigned long int n)    \
  {                                                             \
    return aRule;                                               \
  }

// 0: Chinese
IMPL_PLURAL_RULE(0, 0)
// 1: English
IMPL_PLURAL_RULE(1, n!=1?1:0)
// 2: French
IMPL_PLURAL_RULE(2, n>1?1:0)
// 3: Latvian
IMPL_PLURAL_RULE(3, n%10==1&&n%100!=11?1:n!=0?2:0)
// 4: Scottish Gaelic
IMPL_PLURAL_RULE(4, n==1?0:n==2?1:2)
// 5: Romanian
IMPL_PLURAL_RULE(5, n==1?0:n==0||(n%100>0&&n%100<20)?1:2)
// 6: Lithuanian
IMPL_PLURAL_RULE(6, n%10==1&&n%100!=11?0:n%10>=2&&(n%100<10||n%100>=20)?2:1)
// 7: Russian
IMPL_PLURAL_RULE(7, n%10==1&&n%100!=11?0:n%10>=2&&n%10<=4&&(n%100<10||n%100>=20)?1:2)
// 8: Slovak
IMPL_PLURAL_RULE(8, n==1?0:n>=2&&n<=4?1:2)
// 9: Polish
IMPL_PLURAL_RULE(9, n==1?0:n%10>=2&&n%10<=4&&(n%100<10||n%100>=20)?1:2)
// 10: Slovenian
IMPL_PLURAL_RULE(10, n%100==1?0:n%100==2?1:n%100==3||n%100==4?2:3)
// 11: Irish Gaeilge
IMPL_PLURAL_RULE(11, n==1?0:n==2?1:n>=3&&n<=6?2:n>=7&&n<=10?3:4)
// 12: Arabic
IMPL_PLURAL_RULE(12, n==0?5:n==1?0:n==2?1:n%100>=3&&n%100<=10?2:n%100>=11&&n%100<=99?3:4)
// 13: Maltese
IMPL_PLURAL_RULE(13, n==1?0:n==0||(n%100>0&&n%100<=10)?1:n%100>10&&n%100<20?2:3)
// 14: Macedonian
IMPL_PLURAL_RULE(14, n%10==1?0:n%10==2?1:2)

static const purpleGetText::PluralRule pluralRules[15] = {
  // 0: Chinese
  {1, plural_rule_0},
  // 1: English
  {2, plural_rule_1},
  // 2: French
  {2, plural_rule_2},
  // 3: Latvian
  {3, plural_rule_3},
  // 4: Scottish Gaelic
  {3, plural_rule_4},
  // 5: Romanian
  {3, plural_rule_5},
  // 6: Lithuanian
  {3, plural_rule_6},
  // 7: Russian
  {3, plural_rule_7},
  // 8: Slovak
  {3, plural_rule_8},
  // 9: Polish
  {3, plural_rule_9},
  // 10: Slovenian
  {4, plural_rule_10},
  // 11: Irish Gaeilge
  {5, plural_rule_11},
  // 12: Arabic
  {6, plural_rule_12},
  // 13: Maltese
  {4, plural_rule_13},
  // 14: Macedonian
  {3, plural_rule_14}
};

#define LAST_PLURAL_RULE 14

const purpleGetText::PluralRule *purpleGetText::GetPluralRule()
{
  if (mPluralRule)
    return mPluralRule;

  nsresult rv;
  nsCOMPtr<nsIStringBundleService> stringService = 
     do_GetService(NS_STRINGBUNDLE_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, nsnull);

  nsCOMPtr<nsIStringBundle> bundle;
  rv = stringService->CreateBundle("chrome://global/locale/intl.properties",
                                   getter_AddRefs(bundle));
  NS_ENSURE_SUCCESS(rv, nsnull);

  nsString result;
  rv = bundle->GetStringFromName(NS_LITERAL_STRING("pluralRule").get(), getter_Copies(result));
  NS_ENSURE_SUCCESS(rv, nsnull);

  PRInt32 ruleNumber = strtol(NS_ConvertUTF16toUTF8(result).get(), NULL, 10);
  NS_ENSURE_TRUE(ruleNumber < 0 || ruleNumber > LAST_PLURAL_RULE, nsnull);

  return mPluralRule = pluralRules + ruleNumber;
}

#define IN_RANGE(buf, start, end) (buf >= start && buf <= end)
#define BUF_SIZE 1024
const char *purpleGetText::GetTextIdentifier(const char *string)
{
  static char buffer[BUF_SIZE];

  nsCString str(string);
  ToLowerCase(str);

  // Replace non alphanumeric characters by spaces
  // Keep % symbols for now
  char *buf;
  for (buf = str.BeginWriting(); *buf; ++buf) {
    if (IN_RANGE(*buf, 'A', 'Z') ||
        IN_RANGE(*buf, 'a', 'z') ||
        IN_RANGE(*buf, '0', '9') ||
        *buf == '%')
      continue;
    *buf = ' ';
  }

  // Replace the words begining with % or 0x by spaces
  for (buf = str.BeginWriting(); *buf; ++buf) {
    if (*buf == '%' || (*buf == '0' && buf[1] == 'x')) {
      do {
        *buf = ' ';
        ++buf;
      } while (*buf && *buf != ' ');
      if (!*buf)
        break;
    }
  }

  const char *buf2 = str.BeginReading();
  PRUint32 bufferPosition = 0;
  PRUint32 maxWord = 7 - 1; // 7 words

  // Ignore leading spaces
  while (*buf2 && *buf2 == ' ')
    ++buf2;

  // Copy the first word
  while (*buf2 && *buf2 != ' ') {
    buffer[bufferPosition++] = *buf2;
    ++buf2;
  }

  // Copy the following words, using uppercase for the first letter
  while (*buf2 && maxWord) {
    while (*buf2 && *buf2 == ' ')
      ++buf2;

    if (bufferPosition < BUF_SIZE && IN_RANGE(*buf2, 'a', 'z')) {
      buffer[bufferPosition++] = *buf2 + 'A' - 'a';
      ++buf2;
    }
    while (bufferPosition < BUF_SIZE && *buf2 && *buf2 != ' ') {
      buffer[bufferPosition++] = *buf2;
      ++buf2;
    }
    --maxWord;
  }

  if (bufferPosition < BUF_SIZE)
    buffer[bufferPosition] = 0;
  else
    buffer[BUF_SIZE - 1] = 0;

  LOG(("identifier for \"%s\" is \"%s\"\n", string, buffer));
  return buffer;
}

nsCString *purpleGetText::GetStringFromProperties(const char *package,
                                                  const char *ident)
{
  nsresult rv;
  nsCOMPtr<nsIStringBundleService> stringService = 
     do_GetService(NS_STRINGBUNDLE_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, nsnull);

  nsCString propFileName("chrome://");
  propFileName.Append(package);
  propFileName.Append("/locale/prpl.properties");

  LOG(("looking for %s, in %s\n", ident, propFileName.get()));

  nsCOMPtr<nsIStringBundle> bundle;
  rv = stringService->CreateBundle(propFileName.get(), getter_AddRefs(bundle));
  NS_ENSURE_SUCCESS(rv, nsnull);

  nsString result;
  rv = bundle->GetStringFromName(NS_ConvertUTF8toUTF16(ident).get(),
                                 getter_Copies(result));
  // We do not use NS_ENSURE_SUCCESS here because in case of a collision,
  // it's ok to have a string not found in the bundle, we will retry
  // with the hash of the string
  if (NS_FAILED(rv))
    return nsnull;

  nsCString *cResult = new nsCString();
  CopyUTF16toUTF8(result, *cResult);
  LOG(("OK, found: %s\n", cResult->get()));

  return cResult;
}

// Get a string from a package.  Attempt first with only the identifier;
// if it fails, retry with the 8 first hexadecimal digits of the md5
// hash of the string appended to the identifier
nsCString *purpleGetText::GetString(const char *package,
                                    const char *string)
{
  LOG(("GetString(package = %s, %s)", package, string));

  const char *ident = GetTextIdentifier(string);

  nsCString *result = GetStringFromProperties(package, ident);
  if (result)
    return result;

  nsCOMPtr<nsICryptoHash> hash = do_CreateInstance(NS_CRYPTO_HASH_CONTRACTID);
  NS_ENSURE_TRUE(hash, nsnull);

  nsresult rv = hash->Init(nsICryptoHash::MD5);
  NS_ENSURE_SUCCESS(rv, nsnull);

  rv = hash->Update(reinterpret_cast<const PRUint8*>(string), strlen(string));
  NS_ENSURE_SUCCESS(rv, nsnull);

  nsCAutoString hashed;
  rv = hash->Finish(PR_FALSE, hashed);
  NS_ENSURE_SUCCESS(rv, nsnull);
 
#define NB_HEXA_DIGIT 4
  PRUint8 buf[NB_HEXA_DIGIT];
  memcpy(buf, hashed.BeginReading(), NB_HEXA_DIGIT);

  char *ident2 = const_cast<char *>(ident + strlen(ident));
  for (PRInt32 i = 0; i < NB_HEXA_DIGIT; ++i) {
    const char *hexDigit = "0123456789abcdef";
    PRUint8 d = buf[i] / 16;
    ident2[2 * i] = hexDigit[d];
    d = buf[i] & 15;
    ident2[2 * i + 1] = hexDigit[d];
  }
  ident2[NB_HEXA_DIGIT * 2] = 0;
  LOG(("hashed string = %s\n", ident));

  return GetStringFromProperties(package, ident);
}

PRInt32 purpleGetText::GetPackageIndex(const char *package)
{
  // Attempt to find the index
  for (PRUint32 i = 0; i < mStringCache.Length(); ++i)
    if (mStringCache[i].lang.Equals(package))
      return i;

  // Create the new package
  LangStringCache *pack = mStringCache.AppendElement();
  pack->lang = package;
  pack->table.Init();
  return mStringCache.Length() - 1;
}

// Used as a Uiop function to replace gettext
const char *purpleGetText::GetText(const char *package,
                                   const char *string)
{
  if (!sInstance) {
    nsresult rv = purpleGetText::init();
    NS_ENSURE_SUCCESS(rv, string);
  }

  LOG(("purple_get_text(package = %s, %s)", package, string));
  PRInt32 packageIndex = sInstance->GetPackageIndex(package);
  NS_ENSURE_TRUE(packageIndex != -1, string);

  // check if we can get it from the hashtable
  nsTArray<nsCString> *array = nsnull;
  nsDependentCString cString(string);
  if (sInstance->mStringCache[packageIndex].table.Get(cString, &array)) {
    // Check if we cached that the string is missing
    if (!array)
      return string;
    return (*array)[0].get();
  }

  nsCString *result = sInstance->GetString(package, string);
  // If the localized string doesn't exist, store this info in the hashtable
  if (!result) {
    sInstance->mStringCache[packageIndex].table.Put(cString, nsnull);
#ifdef DEBUG
    nsCString warning(NS_LITERAL_CSTRING("caching the missingness of "));
    warning.Append(package);
    warning.Append(':');
    warning.Append(string);
    NS_WARNING(warning.get());
#endif
    return string;
  }

  // Keep the localized string in the hashtable
  array = new nsTArray<nsCString>(1);
  if (!array) {
    NS_WARNING("Failed to create nsTArray<nsCString>(1)");
    delete result;
    return string;
  }

  array->AppendElement(*result);
  sInstance->mStringCache[packageIndex].table.Put(cString, array);
  delete result;

  return (*array)[0].get();
}

// Uiop function to replace ngettext
const char *purpleGetText::GetPluralText(const char *package,
                                         const char *singular,
                                         const char *plural,
                                         unsigned long int number)
{
  LOG(("purple_get_plural_text(package = %s, %s, %s, %i)",
       package, singular, plural, number));
  const char *defaultResult = (number == 1 ? singular : plural);

  if (!sInstance) {
    nsresult rv = purpleGetText::init();
    NS_ENSURE_SUCCESS(rv, defaultResult);
  }

  if (!sInstance->mPluralRule) {
    sInstance->GetPluralRule();
    NS_ENSURE_TRUE(sInstance->mPluralRule, defaultResult);
  }

  PRUint32 form = sInstance->mPluralRule->func(number);
  NS_ENSURE_TRUE(form < sInstance->mPluralRule->nbForms, defaultResult);

  PRInt32 packageIndex = sInstance->GetPackageIndex(package);
  NS_ENSURE_TRUE(packageIndex != -1, defaultResult);

  // check if we can get it from the hashtable
  nsTArray<nsCString> *array;
  nsDependentCString cString(singular);
  if (sInstance->mStringCache[packageIndex].table.Get(cString, &array)) {
    // Check if we cached that the string is missing
    if (!array)
      return defaultResult;

    NS_ENSURE_TRUE(array->Length() == sInstance->mPluralRule->nbForms,
                   defaultResult);
    return (*array)[form].get();
  }

  nsCString *result = sInstance->GetString(package, singular);
  NS_ENSURE_TRUE(result, defaultResult);

  // Keep the localized string in the hashtable
  array = new nsTArray<nsCString>(sInstance->mPluralRule->nbForms);
  if (!array) {
    NS_WARNING("Failed to create nsTArray<nsCString>(nbForms)");
    delete result;
    return defaultResult;
  }

  ParseString(*result, ';', *array);
  delete result;

  if (array->Length() != sInstance->mPluralRule->nbForms) {
    NS_WARNING("unexpected number of plural forms in the localized string");
    delete array;
    return defaultResult;
  }

  sInstance->mStringCache[packageIndex].table.Put(cString, array);
  return (*array)[form].get();
}

purpleGetText::purpleGetText()
  : mStringCache(10),
    mPluralRule(nsnull)
{
}

nsresult purpleGetText::init()
{
#ifdef PR_LOGGING
  if (!gPurpleGetTextLog)
    gPurpleGetTextLog = PR_NewLogModule("purpleGetText");
#endif

  if (!sInstance) {
    LOG(("creating a new instance of purpleGetText\n"));
    sInstance = new purpleGetText();
    NS_ENSURE_TRUE(sInstance, NS_ERROR_OUT_OF_MEMORY);
  }

  return NS_OK;
}

// Used to delete all the elements of a nsDataHashtable
static PLDHashOperator deleteEnumerator(const nsACString &aKey,
                                        nsTArray<nsCString> *&aArray,
                                        void *aUserArg)
{
  delete aArray;

  return (PLDHashOperator) (PL_DHASH_REMOVE | PL_DHASH_NEXT);
}

void purpleGetText::unInit()
{
  if (!sInstance)
    return;

  LOG(("purpleGetText::unInit\n"));
  for (PRUint32 i = 0; i < sInstance->mStringCache.Length(); ++i)
    sInstance->mStringCache[i].table.Enumerate(deleteEnumerator, nsnull);
    
  delete sInstance;
  sInstance = nsnull;
}
