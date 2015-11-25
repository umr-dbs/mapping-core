/*
 * cache_structure.h
 *
 *  Created on: 09.08.2015
 *      Author: mika
 */

#ifndef CACHE_STRUCTURE_H_
#define CACHE_STRUCTURE_H_

#include "operators/queryrectangle.h"
#include "datatypes/spatiotemporal.h"
#include "cache/priv/cube.h"
#include "util/binarystream.h"
#include "cache/common.h"

#include <map>
#include <queue>
#include <memory>
#include <mutex>

class ResolutionInfo {
public:
	ResolutionInfo();
	ResolutionInfo( const GridSpatioTemporalResult &result );
	ResolutionInfo(BinaryStream &stream);

	bool matches( const QueryRectangle &query );

	void toStream(BinaryStream &stream) const;

	QueryResolution::Type restype;
	Interval pixel_scale_x;
	Interval pixel_scale_y;
	double  actual_pixel_scale_x;
	double  actual_pixel_scale_y;
};


class QueryCube : public Cube3 {
public:
	QueryCube( const QueryRectangle &rect );
	QueryCube( const SpatialReference &sref, const TemporalReference &tref );
	QueryCube( BinaryStream &stream );

	void toStream( BinaryStream &stream ) const;

	epsg_t epsg;
	timetype_t timetype;
};

class CacheCube : public QueryCube {
public:
	CacheCube( const SpatialReference &sref, const TemporalReference &tref );
	CacheCube( const SpatioTemporalResult &result );
	CacheCube( const GridSpatioTemporalResult & result );
	CacheCube( const GenericPlot &result );
	CacheCube( BinaryStream & stream );

	const Interval& get_timespan() const;

	void toStream( BinaryStream &stream ) const;

	ResolutionInfo resolution_info;
};

// Information about data-access
class AccessInfo {
public:
	AccessInfo();
	AccessInfo( time_t last_access, uint32_t access_count );
	AccessInfo( BinaryStream &stream );

	void toStream( BinaryStream &stream ) const;

	time_t last_access;
	uint32_t access_count;
};

//
// Basic information about cached data
//
class CacheEntry : public AccessInfo {
public:
	CacheEntry(CacheCube bounds, uint64_t size);
	CacheEntry(CacheCube bounds, uint64_t size, time_t last_access, uint32_t access_count);
	CacheEntry( BinaryStream &stream );

	void toStream( BinaryStream &stream ) const;

	std::string to_string() const;

	CacheCube bounds;
	uint64_t size;
};

//
// Info about the coverage of a single entry in a cache-query
//
template<typename KType>
class CacheQueryInfo {
public:
	CacheQueryInfo( double score, CacheCube cube, KType key);
	bool operator <(const CacheQueryInfo &b) const;

	// a string representation
	std::string to_string() const;

	// scoring
	double score;

	// BBox of entry
	CacheCube cube;

	KType key;
};

//
// Result of a cache-query.
// Holds two polygons:
// - The area covered by cache-entries
// - The remainder area
template<typename KType>
class CacheQueryResult {
public:

	// Constructs an empty result with the given query-rectangle as remainder
	CacheQueryResult( const QueryRectangle &query );
	CacheQueryResult( const QueryRectangle &query, std::vector<Cube<3>> remainder, std::vector<KType> keys );

	bool has_hit() const;
	bool has_remainder() const;
	std::string to_string() const;

	std::vector<KType> keys;
	std::vector<Cube<3>> remainder;
	double coverage;
};

//
// Unique key generated for an entry in the cache
//
class NodeCacheKey {
public:
	NodeCacheKey( const std::string &semantic_id, uint64_t entry_id );
	NodeCacheKey( BinaryStream &stream );

	void toStream( BinaryStream &stream ) const;

	std::string to_string() const;

	std::string semantic_id;
	uint64_t entry_id;
};

class TypedNodeCacheKey : public NodeCacheKey {
public:
	TypedNodeCacheKey( CacheType type, const std::string &semantic_id, uint64_t entry_id );
	TypedNodeCacheKey( BinaryStream &stream );

	void toStream( BinaryStream &stream ) const;

	std::string to_string() const;

	CacheType type;
};

//
// Reference to a cache-entry on the node
//
class NodeCacheRef : public TypedNodeCacheKey, public CacheEntry {
public:
	NodeCacheRef( const TypedNodeCacheKey &key, const CacheEntry &entry );
	NodeCacheRef( CacheType type, const NodeCacheKey &key, const CacheEntry &entry );
	NodeCacheRef( CacheType type, const std::string semantic_id, uint64_t entry_id, const CacheEntry &entry );
	NodeCacheRef( BinaryStream &stream );

	void toStream(BinaryStream &stream) const;

	std::string to_string() const;
};

//
// The basic structure used for caching
//
template<typename KType, typename EType>
class CacheStructure {
public:
	// Inserts the given result into the cache. The cache copies the content of the result.
	void put( const KType &key, const std::shared_ptr<EType> &result );

	// Fetches the entry by the given id. The result is read-only
	// and not copied.
	std::shared_ptr<EType> get( const KType &key ) const;

	// Queries the cache with the given query-rectangle
	const CacheQueryResult<KType> query( const QueryRectangle &spec ) const;

	// Removes the entry with the given id
	std::shared_ptr<EType> remove( const KType &key );

	std::vector<std::shared_ptr<EType>> get_all() const;

private:
	std::string key_to_string( uint64_t key ) const;
	std::string key_to_string( const std::pair<uint32_t,uint64_t> &key ) const;
	std::map<KType, std::shared_ptr<EType>> entries;
	std::priority_queue<CacheQueryInfo<KType>> get_query_candidates( const QueryRectangle &spec ) const;
	mutable std::mutex mtx;
};

#endif /* CACHE_STRUCTURE_H_ */
