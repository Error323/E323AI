#ifndef E323ASTAR_H
#define E323ASTAR_H

#include <queue>
#include <vector>
#include <list>
#include <cstddef> // for NULL

class AAStar {
	public:
		class ANode {
			public:
				ANode()
					: open(false)
					, closed(false)
					, id(0)
					, g(0.0f)
					, h(0.0f)
					, w(0.0f)
					, parent(NULL)
				{
				}
				ANode(unsigned int id, float w)
					: open(false)
					, closed(false)
					, id(id)
					, g(0.0f)
					, h(0.0f)
					, w(w)
					, parent(NULL)
				{
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
					return (g+h) > (n->g+n->h);
				}

				bool operator () (const ANode* a, const ANode* b) const {
					return (a->g+a->h) > (b->g+b->h);
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
					this->open   = false;
					this->closed = false;
				}
		};

		bool findPath(std::list<ANode*> *path = NULL);

	protected:
		void init();
		virtual ~AAStar() {};

		virtual void successors(ANode *n, std::queue<ANode*> &succ) = 0;
		virtual float heuristic(ANode *n1, ANode *n2) = 0;
		ANode *start, *goal;
		unsigned int visited;

	private:
		/* priority queue of the open list */
		std::priority_queue<ANode*, std::vector<ANode*, std::allocator<ANode*> >, ANode> open;

		/* successors stack */
		std::queue<ANode*> succs;

		/* traces the path from the goal node through its parents */
		void tracePath(ANode *x, std::list<ANode*> &path);
};

#endif
