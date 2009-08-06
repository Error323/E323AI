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
				ANode() {id = 0; g = 0; h = w = f = 0.0f; open = closed = false; }
				ANode(unsigned int id, float w) {
					this->id = id;
					this->w  = w;
					reset();
				}

				virtual ~ANode() {}

				bool open;
				bool closed;

				unsigned int id;

				float g;
				float h;
				float w;
				float f;

				ANode* parent;

				bool operator < (const ANode* n) const {
					return f > (n->f);
				}

				bool operator () (const ANode* a, const ANode* b) const {
					return (a->f > b->f);
				}

				bool operator == (const ANode* n) const {
					return (id == n->id);
				}

				bool operator != (const ANode* n) const {
					return !(this == n);
				}

				void reset() {
					this->g      = 0.0f;
					this->h      = 0.0f;
					this->f      = 0.0f;
					this->open   = false;
					this->closed = false;
				}
		};

		bool findPath(std::list<ANode*> &path);
		ANode* start;
		ANode* goal;

	protected:
		void init();
		virtual ~AAStar() {};

		virtual void successors(ANode *n, std::queue<ANode*> &succ) = 0;
		virtual float heuristic(ANode *n1, ANode *n2) = 0;

	private:
		/* priority queue of the open list */
		std::priority_queue<ANode*, std::vector<ANode*, std::allocator<ANode*> >, ANode> open;

		/* successors stack */
		std::queue<ANode*> succs;

		/* traces the path from the goal node through its parents */
		void tracePath(std::list<ANode*> &path);
};

#endif
