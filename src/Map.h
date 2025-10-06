#ifndef MAP_H
#define MAP_H

//Copyright (C) 2025 Hongyi Chen (BeanPieChen)
//Licensed under the MIT License

#include <cstdint>
#include <climits>
#include <cmath>
#include "MAllocUtils.h"
#include "ContainerUtils.h"

uint32_t HashFloat(double f);

template<typename T>
class HashF {
public:
	uint32_t operator()(const T& value);
};


template<>
class HashF<uint16_t> {
public:
	uint32_t operator()(const uint16_t& value);
};


template<>
class HashF<uint32_t> {
public:
	uint32_t operator()(const uint32_t& value);
};


template<>
class HashF<uint64_t> {
public:
	uint32_t operator()(const uint64_t& value);
};


template<>
class HashF<int16_t> {
public:
	uint32_t operator()(const int16_t& value);
};


template<>
class HashF<int32_t> {
public:
	uint32_t operator()(const int32_t& value);
};


template<>
class HashF<int64_t> {
public:
	uint32_t operator()(const int64_t& value);
};


template<>
class HashF<float> {
public:
	uint32_t operator()(const float& value);
};


template<>
class HashF<double> {
public:
	uint32_t operator()(const double& value);
};


#define MAP_NODE(i) this->table[i]
#define MAP_NODE_FLAGS(i) MAP_NODE(i).flags
#define MAP_NODE_EMPTY(i) ((MAP_NODE_FLAGS(i)&0x1U)==0)
#define MAP_NODE_HAS_NEXT(i) ((MAP_NODE_FLAGS(i)&0x2U)!=0)
#define MAP_NODE_SET_EMPTY(i) MAP_NODE_FLAGS(i)&=(~0x1U)
#define MAP_NODE_SET_USED(i) MAP_NODE_FLAGS(i)|=0x1U
#define MAP_NODE_SET_HAS_NEXT(i) MAP_NODE_FLAGS(i)|=0x2U
#define MAP_NODE_SET_NO_NEXT(i) MAP_NODE_FLAGS(i)&=(~0x2U)
#define HASH_MOD(h,s)	(h % ((s-1)|1))

template<typename Tk,typename Tv>
class Map {
	Copy<Tk> kcopy;
	Deconstructor<Tk> kdeconstruct;
	Copy<Tv> vcopy;
	Deconstructor<Tv> vdeconstruct;

	struct Node {
		uint8_t flags = 0;
		size_t next = 0;
		ContainerNode<Tk> k;
		uint32_t hk;
		ContainerNode<Tv> v;

		Node(){}

		Node& operator==(const Node& right) {
			this->flags = right.flags;
			this->next = right.next;
			kcopy(this->k, right.k);
			this->hk = right.hk;
			vcopy(this->v, right.v);
			return this;
		}

		Node(const Node& obj) {
			this->operator==(obj);
		}
	};

	HashF<Tk> hash_f;
	Node* table = nullptr;
	size_t size;
	size_t last_empty;
	size_t num = 0;

	MAllocF* malloc_f = nullptr;
	FreeF* free_f = nullptr;

	Node* _NewTable(size_t len) {
		Node* ret;
		if (IS_NULL_POINTER(this->malloc_f))
			ret = reinterpret_cast<Node*>(malloc(len * sizeof(Node)));
		else
			ret = reinterpret_cast<Node*>(this->malloc_f(len * sizeof(Node)));
		for (size_t i = 0;i < len;++i)
			new(&(ret[i])) Node();
		return ret;
	}

	void _DeleteTable(Node* table, size_t len) {
		for (size_t i = 0;i < len;++i) {
			this->kdeconstruct(table[i].k);
			this->vdeconstruct(table[i].v);
		}
		if (IS_NULL_POINTER(this->free_f))
			free(table);
		else
			this->free_f(table);
	}

	size_t _FindEmptyNode() {
		while (!MAP_NODE_EMPTY(last_empty)) {
			if (last_empty > 0)
				--last_empty;
			else
				return this->size;
		}
		return last_empty;
	}

	void _Expand() {
		Node* t_old = this->table;
		size_t s_old = this->size;
		this->size *= 2;
		this->num = 0;
		this->last_empty = this->size - 1;
		this->table = this->_NewTable(this->size);
		for (size_t i = 0;i < s_old;++i) {
			if ((t_old[i].flags & 0x1U) == 0)
				continue;
			this->_Set(t_old[i].k._node.data, t_old[i].hk, t_old[i].v._node.data);
		}
		this->_DeleteTable(t_old, s_old);
	}

