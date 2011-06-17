#ifndef E323_CCOVERAGECELL_H
#define E323_CCOVERAGECELL_H

#include <string>
#include <map>

#include "headers/Defines.h"
#include "headers/HEngine.h"
//#include "ARegistrar.h"
#include "CUnit.h"

class AIClasses;
class ARegistrar;
struct UnitType;

class CCoverageCell: public ARegistrar {

public:
	enum NType {
		UNDEFINED,
		DEFENSE_GROUND,   ///< autofire at ground
		DEFENSE_ANTIAIR,  ///< autofire at air
		DEFENSE_UNDERWATER,
		DEFENSE_ANTINUKE, ///< stockpile required, autofire at nuke missles
		DEFENSE_SHIELD,   ///< on-offable defense, does not shoot
		DEFENSE_JAMMER,
		BUILD_ASSISTER,   ///< static builders or assisters
		ECONOMY_BOOSTER   ///< pylons in CA
	};

	CCoverageCell()
			: type(UNDEFINED)
			, ai(NULL)
			, range(0.0f)
			, unit(NULL)
	{
		if (type2str.empty()) {
			type2str[UNDEFINED] = "UNDEFINED";
			type2str[DEFENSE_GROUND] = "DEFENSE_GROUND";
			type2str[DEFENSE_ANTIAIR] = "DEFENSE_ANTIAIR";
			type2str[DEFENSE_UNDERWATER] = "DEFENSE_UNDERWATER";
			type2str[DEFENSE_ANTINUKE] = "DEFENSE_ANTINUKE";
			type2str[DEFENSE_SHIELD] = "DEFENSE_SHIELD";
			type2str[DEFENSE_JAMMER] = "DEFENSE_JAMMER";
			type2str[BUILD_ASSISTER] = "BUILD_ASSISTER";
			type2str[ECONOMY_BOOSTER] = "ECONOMY_BOOSTER";
		}
	}
	CCoverageCell(AIClasses* ai, NType type = UNDEFINED, CUnit* unit = NULL)
			: type(type)
			, ai(ai)
			, range(0.0f)
			, unit(NULL)
	{
		if (unit) {
			setCore(unit);
		}
	}

	bool isVacant() const { return unit == NULL; }

	float3 getCenter() const { return unit ? unit->pos() : ERRORVECTOR; }

	float getRange() { return range; }

	bool isInRange(const float3& pos) const;

	void setCore(CUnit* u);

	CUnit* getCore() const { return unit; }

	void update(std::list<CUnit*>& uncovered);

	void remove();
	/* Overloaded */
	void remove(ARegistrar& obj);
	/* Overloaded */
	ARegistrar::NType regtype() const { return ARegistrar::COVERAGE_CELL; }

	bool addUnit(CUnit* u);
	/* Output stream */
	friend std::ostream& operator<<(std::ostream& out, const CCoverageCell& obj);


	static std::map<NType, std::string> type2str;
	NType type;
	/// units covered by current cell
	std::map<int, CUnit*> units;

//protected:
	AIClasses* ai;

private:
	/// cell range
	float range;
	/// cell core unit: the source of coverage
	CUnit* unit;
};

#endif // E323_CCOVERAGECELL_H
