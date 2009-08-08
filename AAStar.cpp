#include "AAStar.h"

void AAStar::init() {
	while(!open.empty()) open.pop();
	counter = 0;
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
			tracePath(x, path);
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
				y->g      = g;
				y->parent = x;
				y->h      = heuristic(y, goal);
				y->open   = true;
				open.push(y);
			}
			counter++;
		}
	}
	return false;
}

void AAStar::tracePath(ANode* x, std::list<ANode*>& path) {
	while (x != start) {
		path.push_front(x);
		x = x->parent;
	}
}
