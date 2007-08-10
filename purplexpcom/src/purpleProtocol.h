#ifndef PURPLE_PROTOCOL_H_
# define PURPLE_PROTOCOL_H_

#include "purpleIProtocol.h"
#include "purpleIPref.h"
#include <libpurple/account.h>

// very dirty work around for the error:
// nsStringAPI.h:953: error: size of arg" is negative array
#undef HAVE_CPP_2BYTE_WCHAR_T
#include<nsCOMArray.h>

// 63ee9306-8615-4896-9f1f-6e6fafe17d1e
#define PURPLE_PROTOCOL_CID \
  { 0x63ee9306, 0x8615, 0x4896,					\
      { 0x9f, 0x1f, 0x6e, 0x6f, 0xaf, 0xe1, 0x7d, 0x1e }	\
  }

#define PURPLE_PROTOCOL_CONTRACTID \
    "@instantbird.org/purple/protocol;1"

class purpleProtocol : public purpleIProtocol
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_PURPLEIPROTOCOL

  purpleProtocol();
  void Init(PurplePluginInfo *aInfo);
  const char *GetId() {  return mInfo ? mInfo->id : NULL; }

private:
  ~purpleProtocol();

  PurplePluginInfo *mInfo;
  inline nsresult GetBoolOpt(PRBool *aRes, PurpleProtocolOptions aOpt);

protected:
  nsCOMArray<purpleIPref> mOptions;
};

#endif /* !PURPLE_PROTOCOL_H_ */
