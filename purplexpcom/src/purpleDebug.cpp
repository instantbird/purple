/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "purpleDebug.h"
#include "purpleAccountScoper.h"
#include <imIAccountsService.h>
#include <nsComponentManagerUtils.h>
#include <nsDirectoryServiceUtils.h>
#include <nsDirectoryServiceDefs.h>
#include <nsINIParser.h>
#include <nsServiceManagerUtils.h>
#include <nsIConsoleService.h>
#include <nsILocalFile.h>
#include <nsIObserverService.h>
#include <nsIPrefBranch2.h>
#include <nsIPrefService.h>
#include <nsIScriptError.h>

#define LOGLEVEL_PREF "purple.debug.loglevel"

#ifdef PR_LOGGING
//
// NSPR_LOG_MODULES=purpleDebug:5
//
static PRLogModuleInfo *gPurpleDebugLog = nullptr;
#endif
#define LOG(args) PR_LOG(gPurpleDebugLog, PR_LOG_DEBUG, args)

purpleDebug *purpleDebug::sInstance = nullptr;

NS_IMPL_ISUPPORTS1(purpleDebug, nsIObserver)

void purpleDebug::ReportMessage(PurpleDebugLevel level, const char *category,
                                const char *arg)
{
  LOG(("pp-%s: %s", category, arg));

  NS_ENSURE_TRUE(Enabled(level, category), );

  nsCOMPtr<nsIConsoleService> consoleService =
    do_GetService(NS_CONSOLESERVICE_CONTRACTID);
  NS_ENSURE_TRUE(consoleService, );

  nsCString message(category);
  message.Append(": ");
  message.Append(arg);

  nsString msg;
  NS_CStringToUTF16(message, NS_CSTRING_ENCODING_UTF8, msg);

  consoleService->LogStringMessage(msg.get());
}

void
purpleDebug::PrepareFilePath(nsCString &aPath)
{
  NS_ENSURE_TRUE(mBaseLength != -1, );

  if (mModifiedSources) {
    // If the sources were locally modified, we are likely to have
    // compiled the application locally so the source file may exist on
    // the disk. If it exists, we don't want to replace the local path
    // by a mercurial URL.
    nsCOMPtr<nsILocalFile> localFile =
      do_CreateInstance(NS_LOCAL_FILE_CONTRACTID);
    NS_ENSURE_TRUE(localFile, );

#ifdef XP_WIN
    const char *begin, *end;
    PRUint32 len = aPath.BeginReading(&begin, &end);
    nsCString native;
    char *dest;
    NS_CStringGetMutableData(native, len, &dest);

    for (; begin < end; ++begin, ++dest) {
      *dest = *begin == '/' ? '\\' : *begin;
    }

    nsresult rv = localFile->InitWithNativePath(native);
#else
    nsresult rv = localFile->InitWithNativePath(aPath);
#endif
    NS_ENSURE_SUCCESS(rv, );

    bool exists = false;
    rv = localFile->Exists(&exists);
    NS_ENSURE_SUCCESS(rv, );
    if (exists) {
      aPath.Insert("file://", 0);
      return;
    }
  }

  if (StringBeginsWith(aPath, mSourceBase))
    aPath.Replace(0, mBaseLength, mRepositoryBase);
}

void
purpleDebug::ReportMessageWithLocation(PurpleDebugLevel level,
                                       const char *category,
                                       const char *file, int line,
                                       const char *function,
                                       const char *arg)
{
  LOG(("pp-%s: %s, in %s, file: %s:%i", category, arg, function, file, line));

  nsCString cFile(file);
  sInstance->PrepareFilePath(cFile);

  nsDependentCString cSourceLine(category);
  cSourceLine.Append(": ");
  cSourceLine.Append(function);

  PRUint32 flags = 0;
  if (level < PURPLE_DEBUG_ERROR)
    flags |= nsIScriptError::warningFlag;
  if (level < PURPLE_DEBUG_WARNING)
    flags |= nsIScriptError::strictFlag;

  nsCOMPtr<nsIScriptError> error =
    do_CreateInstance(NS_SCRIPTERROR_CONTRACTID);
  NS_ENSURE_TRUE(error, );

  error->Init(NS_ConvertUTF8toUTF16(arg),
              NS_ConvertUTF8toUTF16(cFile),
              NS_ConvertUTF8toUTF16(cSourceLine), // let's use the function name as the code line
              line,
              0, // No column info, use 0
              flags,
              category); // Not sure what this is used for, if used at all...

  if (Enabled(level, category)) {
    if (level == PURPLE_DEBUG_INFO && sInstance->mLogLevel == level) {
      nsCString cInfo(arg);
      cInfo.AppendLiteral("Location: ");
      cInfo.Append(function);
      cInfo.AppendLiteral(", file: ");
      cInfo.Append(cFile);
      cInfo.AppendLiteral(", line: ");
      cInfo.AppendInt(line);
      ReportMessage(level, category, cInfo.get());
    }
    else {
      nsCOMPtr<nsIConsoleService> consoleService =
        do_GetService(NS_CONSOLESERVICE_CONTRACTID);
      NS_ENSURE_TRUE(consoleService, );

      consoleService->LogMessage(error);
    }
  }

  PRUint32 accountId = purpleAccountScoper::GetCurrentAccountId();
  if (!accountId)
    return;

  nsCOMPtr<imIAccountsService> accountsService =
    do_GetService("@mozilla.org/chat/accounts-service;1");
  NS_ENSURE_TRUE(accountsService, );

  nsCOMPtr<imIAccount> account;
  nsresult rv = accountsService->GetAccountByNumericId(accountId,
                                                       getter_AddRefs(account));
  NS_ENSURE_SUCCESS(rv, );
  NS_ENSURE_TRUE(account, );

  rv = account->LogDebugMessage(error, level);
  NS_ENSURE_SUCCESS(rv, );
}

