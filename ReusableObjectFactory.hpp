#ifndef REUSABLE_OBJECT_FACTORY
#define REUSABLE_OBJECT_FACTORY

#include <list>
#include <iostream>

template<class Object> class ReusableObjectFactory {
public:
	ReusableObjectFactory() {}
	~ReusableObjectFactory();

	static Object* Instance();
	static void Release(Object*);
 
private:
	static std::list<Object*> free;
};

template<class Object>
std::list<Object*> ReusableObjectFactory<Object>::free;

template<class Object>
ReusableObjectFactory<Object>::~ReusableObjectFactory() {
	typename std::list<Object*>::iterator i;
	for (i = free.begin(); i != free.end(); i++)
		delete *i;
	free.clear();
}
 
template<class Object>
Object* ReusableObjectFactory<Object>::Instance() {
	Object* object;

	if (free.empty()) {
		object = new Object();
#ifdef DEBUG
		std::cout << "ReusableObjectFactory::Instance " << (*object) << " created" << std::endl;
#endif
	}
	else {
		object = free.front();
		free.pop_front();
#ifdef DEBUG
		std::cout << "ReusableObjectFactory::Instance " << (*object) << " reused" << std::endl;
#endif
	}

	return object;
}
 
template<class Object>
void ReusableObjectFactory<Object>::Release(Object* object) {
	free.push_back(object);
#ifdef DEBUG
	std::cout << "ReusableObjectFactory::Release " << (*object) << " released" << std::endl;
#endif
}

#endif
