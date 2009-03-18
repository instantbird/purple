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

#include <nsStringAPI.h>
#include <nsDataHashtable.h>

class purpleGetText
{
 public:
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
    nsCString lang;
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
