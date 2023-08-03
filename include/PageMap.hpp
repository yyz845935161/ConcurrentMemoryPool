#pragma once
#include"Common.h"

// 基数树
// Single-level array
//BITS 存储页号需要多少位  64位必须用三层
// 32位下   32- PAGE_SHIFT   = 2^19  占用内存: 2^19 *4 = 2^21 = 2M
template <int BITS>
class TCMalloc_PageMap1 {
private:
	static const int LENGTH = 1 << BITS;
	void** array_;

public:
	typedef uintptr_t Number;

	// explicit TCMalloc_PageMap1(void* (*allocator)(size_t)) {
	explicit TCMalloc_PageMap1() {
		// array_ = reinterpret_cast<void**>((*allocator)(sizeof(void*) << BITS));
		size_t size = sizeof(void*) << BITS;
		size_t alignSize = SizeClass::_RoundUp(size, 1<<PAGE_SHIFT);
		array_ = (void**)SystemAlloc(alignSize>>PAGE_SHIFT);
		memset(array_, 0, sizeof(void*) << BITS);
	}

	// Return the current value for KEY.  Returns NULL if not yet set,
	// or if k is out of range.
	void* get(Number k) const {
		if ((k >> BITS) > 0) {
			return NULL;
		}
		return array_[k];
	}

	// REQUIRES "k" is in range "[0,2^BITS-1]".
	// REQUIRES "k" has been ensured before.
	//
	// Sets the value 'v' for key 'k'.
	void set(Number k, void* v) {
		array_[k] = v;
	}
};

// Two-level radix tree 也适用32位
// 两层基数树 第一层固定32 个
// 第二层有2^14 
template <int BITS>
class TCMalloc_PageMap2 {
private:
	// Put 32 entries in the root and (2^BITS)/32 entries in each leaf.
	static const int ROOT_BITS = 5;
	static const int ROOT_LENGTH = 1 << ROOT_BITS;

	static const int LEAF_BITS = BITS - ROOT_BITS;
	static const int LEAF_LENGTH = 1 << LEAF_BITS;

	// Leaf node
	struct Leaf {
		void* values[LEAF_LENGTH];
	};

	Leaf* root_[ROOT_LENGTH];             // Pointers to 32 child nodes
	void* (*allocator_)(size_t);          // Memory allocator

public:
	typedef uintptr_t Number;

	//explicit TCMalloc_PageMap2(void* (*allocator)(size_t)) {
	explicit TCMalloc_PageMap2() {
		//allocator_ = allocator;
		memset(root_, 0, sizeof(root_));

		PreallocateMoreMemory();
	}

	void* get(Number k) const {
		const Number i1 = k >> LEAF_BITS;
		const Number i2 = k & (LEAF_LENGTH - 1);
		if ((k >> BITS) > 0 || root_[i1] == NULL) {
			return NULL;
		}
		return root_[i1]->values[i2];
	}

	void set(Number k, void* v) {
		const Number i1 = k >> LEAF_BITS;
		const Number i2 = k & (LEAF_LENGTH - 1);
		assert(i1 < ROOT_LENGTH);
		root_[i1]->values[i2] = v;
	}

	bool Ensure(Number start, size_t n) {
		for (Number key = start; key <= start + n - 1;) {
			const Number i1 = key >> LEAF_BITS;

			// Check for overflow
			if (i1 >= ROOT_LENGTH)
				return false;

			// Make 2nd level node if necessary
			if (root_[i1] == NULL) {
				//Leaf* leaf = reinterpret_cast<Leaf*>((*allocator_)(sizeof(Leaf)));
				//if (leaf == NULL) return false;
				static ObjectPool<Leaf>	leafPool;
				Leaf* leaf = (Leaf*)leafPool.New();

				memset(leaf, 0, sizeof(*leaf));
				root_[i1] = leaf;
			}

			// Advance key past whatever is covered by this leaf node
			key = ((key >> LEAF_BITS) + 1) << LEAF_BITS;
		}
		return true;
	}

	void PreallocateMoreMemory() {
		// Allocate enough to keep track of all possible pages
		Ensure(0, 1 << BITS);
	}
};

static const int INTERIOR_BITS = 18;
static const int INTERIOR_LENGTH = 262144;
// static const int INTERIOR_LENGTH = 100;
static const int LEAF_BITS = 16;  //16
static const int LEAF_LENGTH = 65536;

// Three-level radix tree
//适用于64位 
template <int BITS>
class TCMalloc_PageMap3 {
private:
	// How many bits should we consume at each interior level
	// static const int INTERIOR_BITS = (BITS + 2) / 3; // Round-up  //18
	// static const int INTERIOR_LENGTH = 1 << INTERIOR_BITS;  //262144

	// // How many bits should we consume at leaf level
	// static const int LEAF_BITS = BITS - 2 * INTERIOR_BITS;  //16
	// static const int LEAF_LENGTH = 1 << LEAF_BITS; //65536

	// static const int INTERIOR_BITS = 18;
	// static const int INTERIOR_LENGTH = 262144;
	// static const int LEAF_BITS = 16;  //16
	// static const int LEAF_LENGTH = 65536;

