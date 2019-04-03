#ifndef RASTER_TYPEJUGGLING_H
#define RASTER_TYPEJUGGLING_H 1

#include "datatypes/raster/raster_priv.h"


template< template<typename T> class func, typename... V>
auto callUnaryOperatorFunc(GenericRaster *raster, V... v)
	-> decltype(func<uint8_t>::execute(nullptr, v...)) {
	switch(raster->dd.datatype) {
		case GDT_Byte:
			return func<uint8_t>::execute((Raster2D<uint8_t> *) raster, v...);
		case GDT_Int16:
			return func<int16_t>::execute((Raster2D<int16_t> *) raster, v...);
		case GDT_UInt16:
			return func<uint16_t>::execute((Raster2D<uint16_t> *) raster, v...);
		case GDT_Int32:
			return func<int32_t>::execute((Raster2D<int32_t> *) raster, v...);
		case GDT_UInt32:
			return func<uint32_t>::execute((Raster2D<uint32_t> *) raster, v...);
		case GDT_Float32:
			return func<float>::execute((Raster2D<float> *) raster, v...);
		case GDT_Float64:
			return func<double>::execute((Raster2D<double> *) raster, v...);
		default:
			throw MetadataException("Cannot call operator with this data type");
	}
}


template< template<typename T1, typename T2> class func, typename T, typename... V>
auto callBinaryOperatorFunc2(Raster2D<T> *raster1, GenericRaster *raster2, V... v)
	-> decltype(func<uint8_t, uint8_t>::execute(nullptr,nullptr, v...)) {
	switch(raster2->dd.datatype) {
		case GDT_Byte:
			return func<T, uint8_t>::execute(raster1, (Raster2D<uint8_t> *) raster2, v...);
		case GDT_Int16:
			return func<T, int16_t>::execute(raster1, (Raster2D<int16_t> *) raster2, v...);
		case GDT_UInt16:
			return func<T, uint16_t>::execute(raster1, (Raster2D<uint16_t> *) raster2, v...);
		case GDT_Int32:
			return func<T, int32_t>::execute(raster1, (Raster2D<int32_t> *) raster2, v...);
		case GDT_UInt32:
			return func<T, uint32_t>::execute(raster1, (Raster2D<uint32_t> *) raster2, v...);
		case GDT_Float32:
			return func<T, float>::execute(raster1, (Raster2D<float> *) raster2, v...);
		case GDT_Float64:
			return func<T, double>::execute(raster1, (Raster2D<double> *) raster2, v...);
		default:
			throw MetadataException("Cannot call operator with this data type");
	}
}


template< template<typename T1, typename T2> class func, typename... V>
auto callBinaryOperatorFunc(GenericRaster *raster1, GenericRaster *raster2, V... v)
	-> decltype(func<uint8_t, uint8_t>::execute(nullptr,nullptr, v...)) {
	switch(raster1->dd.datatype) {
		case GDT_Byte:
			return callBinaryOperatorFunc2<func, uint8_t>((Raster2D<uint8_t> *) raster1, raster2, v...);
		case GDT_Int16:
			return callBinaryOperatorFunc2<func, int16_t>((Raster2D<int16_t> *) raster1, raster2, v...);
		case GDT_UInt16:
			return callBinaryOperatorFunc2<func, uint16_t>((Raster2D<uint16_t> *) raster1, raster2, v...);
		case GDT_Int32:
			return callBinaryOperatorFunc2<func, int32_t>((Raster2D<int32_t> *) raster1, raster2, v...);
		case GDT_UInt32:
			return callBinaryOperatorFunc2<func, uint32_t>((Raster2D<uint32_t> *) raster1, raster2, v...);
		case GDT_Float32:
			return callBinaryOperatorFunc2<func, float>((Raster2D<float> *) raster1, raster2, v...);
		case GDT_Float64:
			return callBinaryOperatorFunc2<func, double>((Raster2D<double> *) raster1, raster2, v...);
		default:
			throw MetadataException("Cannot call operator with this data type");
	}
}

