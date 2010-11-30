#ifndef REUSABLE_OBJECT_FACTORY
#define REUSABLE_OBJECT_FACTORY

#include <list>
#include <iostream>
#include <typeinfo>

template<class Object> class ReusableObjectFactory {
public:
	ReusableObjectFactory() {}
	~ReusableObjectFactory() {}

	static Object* Instance();
	static void Release(Object*);
	static void Shutdown();
	static void Free();
 
private:
	static std::list<Object*> free;
	static std::list<Object*> objects;
};

template<class Object>
std::list<Object*> ReusableObjectFactory<Object>::free;

template<class Object>
std::list<Object*> ReusableObjectFactory<Object>::objects;

template<class Object>
void ReusableObjectFactory<Object>::Shutdown() {
	/*
	std::cout << "ReusableObjectFactory<" 
	          << typeid(Object).name()
	          << ">::Shutdown Releasing "
	          << (objects.size()) 
	          << " objects, " 
	          << (objects.size()*sizeof(Object)) 
	          << " bytes"
	          << std::endl;
	*/
	typename std::list<Object*>::iterator i;
	for (i = objects.begin(); i != objects.end(); ++i)
		delete *i;

	objects.clear();
	free.clear();
}
 
template<class Object>
void ReusableObjectFactory<Object>::Free() {
	/*
	std::cout << "ReusableObjectFactory<" 
	          << typeid(Object).name()
	          << ">::Free Releasing "
	          << (free.size()) 
	          << " objects, " 
	          << (free.size()*sizeof(Object)) 
	          << " bytes"
	          << std::endl;
	*/
	typename std::list<Object*>::iterator i;
	for (i = free.begin(); i != free.end(); ++i) {
		objects.remove(*i);
		delete *i;
	}

	free.clear();
}

template<class Object>
Object* ReusableObjectFactory<Object>::Instance() {
	Object* object;

	if (free.empty()) {
		object = new Object();
		objects.push_back(object);
	}
	else {
		object = free.front();
		free.pop_front();
	}

	//std::cout << "ReusableObjectFactory<" 
	//          << typeid(Object).name() 
	//          << ">::Instance " 
	//          << (*object) 
	//          << std::endl;
	return object;
}
 
template<class Object>
void ReusableObjectFactory<Object>::Release(Object* object) {
	free.push_back(object);
	//std::cout << "ReusableObjectFactory<" 
	//          << typeid(Object).name() 
	//          << ">::Release " 
	//          << (*object) 
	//          << std::endl;
}

#endif
