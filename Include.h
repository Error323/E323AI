#ifndef INCLUDE_H
#define INCLUDE_H

/* C/C++ headers */
#include <math.h>
#include <stdarg.h>

#include <cassert>
#include <string>
#include <ctime>
#include <iostream>
#include <fstream>
#include <sstream>
#include <set>
#include <map>
#include <vector>
#include <queue>
#include <list>
#include <algorithm>

/* Spring Standard Header */
#include "System/StdAfx.h"

/* Spring Engine Headers */
#include "Sim/Units/UnitDef.h"                 /* Unit Definitions */
#include "Sim/Units/CommandAI/CommandQueue.h"  /* Unit Command Queues */
#include "Sim/Features/FeatureDef.h"           /* Feature Definitions */
#include "Sim/MoveTypes/MoveInfo.h"            /* Types of Movement units can have */
#include "Sim/Weapons/WeaponDefHandler.h"      /* Weapon Definitions */
#include "System/float3.h"                     /* 3D vector operations */

/* Spring AI Interface Headers */
#include "ExternalAI/aibase.h"                 /* DLL exports and definitions */
#include "ExternalAI/IGlobalAI.h"              /* Main AI file */
#include "ExternalAI/IAICallback.h"            /* Callback functions */
#include "ExternalAI/IGlobalAICallback.h"      /* AI Interface */
#include "ExternalAI/IAICheats.h"              /* AI Cheat Interface */
#include "ExternalAI/Interface/aidefines.h"    /* SNPRINTF, STRCPY, ... */

/* E323AI Headers */
#include "Defines.h"                           /* Definition declarations */
#include "Container.h"                         /* Class wrapper */
#include "RNG.h"                               /* Random number generator */

#include "CUnit.h"
#include "CGroup.h"
#include "CMetalMap.h"
#include "CUnitTable.h"
#include "CEconomy.h"
#include "CWishList.h"
#include "CTaskPlan.h"
#include "CThreatMap.h"
#include "AAStar.h"
#include "CPathfinder.h"
#include "CIntel.h"
#include "CMilitary.h"

#endif
