//Copyright (C) 2025 Hongyi Chen (BeanPieChen)
//Licensed under the MIT License

#include "Map.h"

uint32_t HashFloat(double f) {
	int i;
	f = frexp(f, &i) * -static_cast<double>(INT_MIN);
	if (!(f == f) || fabs(f) == static_cast<double>(INFINITY))
		return 0;
	else
		return static_cast<uint32_t>(i) + static_cast<uint32_t>(f);
}


uint32_t HashF<uint16_t>::operator()(const uint16_t& value) {
	return value;
}


uint32_t HashF<uint32_t>::operator()(const uint32_t& value) {
	return value;
}


uint32_t HashF<uint64_t>::operator()(const uint64_t& value) {
	uint32_t ret = value & 0xFFFFFFFF;
	ret += value << 32;
	return ret;
}


uint32_t HashF<int16_t>::operator()(const int16_t& value) {
	return static_cast<uint32_t>(value);
}


uint32_t HashF<int32_t>::operator()(const int32_t& value) {
	return static_cast<uint32_t>(value);
}


uint32_t HashF<int64_t>::operator()(const int64_t& value) {
	uint32_t ret = static_cast<uint64_t>(value) & 0xFFFFFFFF;
	ret += static_cast<uint64_t>(value) << 32;
	return ret;
}


uint32_t HashF<float>::operator()(const float& value) {
	return HashFloat(static_cast<double>(value));
}


uint32_t HashF<double>::operator()(const double& value) {
	return HashFloat(value);
}