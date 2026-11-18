#pragma once
// Public half of the firmware signing key (the private half lives only on the maker's Mac). A frame installs an update only if it carries a valid signature from this key.
static const char OTA_PUBKEY_PEM[] =
  "-----BEGIN PUBLIC KEY-----\n"
  "MFkwEwYHKoZIzj0CAQYIKoZIzj0DAQcDQgAEoTf8HJ6kpjGAeL+77QhYGYSDSTX5\n"
  "0tOAVgYVtTvlnDhMWet85GFrKp8k4WboZ4olo9w4Eo7qNnHLp7bk1YywHQ==\n"
  "-----END PUBLIC KEY-----\n";
