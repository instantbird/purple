/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "purpleGetText.h"
#include <nsIChromeRegistry.h>
#include <nsICryptoHash.h>
#include <nsIObserverService.h>
#include <nsIStringBundle.h>
#include <nsServiceManagerUtils.h>
#include <nsComponentManagerUtils.h>
#include <nsNetCID.h>
#include <nsNetUtil.h>

#ifdef PR_LOGGING
//
// NSPR_LOG_MODULES=purpleGetText:5
//
static PRLogModuleInfo *gPurpleGetTextLog = nullptr;
#endif
#define LOG(args) PR_LOG(gPurpleGetTextLog, PR_LOG_DEBUG, args)

purpleGetText *purpleGetText::sInstance = nullptr;

NS_IMPL_ISUPPORTS1(purpleGetText, nsIObserver)

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
// 15: Icelandic
IMPL_PLURAL_RULE(15, n%10==1&&n%100!=11?0:1)

static const purpleGetText::PluralRule pluralRules[] = {
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
  {3, plural_rule_14},
  // 15: Icelandic
  {2, plural_rule_15}
};

#define PLURAL_RULES_COUNT ((PRInt32)(sizeof(pluralRules) / sizeof(PluralRule)))

const purpleGetText::PluralRule *purpleGetText::GetPluralRule()
{
  if (mPluralRule)
    return mPluralRule;

  nsresult rv;
  nsCOMPtr<nsIStringBundleService> stringService =
     do_GetService(NS_STRINGBUNDLE_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, nullptr);

  nsCOMPtr<nsIStringBundle> bundle;
  rv = stringService->CreateBundle("chrome://global/locale/intl.properties",
                                   getter_AddRefs(bundle));
  NS_ENSURE_SUCCESS(rv, nullptr);

  nsString result;
  rv = bundle->GetStringFromName(NS_LITERAL_STRING("pluralRule").get(), getter_Copies(result));
  NS_ENSURE_SUCCESS(rv, nullptr);

  PRInt32 ruleNumber = strtol(NS_ConvertUTF16toUTF8(result).get(), NULL, 10);
  NS_ENSURE_TRUE(ruleNumber >= 0 && ruleNumber < PLURAL_RULES_COUNT, nullptr);

  return mPluralRule = pluralRules + ruleNumber;
}

