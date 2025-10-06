#ifndef CONTAINER_UTILS
#define CONTAINER_UTILS

//Copyright (C) 2025 Hongyi Chen (BeanPieChen)
//Licensed under the MIT License

#include <type_traits>

template<typename T,bool f>
struct _ContainerNode {

};


template<typename T>
struct _ContainerNode<T,true> {
	T data;
};


template<typename T>
struct _ContainerNode<T, false> {
	bool inited;
	T data;
};


template<typename T>
struct ContainerNode {
	_ContainerNode<T, std::is_fundamental<T>::value&& std::is_pointer<T>::value> _node;
};


template<typename T, bool f>
class _Copy {

};


template<typename T>
class _Copy<T, true> {
public:
	void operator()(ContainerNode<T>& node, const T& other) {
		node._node.data = other;
	}
};

template<typename T>
class _Copy<T,false> {
public:
	void operator()(ContainerNode<T>& node, const T& other) {
		if (node._node.inited)
			node._node.data = other;
		else
			new(&(node._node.data)) T(other);
		node._node.inited = true;
	}
};


template<typename T>
class Copy {
	_Copy<T, std::is_fundamental<T>::value&& std::is_pointer<T>::value> copy;

public:
	void operator()(ContainerNode<T>& node, const T& other) {
		this->copy(node, other);
	}
};


template<typename T, bool f>
class _Deconstructor {

};


template<typename T>
class _Deconstructor<T, true> {
public:
	void operator()(ContainerNode<T>& node) {
		//Do nothing
	}
};


template<typename T>
class _Deconstructor<T, false> {
public:
	void operator()(ContainerNode<T>& node) {
		if (node._node.inited) {
			node._node.data.~T();
			node._node.inited = false;
		}
	}
};


template<typename T>
class Deconstructor {
	_Deconstructor<T, std::is_fundamental<T>::value&& std::is_pointer<T>::value> deconstructor;

public:
	void operator()(ContainerNode<T>& obj) {
		this->deconstructor(obj);
	}
};
#endif 