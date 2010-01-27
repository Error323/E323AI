#ifndef GLOBALAI_H
#define GLOBALAI_H

#include "headers/HEngine.h"
#include "headers/HAIInterface.h"

class AIClasses;

class CE323AI: public IGlobalAI {
	public:
		CE323AI(){}
		~CE323AI();

		void InitAI(IGlobalAICallback* callback, int team);

		void UnitCreated(int unit, int builder);
		void UnitFinished(int unit);
		void UnitDestroyed(int unit, int attacker);
		void UnitIdle(int unit);
		void UnitDamaged(int damaged, int attacker, float damage, float3 dir);
		void EnemyDamaged(int damaged, int attacker, float damage, float3 dir);
		void UnitMoveFailed(int unit);

		void EnemyEnterLOS(int enemy);
		void EnemyLeaveLOS(int enemy);
		void EnemyEnterRadar(int enemy);
		void EnemyLeaveRadar(int enemy);
		void EnemyDestroyed(int enemy, int attacker);

		void GotChatMsg(const char* msg, int player);
		int HandleEvent(int msg, const void* data);

		void Update();

	private:
		AIClasses *ai;
};

#endif
