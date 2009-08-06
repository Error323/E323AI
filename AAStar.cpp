#include "AAStar.h"
#include "ScopedTimer.h"

void AAStar::init() {
	while(!open.empty()) open.pop();
}

bool AAStar::findPath(std::list<ANode*>& path) {
	float g;
	bool better;
	ANode *x, *y;

	init();

	start->open = true;
	start->g = 0.0f;
	start->h = heuristic(start,goal);
	start->f = start->h;
	open.push(start);

	while (!open.empty()) {
		x = open.top(); open.pop();
		x->open = false;

		if (x == goal) {
			tracePath(path);
			return true;
		}

		x->open   = false;
		x->closed = true;

		successors(x, succs);
		while (!succs.empty()) {
			y = succs.front(); succs.pop();

			if (y->closed) continue;

			g = x->g + y->w*heuristic(x, y);
			better = false;

			if (!y->open) {
				better  = true;
				y->h    = heuristic(y, goal);
			}
			else if (g < y->g) {
				better = true;
			}
			
			if (better) {
				y->parent = x;
				y->g = g;
				y->f = y->g + y->h;
				if (!y->open) { 
					y->open = true;
					open.push(y);
				}
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