template<template<typename T1, typename T2, typename T3> class function, typename Ta, typename Tb, typename... V>
static auto callTernaryOperatorFunc3(Raster2D<Ta> *raster1, Raster2D<Tb> *raster2, GenericRaster *raster3, V... v)
-> decltype(function<uint8_t, uint8_t, uint8_t >::execute(nullptr, nullptr, nullptr, v...)) {
	switch (raster3->dd.datatype) {
		case GDT_Byte:
			return function<Ta, Tb, uint8_t>::execute(raster1, raster2, (Raster2D<uint8_t> *) raster3, v...);
		case GDT_Int16:
			return function<Ta, Tb, int16_t>::execute(raster1, raster2, (Raster2D<int16_t> *) raster3, v...);
		case GDT_UInt16:
			return function<Ta, Tb, uint16_t>::execute(raster1, raster2, (Raster2D<uint16_t> *) raster3, v...);
		case GDT_Int32:
			return function<Ta, Tb, int32_t>::execute(raster1, raster2, (Raster2D<int32_t> *) raster3, v...);
		case GDT_UInt32:
			return function<Ta, Tb, uint32_t>::execute(raster1, raster2, (Raster2D<uint32_t> *) raster3, v...);
		case GDT_Float32:
			return function<Ta, Tb, float>::execute(raster1, raster2, (Raster2D<float> *) raster3, v...);
		case GDT_Float64:
			return function<Ta, Tb, double>::execute(raster1, raster2, (Raster2D<double> *) raster3, v...);
		default:
			throw std::runtime_error("Cannot call operator with this data type");
	}
}

template<template<typename T1, typename T2, typename T3> class function, typename Ta, typename... V>
static auto callTernaryOperatorFunc2(Raster2D<Ta> *raster1, GenericRaster *raster2, GenericRaster *raster3, V... v)
-> decltype(function<uint8_t, uint8_t, uint8_t>::execute(nullptr, nullptr, nullptr, v...)) {
	switch (raster1->dd.datatype) {
		case GDT_Byte:
			return callTernaryOperatorFunc3<function, Ta, uint8_t>(raster1, (Raster2D<uint8_t> *) raster2, raster3, v...);
		case GDT_Int16:
			return callTernaryOperatorFunc3<function, Ta, int16_t>(raster1, (Raster2D<int16_t> *) raster2, raster3, v...);
		case GDT_UInt16:
			return callTernaryOperatorFunc3<function, Ta, uint16_t>(raster1, (Raster2D<uint16_t> *) raster2, raster3, v...);
		case GDT_Int32:
			return callTernaryOperatorFunc3<function, Ta, int32_t>(raster1, (Raster2D<int32_t> *) raster2, raster3, v...);
		case GDT_UInt32:
			return callTernaryOperatorFunc3<function, Ta, uint32_t>(raster1, (Raster2D<uint32_t> *) raster2, raster3, v...);
		case GDT_Float32:
			return callTernaryOperatorFunc3<function, Ta, float>(raster1, (Raster2D<float> *) raster2, raster3, v...);
		case GDT_Float64:
			return callTernaryOperatorFunc3<function, Ta, double>(raster1, (Raster2D<double> *) raster2, raster3, v...);
		default:
			throw std::runtime_error("Cannot call operator with this data type");
	}
}

