#ifndef ARRAY_H
#define ARRAY_H

//Copyright (C) 2025 Hongyi Chen (BeanPieChen)
//Licensed under the MIT License

#include <stdexcept>
#include "MAllocUtils.h"
#include "ContainerUtils.h"

#define BLOCKS_INCREASE_SIZE 128

template<typename T>
class Array {
	ContainerNode<T>** block_table = nullptr;
	size_t block_table_size = 0;
	size_t block_num = 0;
	size_t block_size = 0;
	size_t size = 0;
	Copy<T> copy;
	Deconstructor<T> deconstruct;

	MAllocF* malloc_f = nullptr;
	FreeF* free_f = nullptr;

	inline ContainerNode<T>** NewBlockTable(size_t size) {
		if (IS_NULL_POINTER(this->malloc_f))
			return reinterpret_cast<ContainerNode<T>**>(malloc(size * sizeof(ContainerNode<T>*)));
		else
			return reinterpret_cast<ContainerNode<T>**>(this->malloc_f(size * sizeof(ContainerNode<T>*)));

	}

	inline ContainerNode<T>* NewBlock() {
		ContainerNode<T>* ret;
		if (IS_NULL_POINTER(this->malloc_f))
			ret = reinterpret_cast<ContainerNode<T>*>(malloc(this->block_size * sizeof(ContainerNode<T>)));
		else
			ret = reinterpret_cast<ContainerNode<T>*>(this->malloc_f(this->block_size * sizeof(ContainerNode<T>)));
		for (size_t i = 0;i < this->block_size;++i)
			ret[i]._node.inited = false;
		return ret;
	}

	inline void DeleteBlockTable(ContainerNode<T>** block_table) {
		if (IS_NULL_POINTER(this->free_f))
			free(block_table);
		else
			this->free_f(block_table);
	}

	inline void DeleteBlock(ContainerNode<T>* block) {
		for (size_t i = 0;i < block_size;++i)
			this->deconstruct(block[i]);
		if (IS_NULL_POINTER(this->free_f))
			free(block);
		else
			this->free_f(block);
	}

	void _Clear() {
		this->size = 0;
		if (this->block_table != nullptr) {
			for (size_t i = 0;i < this->block_num;++i)
				delete[] this->block_table[i];
			delete[] this->block_table;
			this->block_table = nullptr;
			this->block_table_size = 0;
			this->block_num = 0;
		}
	}

	void _BlockTableResize(size_t size) {
		if (size == this->block_table_size)
			return;
		size_t actual_size = size / BLOCKS_INCREASE_SIZE + (size % BLOCKS_INCREASE_SIZE > 0 ? 1 : 0);
		if (actual_size == 0)
			actual_size = 1;
		actual_size *= BLOCKS_INCREASE_SIZE;
		if (actual_size == this->block_table_size)
			return;
		ContainerNode<T>** temp = this->NewBlockTable(actual_size);
		for (size_t i = 0;i < size;++i)
			temp[i] = this->block_table[i];
		this->DeleteBlockTable(this->block_table);
		this->block_table = temp;
		this->block_table_size = actual_size;
	}

public:
	Array(
		size_t block_size = 128, 
		MAllocF* malloc_f = nullptr, 
		FreeF* free_f = nullptr
	): 
		block_size(block_size), malloc_f(malloc_f), free_f(free_f) {
		this->Clear();
	}

	size_t GetCapacity() const {
		return this->block_num * this->block_size;
	}

	void SetCapacity(size_t cap) {
		size_t new_block_num = cap / this->block_size + (cap % this->block_size > 0 ? 1 : 0);
		if (new_block_num == 0)
			new_block_num = 1;
		if (new_block_num > this->block_num) {
			this->_BlockTableResize(new_block_num);
			size_t n = new_block_num - this->block_num;
			for (size_t i = this->block_num;i < new_block_num;++i)
				this->block_table[i] = this->NewBlock();
		}
		else if (new_block_num < this->block_num) {
			for (size_t i = new_block_num;i < this->block_num;++i)
				this->DeleteBlock(this->block_table[i]);
			this->_BlockTableResize(new_block_num);
		}
		this->block_num = new_block_num;
		if (cap < this->size)
			this->size = cap;
	}

	size_t GetSize() const {
		return this->size;
	}

	void Pop() {
		if (this->size > 0)
			--this->size;
	}
	
	void Push(const T& value) {
		this->Set(this->size, value);
	}

	void Clear() {
		this->_Clear();
		this->block_table = this->NewBlockTable(BLOCKS_INCREASE_SIZE);
		this->block_table_size = BLOCKS_INCREASE_SIZE;
		this->block_table[0] = this->NewBlock();
		this->block_num = 1;
	}

	T& Get(size_t index) const {
		if (index >= this->size)
			throw std::out_of_range("Array index out of range");
		return this->block_table[index / this->block_size][index % this->block_size]._node.data;
	}

	void Set(size_t index, const T& value) {
		if (index >= this->GetCapacity())
			this->SetCapacity(index + 1);
		if (index >= this->size)
			this->size = index + 1;
		this->copy(this->block_table[index / this->block_size][index % this->block_size], value);
	}

	void Insert(size_t index, const T& value) {
		if(index >= this->size)
			throw std::out_of_range("Array index out of range");
		for (size_t i = this->size - 1;i >= index;) {
			this->Set(i + 1, this->Get(i));
			if (i == 0)
				break;
			--i;
		}
		this->Set(index, value);
	}

	void Remove(size_t index) {
		if (index >= this->size)
			throw std::out_of_range("Array index out of range");
		for (size_t i = index + 1;i < this->size;++i)
			this->Set(i - 1, this->Get(i));
		--this->size;
	}

	~Array() {
		this->_Clear();
	}
};

#endif