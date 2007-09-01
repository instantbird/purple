#include<nsCOMPtr.h>
#include "nsTArray.h"
#include <glib.h>
#include <libpurple/eventloop.h>
// very dirty work around for the error:
// nsStringAPI.h:953: error: size of arg" is negative array
#undef HAVE_CPP_2BYTE_WCHAR_T
#include<nsStringAPI.h>
#include"purpleISocket.h"

//#define DEBUG_PURPLE_SOCKET

class nsASocketHandler;
struct PRFileDesc;

#include "include/nsSocketTransportService2.h"

// 2e8f828f-2b2f-4a82-9efe-3cf8d983fbbf
#define PURPLE_SOCKET_CID \
{ 0x2e8f828f, 0x2b2f, 0x4a82, \
  { 0x9e, 0xfe, 0x3c, 0xf8, 0xd9, 0x83, 0xfb, 0xbf } \
 }

#define PURPLE_SOCKET_CONTRACTID \
    "@instantbird.org/purple/socket;1"

class purpleSocketInternal : public nsASocketHandler,
  public nsIRunnable
{
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIRUNNABLE

  nsresult Init(PRFileDesc *aFd, purpleISocket *aProxy, PRUint16 aPollFlags);
  void OnSocketReady(PRFileDesc*, PRInt16);
  void OnSocketDetached(PRFileDesc*);
  nsresult PostEvent();
  nsresult Cancel();
  PRBool GetCanceled() { return mCanceled; }
#ifdef DEBUG_PURPLE_SOCKET
  purpleSocketInternal(PRUint32 aId);
#else
  purpleSocketInternal();
#endif
  ~purpleSocketInternal();
  nsresult SetPollFlags(PRInt16 aPollFlags);
#ifdef DEBUG_PURPLE_SOCKET
  PRUint32 mId;
#endif

 private:
  nsresult OnAttach();
  PRFileDesc *mFd;
  PRInt16 mPollFlagsInternal;
  nsCOMPtr<purpleISocket> mProxy;
  PRBool mAttached;
  PRBool mDetached;
  PRBool mCanceled;
};

class purpleSocket : public purpleISocket
{
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_PURPLEISOCKET

  purpleSocket();
  ~purpleSocket();
 private:
  static PRUint32 sLastSocket;
  PRUint32 mId;
  PRInt32 mFd;
  PurpleInputFunction mFunction;
  gpointer mData;
  PurpleInputCondition mCondition;
  purpleSocketInternal *mInternal;
#ifdef DEBUG_PURPLE_SOCKET
  static PRUint32 sInstances;
#endif
};

typedef void (purpleSocket:: *purpleSocketFunc)(void);

class purpleSocketWatcher
{
 public:
  static PRUint32 AddWatch(gint aFd, PurpleInputCondition aCondition,
			   PurpleInputFunction aFunction, gpointer aData);
  static PRBool CancelWatch(PRUint32 aId);
  static nsPISocketTransportService *getSts();

 private:
  static nsTArray<purpleISocket *> sSockets;
  static nsCOMPtr<nsPISocketTransportService> sSts;
};
