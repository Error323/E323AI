#ifndef SINGLETON_OBJECT_FACTORY
#define SINGLETON_OBJECT_FACTORY

#include <map>

template<class Object>
class SingletonObjectFactory {
	public:
		SingletonObjectFactory(){}
		~SingletonObjectFactory();

		static Object* Instance(int i = -1);
	
	private:
		static std::map<int,Object*> objects;
};

template <class Object>
std::map<int,Object*> SingletonObjectFactory<Object>::objects;

template <class Object>
SingletonObjectFactory<Object>::~SingletonObjectFactory() {
	typename std::map<int,Object*>::iterator i;
	for (i = objects.begin(); i != objects.end(); i++)
		delete i->second;
	objects.clear();
}

template <class Object>
Object* SingletonObjectFactory<Object>::Instance(int i) {
	if (objects.find(i) == objects.end())
		objects[i] = new Object();

	return objects[i];
}

#endif
