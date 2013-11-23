/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <nsIObserver.h>
#include <nsStringAPI.h>

#pragma GCC visibility push(default)
#include <libpurple/debug.h>
#pragma GCC visibility pop

class purpleDebug MOZ_FINAL : public nsIObserver
{
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIOBSERVER

  static void ReportMessage(PurpleDebugLevel level, const char *category,
                            const char *arg);

  static void ReportMessageWithLocation(PurpleDebugLevel level,
                                        const char *category,
                                        const char *file, int line,
                                        const char *function,
                                        const char *arg);

  static gboolean Enabled(PurpleDebugLevel level,
                          const char *category);

 private:
  static nsresult init();
  purpleDebug(); // can only be called by init()
  void PrepareFilePath(nsCString &aPath);
  nsresult InitRepositoryInfo();

  static purpleDebug *sInstance;
  nsCString mSourceBase;
  nsCString mRepositoryBase;
  PRInt32 mBaseLength;
  PRInt32 mLogLevel;
  bool mModifiedSources;
};