#define IN_RANGE(buf, start, end) (buf >= start && buf <= end)
#define BUF_SIZE 1024
const char *purpleGetText::GetTextIdentifier(const char *string)
{
  static char buffer[BUF_SIZE];

  nsCString str(string);
  ToLowerCase(str);

  // Replace non alphanumeric characters (except underscore) by spaces
  // Keep % symbols for now
  char *buf;
  for (buf = str.BeginWriting(); *buf; ++buf) {
    if (IN_RANGE(*buf, 'A', 'Z') ||
        IN_RANGE(*buf, 'a', 'z') ||
        IN_RANGE(*buf, '0', '9') ||
        *buf == '_' ||
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
  NS_ENSURE_SUCCESS(rv, nullptr);

  nsCString propFileName("chrome://");
  propFileName.Append(package);
  propFileName.Append("/locale/prpl.properties");

  LOG(("looking for %s, in %s\n", ident, propFileName.get()));

  nsCOMPtr<nsIStringBundle> bundle;
  rv = stringService->CreateBundle(propFileName.get(), getter_AddRefs(bundle));
  NS_ENSURE_SUCCESS(rv, nullptr);

  nsString result;
  rv = bundle->GetStringFromName(NS_ConvertUTF8toUTF16(ident).get(),
                                 getter_Copies(result));
  // We do not use NS_ENSURE_SUCCESS here because in case of a collision,
  // it's ok to have a string not found in the bundle, we will retry
  // with the hash of the string
  if (NS_FAILED(rv))
    return nullptr;

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
  if (*ident) {
    nsCString *result = GetStringFromProperties(package, ident);
    if (result)
      return result;
  }

  nsCOMPtr<nsICryptoHash> hash = do_CreateInstance(NS_CRYPTO_HASH_CONTRACTID);
  NS_ENSURE_TRUE(hash, nullptr);

  nsresult rv = hash->Init(nsICryptoHash::MD5);
  NS_ENSURE_SUCCESS(rv, nullptr);

  rv = hash->Update(reinterpret_cast<const PRUint8*>(string), strlen(string));
  NS_ENSURE_SUCCESS(rv, nullptr);

  nsAutoCString hashed;
  rv = hash->Finish(PR_FALSE, hashed);
  NS_ENSURE_SUCCESS(rv, nullptr);

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
    if (mStringCache[i].packageName.Equals(package)) {
      // If we have an entry but the hashtable isn't initialized,
      // it means that the translation bundle for this package is
      // missing. The error value will make the translation code
      // return early.
      if (!mStringCache[i].table.IsInitialized())
        return -1;
      return i;
    }

  // Create the new package
  LangStringCache *pack = mStringCache.AppendElement();
  pack->packageName = package;

  // check if the translation bundle exists.
  nsCString propFileName("chrome://");
  propFileName.Append(package);
  propFileName.Append("/locale/prpl.properties");

  nsCOMPtr<nsIURI> uri;
  nsresult rv = NS_NewURI(getter_AddRefs(uri), propFileName);
  NS_ENSURE_SUCCESS(rv, -1);

  nsCOMPtr<nsIChromeRegistry> chromeRegistry =
    do_GetService(NS_CHROMEREGISTRY_CONTRACTID);
  NS_ENSURE_TRUE(chromeRegistry, -1);

  nsCOMPtr<nsIURI> resolvedURI;
  rv = chromeRegistry->ConvertChromeURL(uri, getter_AddRefs(resolvedURI));
  if (NS_FAILED(rv)) {
    // this prpl doesn't provide a translation bundle
#ifdef DEBUG
    nsCString warning("Missing translation file: ");
    warning.Append(propFileName);
    NS_WARNING(warning.get());
#endif

    // We don't initialize the hashtable, so that next time we are
    // called for the same package we can detect it and return early

    // return -1 to indicate a failure.
    return -1;
  }

  // The translation file exists, init the hashtable and return the new index
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
  // We don't use NS_ENSURE_TRUE here so that we don't print a warning
  // each time a translation from a missing file is requested
  if (packageIndex == -1)
    return string;

  // check if we can get it from the hashtable
  nsTArray<nsCString> *array = nullptr;
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
    sInstance->mStringCache[packageIndex].table.Put(cString, nullptr);
#ifdef DEBUG
    nsCString warning(NS_LITERAL_CSTRING("caching the missingness of "));
    warning.Append(package);
    warning.Append('<');
    warning.Append(string);
    warning.Append('>');
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
  // We don't use NS_ENSURE_TRUE here so that we don't print a warning
  // each time a translation from a missing file is requested
  if (packageIndex == -1)
    return defaultResult;

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

NS_IMETHODIMP
purpleGetText::Observe(nsISupports *aSubject,
                       const char *aTopic,
                       const PRUnichar *aSomeData)
{
  LOG(("purpleGetText::Observer(%s)\n", aTopic));
  unInit();
  return NS_OK;
}

purpleGetText::purpleGetText()
  : mStringCache(10),
    mPluralRule(nullptr)
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
    NS_ADDREF(sInstance);

    nsCOMPtr<nsIObserverService> os =
      do_GetService("@mozilla.org/observer-service;1");
    if (os)
      os->AddObserver(sInstance, NS_CHROME_FLUSH_TOPIC, PR_FALSE);
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

  nsCOMPtr<nsIObserverService> os =
    do_GetService("@mozilla.org/observer-service;1");
  if (os)
    os->RemoveObserver(sInstance, NS_CHROME_FLUSH_TOPIC);

  LOG(("purpleGetText::unInit\n"));
  for (PRUint32 i = 0; i < sInstance->mStringCache.Length(); ++i)
    if (sInstance->mStringCache[i].table.IsInitialized())
      sInstance->mStringCache[i].table.Enumerate(deleteEnumerator, nullptr);

  NS_RELEASE(sInstance);
  sInstance = nullptr;
}
