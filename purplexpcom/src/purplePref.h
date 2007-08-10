#include "purpleIPref.h"
#include <libpurple/accountopt.h>

// fe21354d-2932-4a12-8d5b-15ac975d97e6
#define PURPLE_PREF_CID						\
  { 0xfe21354d, 0x2932, 0x4a12,					\
      { 0x8d, 0x5b, 0x15, 0xac, 0x97, 0x5d, 0x97, 0xe6 }	\
  }

#define PURPLE_PREF_CONTRACTID			\
  "@instantbird.org/purple/pref;1"

class purplePref : public purpleIPref
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_PURPLEIPREF

  purplePref();
  void Init(PurpleAccountOption *aOpt);

private:
  ~purplePref();

  PurpleAccountOption *mOpt;

protected:
  /* additional members */
};
