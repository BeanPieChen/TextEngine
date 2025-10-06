#ifndef LIST_H
#define LIST_H

//Copyright (C) 2025 Hongyi Chen (BeanPieChen)
//Licensed under the MIT License

#include <stdexcept>
#include <cstddef>
#include "MAllocUtils.h"
#include "ContainerUtils.h"

template<typename T>
class List {
private:
	Copy<T> copy;
	Deconstructor<T> deconstruct;

	struct Node {
		ContainerNode<T> data;
		Node* pre = nullptr;
		Node* next = nullptr;

		Node() {}

		Node& operator==(const Node& right) {
			copy(this->data, right.data);
			this->pre = right.pre;
			this->next = right.next;
			return this;
		}

		Node(const Node& obj) {
			this->operator==(obj);
		}
	};

	class NullListNodeRef :std::runtime_error {
	public:
		NullListNodeRef(const std::string& message) :std::runtime_error(message) {}
	};

	size_t size = 0;

public:
	class NodeRef {
		Node* node = nullptr;
		
		NodeRef(Node* node) {
			this->node = node;
		}

		friend class List;

	public:
		bool Pre() {
			if (IS_NULL_POINTER(this->node))
				return false;
			this->node = this->node->pre;
			return IS_NULL_POINTER(this->node);
		}

		bool Next() {
			if (IS_NULL_POINTER(this->node))
				return false;
			this->node = this->node->next;
			return IS_NULL_POINTER(this->node);
		}

		bool IsNull() const {
			return IS_NULL_POINTER(this->node);
		}

		T& Data() {
			if (IS_NULL_POINTER(this->node))
				throw NullListNodeRef("Accessing null list node reference.");
			return this->node->data._node.data;
		}

		bool operator==(const NodeRef& right) const {
			if (IS_NULL_POINTER(this->node) && IS_NULL_POINTER(right.node))
				return true;
			return this->node == right.node;
		}

		bool operator!=(const NodeRef& right) const {
			return !this->operator==(right);
		}
	};

private:
	MAllocF* malloc_f = nullptr;
	FreeF* free_f = nullptr;
	Node* head = nullptr;
	Node* tail = nullptr;

	inline Node* _NewNode() {
		Node* ret;
		if (IS_NULL_POINTER(this->malloc_f))
			ret = reinterpret_cast<Node*>(malloc(sizeof(Node)));
		else
			Node* ret = reinterpret_cast<Node*>(this->malloc_f(sizeof(Node)));
		ret->next = nullptr;
		ret->pre = nullptr;
		return ret;
	}

	inline void _DeleteNode(Node* node) {
		this->deconstruct(node->data);
		if (IS_NULL_POINTER(this->free_f))
			free(node);
		else
			return this->free_f(node);
	}

public:
	List(MAllocF* malloc_f = nullptr, FreeF* free_f = nullptr):malloc_f(malloc_f),free_f(free_f) {}

	void Clear() {
		Node* p = head;
		if (!IS_NULL_POINTER(head)) {
			Node* temp = p;
			p = p->next;
			this->_DeleteNode(temp);
		}
		this->head = nullptr;
		this->tail = nullptr;
		size = 0;
	}

	bool IsEmpty() const {
		return IS_NULL_POINTER(this->head);
	}

	size_t GetSize() const {
		return this->size;
	}

	NodeRef GetFront() {
		return NodeRef(this->head);
	}

	NodeRef GetBack() {
		return NodeRef(this->tail);
	}

	void PopFront() {
		if (IS_NULL_POINTER(this->head))
			return;
		Node* temp = this->head;
		this->head = this->head->next;
		if (IS_NULL_POINTER(this->head))
			this->tail = nullptr;
		else
			this->head->pre = nullptr;
		this->_DeleteNode(temp);
		--this->size;
	}

	void PopBack() {
		if (IS_NULL_POINTER(this->tail))
			return;
		Node* temp = this->tail;
		this->tail = this->tail->pre;
		if (IS_NULL_POINTER(this->tail))
			this->head = nullptr;
		else
			this->tail->next = nullptr;
		this->_DeleteNode(temp);
		--this->size;
	}

	void PushFront(const T& data) {
		Node* temp = this->_NewNode();
		this->copy(temp->data, data);
		temp->data = data;
		if (IS_NULL_POINTER(this->head))
			this->tail = temp;
		else {
			temp->next = this->head;
			this->head->pre = temp;
		}
		this->head = temp;
		++this->size;
	}

	void PushBack(const T& data) {
		Node* temp = this->_NewNode();
		this->copy(temp->data, data);
		if (IS_NULL_POINTER(this->tail))
			this->head = temp;
		else {
			temp->pre = this->tail;
			this->tail->next = temp;
		}
		this->tail = temp;
		++this->size;
	}

	void InsertBefore(const NodeRef& node, const T& value) {
		Node* temp = this->_NewNode();
		this->copy(temp->data, value);
		temp->next = node.node;
		temp->pre = node.node->pre;
		if (IS_NULL_POINTER(node.node->pre))
			this->head = temp;
		else
			node.node->pre->next = temp;
		node.node->pre = temp;
		++this->size;
	}

	void InsertAfter(const NodeRef& node, const T& value) {
		Node* temp = this->_NewNode();
		this->copy(temp->data, value);
		temp->next = node.node->next;
		temp->pre = node.node;
		if (IS_NULL_POINTER(node.node->next))
			this->tail = temp;
		else
			node.node->next->pre = temp;
		node.node->next = temp;
		++this->size;
	}

	void Remove(NodeRef& node) {
		if (IS_NULL_POINTER(node))
			return;
		if (IS_NULL_POINTER(node.node->pre))
			this->head = node.node->next;
		else
			node.node->pre->next = node.node->next;
		if (IS_NULL_POINTER(node.node->next))
			this->tail = node.node->pre;
		else
			node.node->next->pre = node.node->pre;
		this->_DeleteNode(node.node);
		node.node = nullptr;
		--this->size;
	}

	~List() {
		this->Clear();
	}
};

#endif