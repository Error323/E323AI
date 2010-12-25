#ifndef E323_CONTAINER_H
#define E323_CONTAINER_H

#include <vector>
#include <map>
#include <memory>
#include "CUnitTable.h"

class AContainer {

public:	
	virtual bool empty() = 0;
	virtual int size() = 0;
	virtual int fetch() = 0;
	virtual bool end() = 0;
	virtual void reset() = 0;
};

class CVectorContainer: public AContainer {
	
public:	
	CVectorContainer(std::vector<int>& src):ref(src) { reset(); };

	bool empty() { return ref.empty(); }
	int size() { return ref.size(); }
	int fetch() { return *(it++); }
	bool end() { return it == ref.end(); }
	void reset() { it = ref.begin(); }

private:
	std::vector<int>& ref;
	std::vector<int>::iterator it;
};

class CMapContainer: public AContainer {
	
public:	
	CMapContainer(std::map<int, UnitType*>& src):ref(src) { reset(); }

	bool empty() { return ref.empty(); }
	int size() { return ref.size(); }
	int fetch() { return (it++)->first; }
	bool end() { return it == ref.end(); }
	void reset() { it = ref.begin(); }

private:
	std::map<int, UnitType*>& ref;
	std::map<int, UnitType*>::iterator it;
};

class CContainer {

public:
	CContainer(std::vector<int>& src):pcont(new CVectorContainer(src)) {};
	CContainer(std::map<int, UnitType*>& src):pcont(new CMapContainer(src)) {};

	bool empty() { return pcont->empty(); }
	int size() { return pcont->size(); }
	int fetch() { return pcont->fetch(); }
	bool end() { return pcont->end(); }
	void reset() { pcont->reset(); }

private:
	std::auto_ptr<AContainer> pcont;
};

#endif
