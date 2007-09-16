#include<nsCOMPtr.h>
#include "nsTArray.h"
#include <glib.h>
#include <nsITimer.h>

struct purpleTimeout
{
  PRUint32 mId;
  nsCOMPtr<nsITimer> mTimer;
  GSourceFunc mFunction;
  gpointer mData;
};

class purpleTimer
{
 public:
  static PRUint32 AddTimeout(PRUint32 aInterval,
			     GSourceFunc aFunction,
			     gpointer aData);
  static void ExecTimer(nsITimer *aTimer, void *aTimerClosure);
  static PRBool CancelTimer(PRUint32 aId);

 private:
  static nsTArray<purpleTimeout *> mTimeouts;
  static PRUint32 mLastTimer;
};
