#ifndef DEFINES_H
#define DEFINES_H

#include "AIExport.h"

#define MAX_FLOAT 1000000000.0f
#define MAX_INT   1000000000
#define EPSILON   .00000000001f

#define ERRORVECTOR float3(-1,0,0)
#define NULLVECTOR  float3(0,0,0)


/* AI meta data */
#define AI_VERSION_NR  aiexport_getVersion()
#define AI_VERSION     std::string("E323AI ") + AI_VERSION_NR + " - Mutalisk"
#define AI_CREDITS     "Error323 - folkerthuizinga@gmail.com"
#define AI_NOTES	   "This A.I. mainly focusses on the XTA mod"

/* Folders */
#define LOG_FOLDER  "logs/"
#define CFG_FOLDER  "configs/"
#define LOG_PATH    std::string(aiexport_getDataDir(true)) + LOG_FOLDER
#define CFG_PATH    std::string(aiexport_getDataDir(true)) + CFG_FOLDER

/* Logger */
#define LOG(x)		(*ai->logger << x)
#define LOGN(x)		(*ai->logger << x << " :" << ai->call->GetCurrentFrame() << std::endl)
#define LOGS(x)     (ai->call->SendTextMsg(x, 0))

/* Misc macro's */
#define UD(u) (ai->call->GetUnitDef(u))
#define UT(u) (&(ai->unitTable->units[u]))
#define COMM ai->unitTable->comm
#define id(x,z) (x*Z+z)

/* Metal to Energy ratio */
#define METAL2ENERGY 45

/* Max enemy units */
#define MAX_UNITS 500

/* Threatmap resolution */
#define THREATRES 8

/* Minimal group size */
#define MINGROUPSIZE 3

/* Compute the sign of an int */
#define SIGN(x) (+1 | (x >> (sizeof(int) * 8 - 1)))

/* Unit categories */
enum unitCategory {
	TECH1        = (1<<0),
	TECH2        = (1<<1),
	TECH3        = (1<<2),

	AIR          = (1<<3),
	SEA          = (1<<4),
	LAND         = (1<<5),
	STATIC       = (1<<6),
	MOBILE       = (1<<7),

	FACTORY      = (1<<8),
	BUILDER      = (1<<9),
	ASSIST       = (1<<10),
	RESURRECTOR  = (1<<11),

	COMMANDER    = (1<<12),
	ATTACKER     = (1<<13),
	ANTIAIR      = (1<<14),
	SCOUT        = (1<<15),
	ARTILLERY    = (1<<16),
	SNIPER       = (1<<17),
	ASSAULT      = (1<<18),

	MEXTRACTOR   = (1<<19),
	MMAKER       = (1<<20),
	EMAKER       = (1<<21),
	MSTORAGE     = (1<<22),
	ESTORAGE     = (1<<23),
	WIND         = (1<<24),
	TIDAL        = (1<<25),

	KBOT         = (1<<26),
	VEHICLE      = (1<<27)
};

/* Build priorities */
enum buildPriority {
	LOW    = 0,
	NORMAL = 1,
	HIGH   = 2
};

#endif