template<template<typename T1, typename T2, typename T3> class function, typename... V>
static auto callTernaryOperatorFunc(GenericRaster*raster1, GenericRaster *raster2, GenericRaster *raster3, V... v)
-> decltype(function<uint8_t, uint8_t, uint8_t>::execute(nullptr, nullptr, nullptr, v...)) {
	switch (raster1->dd.datatype) {
		case GDT_Byte:
			return callTernaryOperatorFunc2<function, uint8_t>((Raster2D<uint8_t> *) raster1, raster2, raster3, v...);
		case GDT_Int16:
			return callTernaryOperatorFunc2<function, int16_t>((Raster2D<int16_t> *) raster1, raster2, raster3, v...);
		case GDT_UInt16:
			return callTernaryOperatorFunc2<function, uint16_t>((Raster2D<uint16_t> *) raster1, raster2, raster3, v...);
		case GDT_Int32:
			return callTernaryOperatorFunc2<function, int32_t>((Raster2D<int32_t> *) raster1, raster2, raster3, v...);
		case GDT_UInt32:
			return callTernaryOperatorFunc2<function, uint32_t>((Raster2D<uint32_t> *) raster1, raster2, raster3, v...);
		case GDT_Float32:
			return callTernaryOperatorFunc2<function, float>((Raster2D<float> *) raster1, raster2, raster3, v...);
		case GDT_Float64:
			return callTernaryOperatorFunc2<function, double>((Raster2D<double> *) raster1, raster2, raster3, v...);
		default:
			throw std::runtime_error("Cannot call operator with this data type");
	}
}


template<typename T> struct RasterTypeInfo {
	static const GDALDataType type = GDT_Unknown;
};
template<> struct RasterTypeInfo<uint8_t> {
	static const GDALDataType type = GDT_Byte;
	static constexpr const char *cltypename = "uchar";
	static const bool isinteger = true;
	static const bool issigned = false;
	typedef uint32_t accumulator_type;
	typedef int32_t signed_accumulator_type;
	static uint16_t getRange(uint8_t min, uint8_t max) { return (uint16_t) max-min+1; } // 1 .. 256, thus uint_16_t
};
template<> struct RasterTypeInfo<int16_t> {
	static const GDALDataType type = GDT_Int16;
	static constexpr const char *cltypename = "short";
	static const bool isinteger = true;
	static const bool issigned = true;
	typedef int64_t accumulator_type;
	typedef int64_t signed_accumulator_type;
	static uint32_t getRange(int16_t min, int16_t max) { return (int32_t) max-min+1; }
};
template<> struct RasterTypeInfo<uint16_t> {
	static const GDALDataType type = GDT_UInt16;
	static constexpr const char *cltypename = "ushort";
	static const bool isinteger = true;
	static const bool issigned = false;
	typedef uint64_t accumulator_type;
	typedef int64_t signed_accumulator_type;
	static uint32_t getRange(uint16_t min, uint16_t max) { return (uint32_t) max-min+1; }
};
template<> struct RasterTypeInfo<int32_t> {
	static const GDALDataType type = GDT_Int32;
	static constexpr const char *cltypename = "int";
	static const bool isinteger = true;
	static const bool issigned = true;
	typedef int64_t accumulator_type;
	typedef int64_t signed_accumulator_type;
	static uint64_t getRange(int32_t min, int32_t max) { return (int64_t) max-min+1; }
};
template<> struct RasterTypeInfo<uint32_t> {
	static const GDALDataType type = GDT_UInt32;
	static constexpr const char *cltypename = "uint";
	static const bool isinteger = true;
	static const bool issigned = false;
	typedef uint64_t accumulator_type;
	typedef int64_t signed_accumulator_type;
	static uint64_t getRange(uint32_t min, uint32_t max) { return (uint64_t) max-min+1; }
};
template<> struct RasterTypeInfo<float> {
	static const GDALDataType type = GDT_Float32;
	static constexpr const char *cltypename = "float";
	static const bool isinteger = false;
	static const bool issigned = true;
	typedef double accumulator_type;
	typedef double signed_accumulator_type;
	static double getRange(float min, float max) { return (double) max-min; }
};
template<> struct RasterTypeInfo<double> {
	static const GDALDataType type = GDT_Float64;
	static constexpr const char *cltypename = "double";
	static const bool isinteger = false;
	static const bool issigned = true;
	typedef double accumulator_type;
	typedef double signed_accumulator_type;
	static double getRange(double min, double max) { return (double) max-min; }
};





#endif
