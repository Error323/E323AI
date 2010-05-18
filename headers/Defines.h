#ifndef DEFINES_H
#define DEFINES_H

#include "../AIExport.h"

#define MAX_INT   2147483647
#define MAX_FLOAT float(MAX_INT)
#define EPSILON   1/MAX_FLOAT
#define EPS 0.0001f

#define ERRORVECTOR float3(-1.0f,0.0f,0.0f)
#define ZEROVECTOR  float3(0.0f,0.0f,0.0f)


/* AI meta data */
#define AI_VERSION_NR  aiexport_getVersion()
#define AI_NAME        std::string("E323AI")
#define AI_VERSION     AI_NAME + " " + AI_VERSION_NR + " - High Templar (" + __DATE__ + ")"
#define AI_CREDITS     "Error323 & Simon Logic"
#define AI_NOTES       "This A.I. mainly focusses on the XTA and BA mods"

/* Folders */
#define LOG_FOLDER   "logs/"
#define CFG_FOLDER   "configs/"
#define CACHE_FOLDER "cache/"

/* Templates */
#define CONFIG_TEMPLATE "template-config.cfg"

/* Misc macro's */
#define UD(udid) (ai->cb->GetUnitDef(udid))
#define UT(udid) (&(ai->unittable->units[udid]))
#define UC(udid) (ai->unittable->units[udid].cats)
#define ID(x,z) ((z)*X+(x))
#define ID_GRAPH(x,z) ((z)*XX+(x))

/* Metal to Energy ratio */
#define METAL2ENERGY 60

/* Max features for range scan */
#define MAX_FEATURES 15

/* Max enemies for range scan */
#define MAX_ENEMIES 30

/* Max enemy units (used by CThreatMap) */
#define MAX_UNITS_AI 500

/* Map ratios */
#define HEIGHT2REAL 8
#define I_MAP_RES 8 /* Inverse map resolution (must be even) */
#define HEIGHT2SLOPE 2
#define FOOTPRINT2REAL 8

/* Max factory Assisters */
#define FACTORY_ASSISTERS 6

/* Max number of scouts per group */
#define MAX_SCOUTS_IN_GROUP 3

/* Max number of idle scout groups to prevent building unnecessary scouts */
#define MAX_IDLE_SCOUT_GROUPS 3

/* Critical number of units per group where pathfinding stalls the game */
#define GROUP_CRITICAL_MASS 20

/* Max unit weapon range to be considered by threatmap algo */
#define MAX_WEAPON_RANGE_FOR_TM 1200.0f

/* Max distance at which groups can merge */
#define MERGE_DISTANCE 1500.0f

/* Number of multiplex iterations */
#define MULTIPLEXER 10 

/* Number of frames a new unit can not accept tasks */
#define NEW_UNIT_DELAY (5*30)

#define BAD_TARGET_TIMEOUT (60*30)

/* Draw time */
#define DRAW_TIME MULTIPLEXER*30


/* Stats url */
#define UPLOAD_URL "http://fhuizing.pythonic.nl/ai-stats.php"

/* We gonna use math.h constants */
#define _USE_MATH_DEFINES

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
	ASSISTER     = (1<<10),
	RESURRECTOR  = (1<<11),

	COMMANDER    = (1<<12),
	ATTACKER     = (1<<13),
	ANTIAIR      = (1<<14),
	SCOUTER      = (1<<15),
	ARTILLERY    = (1<<16),
	SNIPER       = (1<<17),
	ASSAULT      = (1<<18),

	MEXTRACTOR   = (1<<19),
	MMAKER       = (1<<20),
	EMAKER       = (1<<21),
	MSTORAGE     = (1<<22),
	ESTORAGE     = (1<<23),

	KBOT         = (1<<24),
	VEHICLE      = (1<<25),
	HOVER        = (1<<26),

	DEFENSE      = (1<<27)
};

/* Build priorities */
enum buildPriority {
	LOW    = 0,
	NORMAL = 1,
	HIGH   = 2
};

/* Build tasks */
enum buildType {
	BUILD_MPROVIDER,
	BUILD_EPROVIDER,
	BUILD_AA_DEFENSE,
	BUILD_AG_DEFENSE,
	BUILD_FACTORY,
	BUILD_MSTORAGE,
	BUILD_ESTORAGE
};

#endif
