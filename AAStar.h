#ifndef E323ASTAR_H
#define E323ASTAR_H

#include <queue>
#include <vector>
#include <iterator>
#include <list>

class AAStar {
	public:
		class ANode {
			public:
				ANode() {id = 0; g = 0; h = w = 0.0f; open = closed = false; }
				ANode(unsigned int id, float w) {
					this->id		= id;
					this->w			= w;
					this->g			= 0;
					this->h			= 0.0f;
					this->open		= false;
					this->closed	= false;
				}

				virtual ~ANode() {}

				bool open;
				bool closed;

				unsigned int id;

				float g;
				float h;
				float w;

				ANode* parent;

				bool operator < (const ANode* n) const {
					return (g + h) > (n->g + n->h);
				}

				bool operator () (const ANode* a, const ANode* b) const {
					return (float(a->g + a->h) > float(b->g + b->h));
				}

				bool operator == (const ANode* n) const {
					return (id == n->id);
				}

				bool operator != (const ANode* n) const {
					return !(this == n);
				}
		};

		std::vector<ANode*> history;
		bool findPath(std::list<ANode*> &path);
		ANode* start;
		ANode* goal;

	protected:
		void init();
		virtual ~AAStar() {};

		virtual void successors(ANode *n, std::queue<ANode*> &succ) = 0;
		virtual float heuristic(ANode *n1, ANode *n2) = 0;

		/* nodes visited during pathfinding */
		std::vector<ANode*> visited;

	private:
		/* priority queue of the open list */
		std::priority_queue<ANode*, std::vector<ANode*, std::allocator<ANode*> >, ANode> open;

		/* successors stack */
		std::queue<ANode*> succs;

		/* traces the path from the goal node through its parents */
		void tracePath(std::list<ANode*> &path);
};

#endif