	void _Set(const Tk& key, uint32_t hk, const Tv& value) {
		size_t mp = HASH_MOD(hk, this->size);
		if (MAP_NODE_EMPTY(mp)) {
			this->kcopy(MAP_NODE(mp).k, key);
			MAP_NODE(mp).hk = hk;
			this->vcopy(MAP_NODE(mp).v, value);
			MAP_NODE_SET_USED(mp);
			++this->num;
		}
		else {
			if (MAP_NODE(mp).k._node.data == key) {
				this->vcopy(MAP_NODE(mp).v, value);
			}
			else {
				if (HASH_MOD(MAP_NODE(mp).hk, this->size) != mp) {
					size_t pre = HASH_MOD(MAP_NODE(mp).hk, this->size);
					while (MAP_NODE(pre).next != mp)
						pre = MAP_NODE(pre).next;
					size_t empty = this->_FindEmptyNode();
					if (empty >= this->size) {
						this->_Expand();
						this->_Set(key, hk, value);
						return;
					}
					MAP_NODE(pre).next = empty;
					MAP_NODE(empty) = MAP_NODE(mp);
					MAP_NODE_SET_USED(empty);
					MAP_NODE_FLAGS(mp) = 0;
					this->kcopy(MAP_NODE(mp).k, key);
					MAP_NODE(mp).hk = hk;
					this->vcopy(MAP_NODE(mp).v, value);
					MAP_NODE_SET_USED(mp);
				}
				else {
					while (MAP_NODE_HAS_NEXT(mp)) {
						mp = MAP_NODE(mp).next;
						if (MAP_NODE(mp).k._node.data == key) {
							this->vcopy(MAP_NODE(mp).v, value);
							return;
						}
					}
					size_t empty = this->_FindEmptyNode();
					if (empty >= this->size) {
						this->_Expand();
						this->_Set(key, hk, value);
						return;
					}
					MAP_NODE(mp).next = empty;
					MAP_NODE_SET_HAS_NEXT(mp);
					this->kcopy(MAP_NODE(empty).k, key);
					MAP_NODE(empty).hk = hk;
					this->vcopy(MAP_NODE(empty).v, value);
					MAP_NODE_SET_USED(empty);
				}
				++this->num;
			}
		}
	}

public:
	class NodeRef {
		Node* table = nullptr;
		size_t mp;

		NodeRef(Node* table, size_t mp):table(table),mp(mp){}
		friend class Map;

	public:
		class NullMapNodeRef :std::runtime_error {
		public:
			NullMapNodeRef() :std::runtime_error("Access to null map node reference") {}
		};

		bool IsNull() {
			return IS_NULL_POINTER(this->table);
		}

		Tv& Value() {
			if(IS_NULL_POINTER(this->table))
				throw NullMapNodeRef();
			return this->table[mp].v._node.data;
		}
	};

	class MapKeyError :std::runtime_error {
	public:
		MapKeyError() :std::runtime_error("Map key not exist") {}
	};

	Map(MAllocF* malloc_f = nullptr, FreeF* free_f = nullptr) :malloc_f(malloc_f), free_f(free_f) {
		this->Clear();
	}

	void Clear() {
		if(!IS_NULL_POINTER(this->table))
			this->_DeleteTable(this->table, this->size);
		this->table = this->_NewTable(512);
		this->size = 512;
		this->last_empty = this->size - 1;
		this->num = 0;
	}

	bool GetSize() const {
		return this->num;
	}

	NodeRef Find(const Tk& key) {
		uint32_t hk = this->hash_f(key);
		size_t mp = HASH_MOD(hk, this->size);
		while (!MAP_NODE_EMPTY(mp)) {
			if (MAP_NODE(mp).k._node.data == key)
				return NodeRef(this->table, mp);
			if (MAP_NODE_HAS_NEXT(mp))
				mp = MAP_NODE(mp).next;
			else
				break;
		}
		return NodeRef(nullptr, 0);
	}

	Tv& Get(const Tk& key) {
		NodeRef& ret = this->Find(key);
		if (ret.table == nullptr)
			throw MapKeyError("Map key error");
		else
			return MAP_NODE(ret.mp).v._node.data;
	}

	void Set(const Tk& key, const Tv& value) {
		this->_Set(key, this->hash_f(key), value);
	}

	void Remove(const Tk& key) {
		NodeRef node = this->Get(key);
		if (IS_NULL_POINTER(node.table))
			return;
		size_t rm = node.mp;
		if (HASH_MOD(MAP_NODE(rm).hk, this->size) == rm) {
			if (MAP_NODE_HAS_NEXT(rm)) {
				rm = MAP_NODE(rm).next;
				MAP_NODE(node.mp) = MAP_NODE(rm);
			}
		}
		else {
			size_t pre = HASH_MOD(MAP_NODE(rm).hk, this->size);
			while (!MAP_NODE(pre).next == rm)
				pre = MAP_NODE(pre).next;
			if (MAP_NODE_HAS_NEXT(rm))
				MAP_NODE(pre).next = MAP_NODE(rm).next;
			else
				MAP_NODE_SET_NO_NEXT(pre);
		}
		MAP_NODE(rm).flags = 0;
		if (rm > this->last_empty)
			last_empty = rm;
		--this->num;
	}

	~Map() {
		this->_DeleteTable(this->table, this->size);
	}
};

#endif