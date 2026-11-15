#pragma once
// Engines view data: aircraft type (ICAO) -> family, seats, engine count and the engine that most likely hangs on it.
// The ADS-B feed does not say which engine a plane has, so this is an educated guess per type (the screen says so in the README).
#include <Arduino.h>
#include "adsb.h"

enum Maker : uint8_t { M_CFM, M_PW, M_GE, M_RR, N_MAKERS };
static const char* const MAKER_NAME[N_MAKERS] = {"CFM", "P&W", "GE", "Rolls-Royce"};

struct EngModel { const char* name; uint8_t maker; uint8_t klbf; };            // klbf = thrust of one engine in thousand pounds-force
static const EngModel EMS[] = {
  {"CFM56", M_CFM, 25}, {"LEAP", M_CFM, 29},
  {"PW100", M_PW, 3}, {"PW1100G", M_PW, 30}, {"PW1500G", M_PW, 23}, {"PW1900G", M_PW, 21}, {"PW2000", M_PW, 41},
  {"GE90", M_GE, 100}, {"GE9X", M_GE, 105}, {"GEnx", M_GE, 66}, {"CF6", M_GE, 55}, {"CF34", M_GE, 14},
  {"Trent", M_RR, 72}, {"AE3007", M_RR, 7}, {"BR715", M_RR, 21},
};
enum EM_ : uint8_t { E_CFM56, E_LEAP, E_PW100, E_PW1100G, E_PW1500G, E_PW1900G, E_PW2000, E_GE90, E_GE9X, E_GENX, E_CF6, E_CF34, E_TRENT, E_AE3007, E_BR715, N_EMS };

enum Fam : uint8_t { F_A320, F_B737, F_B777, F_A330, F_B787, F_ATR, F_DASH8, F_A350, F_EMB, F_CRJ, F_B747, F_B767, F_B757, F_A380, F_A220, F_A340, F_A300, F_B717, N_FAMS };
static const char* const FAM_NAME[N_FAMS] = {"A320", "B737", "B777", "A330", "B787", "ATR", "Dash 8", "A350", "Embraer", "CRJ", "B747", "B767", "B757", "A380", "A220", "A340", "A300/10", "B717"};