gboolean
purpleDebug::Enabled(PurpleDebugLevel level,
                     const char *category)
{
  nsresult rv = init();
  NS_ENSURE_SUCCESS(rv, FALSE);

  return level >= sInstance->mLogLevel;
}

NS_IMETHODIMP
purpleDebug::Observe(nsISupports *aSubject,
                     const char *aTopic,
                     const PRUnichar *aSomeData)
{
  LOG(("purpleDebug::Observer(%s)", aTopic));

  if (!strcmp(aTopic, NS_PREFBRANCH_PREFCHANGE_TOPIC_ID)) {
    nsCOMPtr<nsIPrefBranch> prefBranch = do_QueryInterface(aSubject);
    if (prefBranch)
      prefBranch->GetIntPref(LOGLEVEL_PREF, &mLogLevel);
  }

  return NS_OK;
}

nsresult
purpleDebug::InitRepositoryInfo()
{
  nsCOMPtr<nsIFile> folder;
  nsresult rv = NS_GetSpecialDirectory(NS_XPCOM_CURRENT_PROCESS_DIR,
                                       getter_AddRefs(folder));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsILocalFile> appIni = do_QueryInterface(folder);
  NS_ENSURE_TRUE(appIni, rv);

  rv = appIni->AppendNative(NS_LITERAL_CSTRING("application.ini"));
  NS_ENSURE_SUCCESS(rv, rv);

  nsINIParser parser;
  rv = parser.Init(appIni);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = parser.GetString("App", "SourceRepository", mRepositoryBase);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCString sourceStamp;
  rv = parser.GetString("App", "SourceStamp", sourceStamp);
  NS_ENSURE_SUCCESS(rv, rv);

  // See if there were local modifications to the source files when building
  if (sourceStamp[sourceStamp.Length() - 1] == '+') {
    mModifiedSources = PR_TRUE;
    sourceStamp.SetLength(sourceStamp.Length() - 1);
  }
  mRepositoryBase.AppendLiteral("raw-file/");
  mRepositoryBase.Append(sourceStamp);

  return NS_OK;
}

purpleDebug::purpleDebug()
  : mSourceBase(__FILE__),
    mLogLevel(PURPLE_DEBUG_WARNING),
    mModifiedSources(PR_FALSE)
{
  mBaseLength = mSourceBase.RFind("/purple/purplexpcom/src/purpleDebug.cpp");

  if (mBaseLength != -1) {
    mSourceBase.SetLength(mBaseLength);

    // We have the source base path, now get the repository path
    InitRepositoryInfo();
  }

  // init our pref and observer
  nsCOMPtr<nsIPrefBranch2> prefBranch =
    do_GetService(NS_PREFSERVICE_CONTRACTID);
  NS_ENSURE_TRUE(prefBranch, );

  prefBranch->AddObserver(LOGLEVEL_PREF, this, PR_FALSE);
  prefBranch->GetIntPref(LOGLEVEL_PREF, &mLogLevel);
}

nsresult purpleDebug::init()
{
#ifdef PR_LOGGING
  if (!gPurpleDebugLog)
    gPurpleDebugLog = PR_NewLogModule("purpleDebug");
#endif

#ifdef DEBUG
  if (g_getenv("PURPLE_VERBOSE_DEBUG"))
    purple_debug_set_verbose(TRUE);
#endif

  if (!sInstance) {
    LOG(("creating a new instance of purpleDebug"));
    sInstance = new purpleDebug();
    NS_ENSURE_TRUE(sInstance, NS_ERROR_OUT_OF_MEMORY);
  }

  return NS_OK;
}
