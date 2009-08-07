#include "AAStar.h"

void AAStar::init() {
	while(!open.empty()) open.pop();
}

bool AAStar::findPath(std::list<ANode*>& path) {
	float g;
	ANode *x, *y;

	init();

	start->open = true;
	start->g = 0.0f;
	start->h = heuristic(start,goal);
	open.push(start);

	while (!open.empty()) {
		x = open.top(); open.pop();

		if (x == goal) {
			tracePath(path);
			return true;
		}

		x->open   = false;
		x->closed = true;

		successors(x, succs);
		while (!succs.empty()) {
			y = succs.front(); succs.pop();

			if (y->closed)
				continue;

			g = x->g + y->w*heuristic(x,y);
			if (y->open && g < y->g)
				y->open = false;

			if (!y->open) {
				y->g = g;
				y->parent = x;
				y->h = heuristic(y, goal);
				open.push(y);
				y->open = true;
			}
		}
	}
	return false;
}

void AAStar::tracePath(std::list<ANode*>& path) {
	ANode* n = goal;
	while (n != start) {
		path.push_front(n);
		n = n->parent;
	}
}
