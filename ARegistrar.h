#ifndef AREGISTRAR_H
#define AREGISTRAR_H

#include <list>

class ARegistrar {
	public:
		virtual ~ARegistrar() {}

		/* The key of this register, e.g. group id */
		int key;

		/* Register an object */
		void reg(ARegistrar &obj) { records.push_back(&obj); }

		/* Unregister an object */
		void unreg(ARegistrar &obj) { records.remove(&obj); }

		/* Propagate removal through the system */
		virtual void remove(ARegistrar &obj) = 0;

	protected:
		ARegistrar(): key(0) {}
		ARegistrar(int _key): key(_key) {}

		/* The other objects where this registrar is registered */
		std::list<ARegistrar*> records;
};
#endif
