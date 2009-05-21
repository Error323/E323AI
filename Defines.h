#ifndef DEFINES_H
#define DEFINES_H

#include "AIExport.h"

#define MAX_FLOAT 1000000000.0f
#define MAX_INT   1000000000
#define EPSILON   1.0f/MAX_FLOAT

#define ERRORVECTOR float3(-1,0,0)
#define NULLVECTOR  float3(0,0,0)

#define AI_VERSION  aiexport_getVersion()

/* AI meta data */
#define AI_NOTES	"This A.I. mainly focusses on the XTA mod"
#define AI_CREDITS  "Error323 - folkerthuizinga@gmail.com"

/* Folders */
#define LOG_FOLDER  "logs/"
#define LOG_PATH    std::string(aiexport_getDataDir(true)) + LOG_FOLDER

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
#define MINGROUPSIZE 5


/* Facings */
enum facing {
	NONE  =-1, 
	SOUTH = 0, 
	EAST  = 1, 
	NORTH = 2, 
	WEST  = 3
};

/* Unit categories */
enum unitCategory {
	COMMANDER    = (1<<0),
	STATIC       = (1<<1),
	MOBILE       = (1<<2),
	AIR          = (1<<3),
	SEA          = (1<<4),
	LAND         = (1<<5),
	BUILDER      = (1<<6),
	ATTACKER     = (1<<7),
	ASSIST       = (1<<8),
	RESURRECTOR  = (1<<9),
	FACTORY      = (1<<10),
	ANTIAIR      = (1<<11),
	RADAR        = (1<<12),
	JAMMER       = (1<<13),
	SONAR        = (1<<14),
	MMAKER       = (1<<15),
	EMAKER       = (1<<16),
	MSTORAGE     = (1<<17),
	ESTORAGE     = (1<<18),
	MEXTRACTOR   = (1<<19),
	TRANSPORT    = (1<<20),
	SCOUT        = (1<<21),
	ARTILLERY    = (1<<22),
	VEHICLE      = (1<<24),
	KBOT         = (1<<25),
	TECH1        = (1<<26),
	TECH2        = (1<<27),
	TIDAL        = (1<<28),
	WIND         = (1<<29)
};

/* Tasks */
enum task {
	BUILD_MMAKER   = (1<<0),
	BUILD_EMAKER   = (1<<1),
	BUILD_FACTORY  = (1<<2),
	ASSIST_FACTORY = (1<<3),
	HARRAS         = (1<<4),
	ATTACK         = (1<<5),
	UNKNOWN        = (1<<6)
};

/* Build priorities */
enum buildPriority {
	LOW    = 0,
	NORMAL = 1,
	HIGH   = 2
};

enum quadrant {
	NORTH_WEST,
	NORTH_EAST,
	SOUTH_WEST,
	SOUTH_EAST
};

#endif
