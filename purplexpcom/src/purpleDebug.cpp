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

#include "purpleDebug.h"
#include <nsComponentManagerUtils.h>
#include <nsDirectoryServiceUtils.h>
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
static PRLogModuleInfo *gPurpleDebugLog = nsnull;
#endif
#define LOG(args) PR_LOG(gPurpleDebugLog, PR_LOG_DEBUG, args)

purpleDebug *purpleDebug::sInstance = nsnull;

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

    PRBool exists = PR_FALSE;
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
  NS_ENSURE_TRUE(Enabled(level, category), );

  nsCString cFile(file);
  sInstance->PrepareFilePath(cFile);

  if (level == PURPLE_DEBUG_INFO && sInstance->mLogLevel == level) {
    nsCString cInfo(arg);
    cInfo.AppendLiteral("Location: ");
    cInfo.Append(function);
    cInfo.AppendLiteral(", file: ");
    cInfo.Append(cFile);
    cInfo.AppendLiteral(", line: ");
    cInfo.AppendInt(line);
    ReportMessage(level, category, cInfo.get());
    return;
  }

  LOG(("pp-%s: %s, in %s, file: %s:%i", category, arg, function, file, line));

  nsString sourceName;
  NS_CStringToUTF16(cFile, NS_CSTRING_ENCODING_UTF8, sourceName);

  nsString msg;
  nsDependentCString message(arg);
  NS_CStringToUTF16(message, NS_CSTRING_ENCODING_UTF8, msg);

  nsString sourceLine;
  nsDependentCString cSourceLine(category);
  cSourceLine.Append(": ");
  cSourceLine.Append(function);
  NS_CStringToUTF16(cSourceLine, NS_CSTRING_ENCODING_UTF8, sourceLine);

  PRUint32 flags = 0;
  if (level < PURPLE_DEBUG_ERROR)
    flags |= nsIScriptError::warningFlag;
  if (level < PURPLE_DEBUG_WARNING)
    flags |= nsIScriptError::strictFlag;

  nsCOMPtr<nsIScriptError> error =
    do_CreateInstance(NS_SCRIPTERROR_CONTRACTID);
  NS_ENSURE_TRUE(error, );

  error->Init(msg.get(),
              sourceName.get(),
              sourceLine.get(), // let's use the function name as the code line
              line,
              0, // No column info, use 0
              flags,
              category); // Not sure what this is used for, if used at all...

  nsCOMPtr<nsIConsoleService> consoleService =
    do_GetService(NS_CONSOLESERVICE_CONTRACTID);
  NS_ENSURE_TRUE(consoleService, );

  consoleService->LogMessage(error);
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
  nsresult rv = NS_GetSpecialDirectory("resource:app",
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
