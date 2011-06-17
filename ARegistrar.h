#ifndef AREGISTRAR_H
#define AREGISTRAR_H

#include <list>

class ARegistrar {

public:
	enum NType {
		UNDEFINED,
		GROUP,
		TASK,
		UNIT,
		COVERAGE_CELL,
		COVERAGE_HANDLER
	};

	virtual ~ARegistrar() {}

	/* The key of this register, e.g. group id */
	int key;

	/* Register an object */
	void reg(ARegistrar& obj) { records.push_front(&obj); }
	/* Unregister an object */
	void unreg(ARegistrar& obj) { records.remove(&obj); }
	/* Propagate removal through the system */
	virtual void remove(ARegistrar& obj) = 0;
	/* Registrar type */
	virtual NType regtype() const { return UNDEFINED; }
	/* Check if current object is registered inside another type of object */
	bool regExists(NType type) {
		for (std::list<ARegistrar*>::const_iterator it = records.begin(); it != records.end(); ++it) {
			if ((*it)->regtype() == type)
				return true;
		}
		return false;
	}
	/* Count number of object registrations inside other objects of specific type */
	int regCount(NType type) {
		int result = 0;
		for (std::list<ARegistrar*>::const_iterator it = records.begin(); it != records.end(); ++it) {
			if ((*it)->regtype() == type)
				result++;
		}
		return result;
	}

protected:
	ARegistrar(int _key = 0): key(_key) {}

	/* The other objects where this registrar is registered */
	std::list<ARegistrar*> records;
};
#endif
