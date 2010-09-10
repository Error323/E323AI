#ifndef AREGISTRAR_H
#define AREGISTRAR_H

#include <list>
#include <string>

enum RegistrarType {
	REG_UNDEFINED,
	REG_GROUP,
	REG_TASK,
	REG_UNIT
};

class ARegistrar {
	public:
		virtual ~ARegistrar() {}

		/* The key of this register, e.g. group id */
		int key;

		/* The name of this Registrar */
		std::string name;

		/* Register an object */
		void reg(ARegistrar &obj) { records.push_front(&obj); }

		/* Unregister an object */
		void unreg(ARegistrar &obj) { records.remove(&obj); }

		/* Propagate removal through the system */
		virtual void remove(ARegistrar &obj) = 0;

		virtual RegistrarType regtype() { return REG_UNDEFINED; } 

	protected:
		ARegistrar(): key(0), name("undefined") {}
		ARegistrar(int _key): key(_key), name("undefined") {}
		ARegistrar(int _key, std::string _name): key(_key), name(_name) {}

		/* The other objects where this registrar is registered */
		std::list<ARegistrar*> records;
};
#endif
