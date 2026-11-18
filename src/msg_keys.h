#pragma once
// Message feature (a private note from the maker that pops up on the frame). Everything here is meant to be public:
//  - the mailbox name and the AES key only scramble the text, so a casual look at the mailbox shows nothing readable;
//  - the public key below can only CHECK signatures. A message is shown only if it is signed with the private key, which lives on the maker's Mac and nowhere else.
static const char MSG_TOPIC[] = "ff3348f2344e2a3f457024";
static const uint8_t MSG_AESKEY[32] = {0x89,0xc8,0x9d,0xe6,0x1c,0x30,0x98,0xca,0x52,0xf5,0x3d,0x49,0x2d,0x0b,0x5f,0xcd,0xd9,0xe4,0x8c,0x56,0xc7,0x02,0xfb,0x32,0x41,0x8f,0x00,0xe5,0x5e,0xce,0xa1,0x71};
static const char MSG_PUBKEY_PEM[] =
  "-----BEGIN PUBLIC KEY-----\n"
  "MFkwEwYHKoZIzj0CAQYIKoZIzj0DAQcDQgAEA3tqX/k3Y2RvJtPMhUW7GUko8cIB\n"
  "S7+g0Wm9JFOJCXao5ieTpxw5oNX40sIE4P4gCesa06+g7G+Q5PYuSf7lhQ==\n"
  "-----END PUBLIC KEY-----\n"  ;
