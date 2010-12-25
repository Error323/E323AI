#ifndef DEFINES_H
#define DEFINES_H

#include <bitset>
#include <string>

#include "../AIExport.h"

#define MAX_INT   2147483647
#define MAX_FLOAT float(MAX_INT)
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

/* Max enemy units (used by CThreatMap; TODO: avoid) */
#define MAX_UNITS_AI 500

/* Map ratios */
#define FOOTPRINT2REAL 8
#define HEIGHT2REAL FOOTPRINT2REAL
#define SLOPE2HEIGHT 2
#define PATH2SLOPE 8
#define PATH2REAL (PATH2SLOPE * SLOPE2HEIGHT * HEIGHT2REAL)

/* Max factory Assisters */
#define FACTORY_ASSISTERS 6

/* Max number of scouts per group */
#define MAX_SCOUTS_IN_GROUP 3

/* Max number of idle scout groups to prevent building unnecessary scouts */
#define MAX_IDLE_SCOUT_GROUPS 3

/* Critical number of units per group when regrouping for moving group 
   stalls the game */
#define GROUP_CRITICAL_MASS 20

/* Max unit weapon range to be considered by threatmap algo (a hack!) */
#define MAX_WEAPON_RANGE_FOR_TM 1200.0f

/* Max distance at which groups can merge */
#define MERGE_DISTANCE 1500.0f

/* Number of multiplex iterations */
#define MULTIPLEXER 10 

/* Number of frames a new unit can not accept tasks */
#define IDLE_UNIT_TIMEOUT (5*30)

#define BAD_TARGET_TIMEOUT (60*30)

/* Number of unique tasks allowed to be executed per update */
#define MAX_TASKS_PER_UPDATE 3

/* Draw time */
#define DRAW_TIME MULTIPLEXER*30

/* Stats url (NOT used) */
#define UPLOAD_URL "http://fhuizing.pythonic.nl/ai-stats.php"

/* We gonna use math.h constants */
#define _USE_MATH_DEFINES

#define MIN_TECHLEVEL 1
#define MAX_TECHLEVEL 5

#define MAX_CATEGORIES 46

typedef std::bitset<MAX_CATEGORIES> unitCategory;

const unitCategory
	TECH1      (1UL<<0), // hardcored techlevels for now
	TECH2      (1UL<<1),
	TECH3      (1UL<<2),
	TECH4      (1UL<<3),
	TECH5      (1UL<<4),

	AIR        (1UL<<5), // can fly
	SEA        (1UL<<6), // can float
	LAND       (1UL<<7), // can walk/drive
	SUB        (1UL<<8), // can dive
	
	STATIC     (1UL<<9),
	MOBILE     (1UL<<10),

	FACTORY    (1UL<<11),
	BUILDER    (1UL<<12),
	ASSISTER   (1UL<<13),
	RESURRECTOR(1UL<<14),

	COMMANDER  (1UL<<15),
	ATTACKER   (1UL<<16),
	ANTIAIR    (1UL<<17),
	SCOUTER    (1UL<<18),
	ARTILLERY  (1UL<<19),
	SNIPER     (1UL<<20),
	ASSAULT    (1UL<<21),

	MEXTRACTOR (1UL<<22),
	MMAKER     (1UL<<23),
	EMAKER     (1UL<<24),
	MSTORAGE   (1UL<<25),
	ESTORAGE   (1UL<<26),
	
	DEFENSE    (1UL<<27),

	KBOT       (1UL<<28), // produces kbots
	VEHICLE    (1UL<<29), // produces vehicles
	HOVER      (1UL<<30), // produces hovercraft
	AIRCRAFT   (1UL<<31), // produces aircraft
	NAVAL      ('1' + std::string(32, '0')), // produces naval units

	JAMMER     ('1' + std::string(33, '0')),
	NUKE       ('1' + std::string(34, '0')),
	ANTINUKE   ('1' + std::string(35, '0')),
	PARALYZER  ('1' + std::string(36, '0')),
	TORPEDO	   ('1' + std::string(37, '0')),
	TRANSPORT  ('1' + std::string(38, '0')),
	EBOOSTER   ('1' + std::string(39, '0')),
	MBOOSTER   ('1' + std::string(40, '0')),
	SHIELD     ('1' + std::string(41, '0')),
	NANOTOWER  ('1' + std::string(42, '0')),
	REPAIRPAD  ('1' + std::string(43, '0')),

	WIND       ('1' + std::string(44, '0')),
	TIDAL      ('1' + std::string(45, '0'));

const unitCategory
	CATS_ANY     (std::string(MAX_CATEGORIES, '1')),
	CATS_ENV     (AIR|LAND|SEA|SUB),
	CATS_ECONOMY (FACTORY|BUILDER|ASSISTER|RESURRECTOR|COMMANDER|MEXTRACTOR|MMAKER|EMAKER|MSTORAGE|ESTORAGE|EBOOSTER|MBOOSTER);

/* Build tasks */
enum buildType {
	BUILD_MPROVIDER,
	BUILD_EPROVIDER,
	BUILD_AA_DEFENSE,
	BUILD_AG_DEFENSE,
	BUILD_UW_DEFENSE,
	BUILD_FACTORY,
	BUILD_MSTORAGE,
	BUILD_ESTORAGE,
	BUILD_MISC_DEFENSE,
	BUILD_IMP_DEFENSE
};

enum difficultyLevel {
	DIFFICULTY_EASY = 1,
	DIFFICULTY_NORMAL,
	DIFFICULTY_HARD
};

#endif