struct TypeInfo { const char* t; uint8_t fam, em, n; uint16_t seats; };           // n = engines, seats = typical airline layout
static const TypeInfo TYPES[] = {
  {"A318", F_A320, E_CFM56, 2, 107}, {"A319", F_A320, E_CFM56, 2, 144}, {"A320", F_A320, E_CFM56, 2, 180}, {"A321", F_A320, E_CFM56, 2, 220},
  {"A19N", F_A320, E_LEAP, 2, 150}, {"A20N", F_A320, E_LEAP, 2, 186}, {"A21N", F_A320, E_LEAP, 2, 220},
  {"B731", F_B737, E_CFM56, 2, 100}, {"B732", F_B737, E_CFM56, 2, 120}, {"B733", F_B737, E_CFM56, 2, 134}, {"B734", F_B737, E_CFM56, 2, 150}, {"B735", F_B737, E_CFM56, 2, 108},
  {"B736", F_B737, E_CFM56, 2, 108}, {"B737", F_B737, E_CFM56, 2, 126}, {"B738", F_B737, E_CFM56, 2, 175}, {"B739", F_B737, E_CFM56, 2, 180},
  {"B37M", F_B737, E_LEAP, 2, 172}, {"B38M", F_B737, E_LEAP, 2, 178}, {"B39M", F_B737, E_LEAP, 2, 193}, {"B3XM", F_B737, E_LEAP, 2, 204},
  {"B772", F_B777, E_GE90, 2, 305}, {"B77L", F_B777, E_GE90, 2, 301}, {"B77W", F_B777, E_GE90, 2, 350}, {"B773", F_B777, E_GE90, 2, 368}, {"B778", F_B777, E_GE9X, 2, 384}, {"B779", F_B777, E_GE9X, 2, 414},
  {"B788", F_B787, E_GENX, 2, 242}, {"B789", F_B787, E_GENX, 2, 290}, {"B78X", F_B787, E_GENX, 2, 330},
  {"B744", F_B747, E_CF6, 4, 416}, {"B748", F_B747, E_GENX, 4, 410},
  {"B752", F_B757, E_PW2000, 2, 200}, {"B753", F_B757, E_PW2000, 2, 243},
  {"B762", F_B767, E_CF6, 2, 181}, {"B763", F_B767, E_CF6, 2, 218}, {"B764", F_B767, E_CF6, 2, 245},
  {"B712", F_B717, E_BR715, 2, 106},
  {"A332", F_A330, E_TRENT, 2, 260}, {"A333", F_A330, E_TRENT, 2, 290}, {"A338", F_A330, E_TRENT, 2, 260}, {"A339", F_A330, E_TRENT, 2, 287},
  {"A342", F_A340, E_CFM56, 4, 260}, {"A343", F_A340, E_CFM56, 4, 295}, {"A345", F_A340, E_TRENT, 4, 313}, {"A346", F_A340, E_TRENT, 4, 380},
  {"A359", F_A350, E_TRENT, 2, 315}, {"A35K", F_A350, E_TRENT, 2, 350}, {"A388", F_A380, E_TRENT, 4, 525},
  {"A306", F_A300, E_CF6, 2, 260}, {"A30B", F_A300, E_CF6, 2, 260}, {"A310", F_A300, E_CF6, 2, 220},
  {"BCS1", F_A220, E_PW1500G, 2, 110}, {"BCS3", F_A220, E_PW1500G, 2, 130},
  {"AT43", F_ATR, E_PW100, 2, 48}, {"AT45", F_ATR, E_PW100, 2, 48}, {"AT46", F_ATR, E_PW100, 2, 48}, {"AT72", F_ATR, E_PW100, 2, 70}, {"AT73", F_ATR, E_PW100, 2, 70}, {"AT75", F_ATR, E_PW100, 2, 70}, {"AT76", F_ATR, E_PW100, 2, 70},
  {"DH8A", F_DASH8, E_PW100, 2, 37}, {"DH8B", F_DASH8, E_PW100, 2, 37}, {"DH8C", F_DASH8, E_PW100, 2, 50}, {"DH8D", F_DASH8, E_PW100, 2, 78},
  {"E135", F_EMB, E_AE3007, 2, 37}, {"E145", F_EMB, E_AE3007, 2, 50}, {"E170", F_EMB, E_CF34, 2, 76}, {"E75S", F_EMB, E_CF34, 2, 76}, {"E75L", F_EMB, E_CF34, 2, 76},
  {"E190", F_EMB, E_CF34, 2, 100}, {"E195", F_EMB, E_CF34, 2, 118}, {"E290", F_EMB, E_PW1900G, 2, 106}, {"E295", F_EMB, E_PW1900G, 2, 132}, {"E275", F_EMB, E_PW1900G, 2, 88},
  {"CRJ1", F_CRJ, E_CF34, 2, 50}, {"CRJ2", F_CRJ, E_CF34, 2, 50}, {"CRJ7", F_CRJ, E_CF34, 2, 70}, {"CRJ9", F_CRJ, E_CF34, 2, 90}, {"CRJX", F_CRJ, E_CF34, 2, 100},
};
static const int N_TYPES = sizeof(TYPES) / sizeof(TYPES[0]);
static const TypeInfo* typeInfo(const char* t) { for (int i = 0; i < N_TYPES; i++) if (!strcmp(TYPES[i].t, t)) return &TYPES[i]; return nullptr; }

// What is in the air right now, worked out from the plane list (planes of unknown type, light aircraft and helicopters are left out).
struct Tally {
  uint16_t famPlanes[N_FAMS] = {}, famEng[N_FAMS] = {};
  uint16_t emEng[N_EMS] = {}; uint32_t emPeople[N_EMS] = {};
  uint16_t makerEng[N_MAKERS] = {}; uint32_t makerPeople[N_MAKERS] = {};
  uint32_t people = 0, thrustK = 0; uint16_t engines = 0, planesCounted = 0;
};
static void tallyPlanes(Tally& t) {
  for (uint8_t i = 0; i < nPlanes; i++) {
    const TypeInfo* ti = typeInfo(planes[i].type); if (!ti) continue;
    const EngModel& m = EMS[ti->em];
    t.famPlanes[ti->fam]++; t.famEng[ti->fam] += ti->n;
    t.emEng[ti->em] += ti->n; t.emPeople[ti->em] += ti->seats;
    t.makerEng[m.maker] += ti->n; t.makerPeople[m.maker] += ti->seats;
    t.people += ti->seats; t.engines += ti->n; t.thrustK += (uint32_t)m.klbf * ti->n; t.planesCounted++;
  }
}
