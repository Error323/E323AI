#ifndef HAIINTERFACE_H
#define HAIINTERFACE_H

/* Spring AI Interface Headers */
#if defined(BUILDING_AI_FOR_SPRING_0_81_2)
	#include "ExternalAI/aibase.h"                 /* DLL exports and definitions */
	#include "ExternalAI/IGlobalAI.h"              /* Main AI file */
	#include "ExternalAI/IAICallback.h"            /* Callback functions */
	#include "ExternalAI/IGlobalAICallback.h"      /* AI Interface */
	#include "ExternalAI/IAICheats.h"              /* AI Cheat Interface */
	#include "ExternalAI/Interface/aidefines.h"    /* SNPRINTF, STRCPY, ... */
#else
	#include "LegacyCpp/aibase.h"                  /* DLL exports and definitions */
	#include "LegacyCpp/IGlobalAI.h"               /* Main AI file */
	#include "LegacyCpp/IAICallback.h"             /* Callback functions */
	#include "LegacyCpp/IGlobalAICallback.h"       /* AI Interface */
	#include "LegacyCpp/IAICheats.h"               /* AI Cheat Interface */
#endif

#endif