	// Interior node
	struct Node {
		Node* ptrs[INTERIOR_LENGTH];
	};

	// Leaf node
	struct Leaf {
		void* values[LEAF_LENGTH];
	};

	Node* root_;                          // Root of radix tree
	void* (*allocator_)(size_t);          // Memory allocator

	Node* NewNode() {
		// Node* result = reinterpret_cast<Node*>((*allocator_)(sizeof(Node)));
		// if (result != NULL) {
		// 	memset(result, 0, sizeof(*result));
		// }
		static ObjectPool<Node>	NodePool;
		Node* result = (Node*)NodePool.New();
		if (result != NULL) {
			memset(result, 0, sizeof(*result));
		}

		return result;
	}

public:
	typedef uintptr_t Number;

	// explicit TCMalloc_PageMap3(void* (*allocator)(size_t)) {
	// 	allocator_ = allocator;
	// 	root_ = NewNode();
	// }

	explicit TCMalloc_PageMap3() {
		// allocator_ = allocator;

		root_ = NewNode();
		// PreallocateMoreMemory() ;
	}




	void* get(Number k) const {
		const Number i1 = k >> (LEAF_BITS + INTERIOR_BITS);
		const Number i2 = (k >> LEAF_BITS) & (INTERIOR_LENGTH - 1);
		const Number i3 = k & (LEAF_LENGTH - 1);
		if ((k >> BITS) > 0 ||
			root_->ptrs[i1] == NULL || root_->ptrs[i1]->ptrs[i2] == NULL) {
			return NULL;
		}
		return reinterpret_cast<Leaf*>(root_->ptrs[i1]->ptrs[i2])->values[i3];
	}

	void set(Number k, void* v) {
		Ensure(k, (size_t)1 << INTERIOR_BITS);
		assert(k >> BITS == 0);
		const Number i1 = k >> (LEAF_BITS + INTERIOR_BITS);
		const Number i2 = (k >> LEAF_BITS) & (INTERIOR_LENGTH - 1);
		const Number i3 = k & (LEAF_LENGTH - 1);
		reinterpret_cast<Leaf*>(root_->ptrs[i1]->ptrs[i2])->values[i3] = v;
	}

	bool Ensure(Number start, size_t n) {
		// for (Number key = start; key <= start + n - 1;) {
		// 	const Number i1 = key >> (LEAF_BITS + INTERIOR_BITS);
		// 	const Number i2 = (key >> LEAF_BITS) & (INTERIOR_LENGTH - 1);

		// 	// Check for overflow
		// 	if (i1 >= INTERIOR_LENGTH || i2 >= INTERIOR_LENGTH)
		// 		return false;

		// 	// Make 2nd level node if necessary
		// 	if (root_->ptrs[i1] == NULL) {
		// 		Node* n = NewNode();
		// 		if (n == NULL) return false;
		// 		root_->ptrs[i1] = n;
		// 	}

		// 	// Make leaf node if necessary
		// 	if (root_->ptrs[i1]->ptrs[i2] == NULL) {
		// 		// Leaf* leaf = reinterpret_cast<Leaf*>((*allocator_)(sizeof(Leaf)));
		// 		// if (leaf == NULL) return false;
		// 		static ObjectPool<Leaf>	leafPool;
		// 		Leaf* leaf = (Leaf*)leafPool.New();

		// 		memset(leaf, 0, sizeof(*leaf));
		// 		root_->ptrs[i1]->ptrs[i2] = reinterpret_cast<Node*>(leaf);
		// 	}

		// 	// Advance key past whatever is covered by this leaf node
		// 	key = ((key >> LEAF_BITS) + 1) << LEAF_BITS;
		// }
		Number key = start;
		const Number i1 = key >> (LEAF_BITS + INTERIOR_BITS);
		const Number i2 = (key >> LEAF_BITS) & (INTERIOR_LENGTH - 1);

		// Check for overflow
		if (i1 >= INTERIOR_LENGTH || i2 >= INTERIOR_LENGTH)
			return false;

		// Make 2nd level node if necessary
		if (root_->ptrs[i1] == NULL) {
			Node* n = NewNode();
			if (n == NULL) 
			{
				assert(false);
				return false;
			}
			root_->ptrs[i1] = n;
		}

		// Make leaf node if necessary
		if (root_->ptrs[i1]->ptrs[i2] == NULL) {
			// Leaf* leaf = reinterpret_cast<Leaf*>((*allocator_)(sizeof(Leaf)));
			// if (leaf == NULL) return false;
			static ObjectPool<Leaf>	leafPool;
			Leaf* leaf = (Leaf*)leafPool.New();

			memset(leaf, 0, sizeof(*leaf));
			root_->ptrs[i1]->ptrs[i2] = reinterpret_cast<Node*>(leaf);
		}

		// Advance key past whatever is covered by this leaf node
		key = ((key >> LEAF_BITS) + 1) << LEAF_BITS;
		
		return true;
	}

	void PreallocateMoreMemory() {
		// Ensure(0, (size_t)1 << BITS);
	}
};