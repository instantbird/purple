#ifndef PURPLE_TAG_H_
# define PURPLE_TAG_H_

#include "purpleITag.h"
#include <libpurple/account.h>

// 0012b173-0404-46e7-bd54-e04e22e3dd69
#define PURPLE_TAG_CID                                          \
  { 0x0012b173, 0x0404, 0x46e7,                                 \
      { 0xbd, 0x54, 0xe0, 0x4e, 0x22, 0xe3, 0xdd, 0x69 }        \
  }

#define PURPLE_TAG_CONTRACTID                   \
  "@instantbird.org/purple/tag;1"

class purpleTag : public purpleITag
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_PURPLEITAG

  purpleTag();
  void Init(PRUint32 aId, PurpleGroup *aGroup = NULL);
  void addBuddy(PurpleBuddy *aBuddy);

private:
  ~purpleTag();

protected:
  /* additional members */
  PRUint32 mId;
  PurpleGroup *mGroup;
};

#endif /* !PURPLE_TAG_H _ */
