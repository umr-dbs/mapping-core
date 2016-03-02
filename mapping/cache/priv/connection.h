/*
 * connection.h
 *
 *  Created on: 23.06.2015
 *      Author: mika
 */

#ifndef CONNECTION_H_
#define CONNECTION_H_

#include "util/binarystream.h"
#include "cache/node/node_cache.h"
#include "cache/priv/cache_stats.h"
#include "cache/priv/transfer.h"
#include "cache/priv/redistribution.h"
#include "cache/common.h"

#include <map>
#include <memory>

class Node;


class NewConnection {
public:
	NewConnection( const std::string &hostname, int fd );
	int read_fd() const;
	bool read();
	BinaryStream& get_data();
	std::unique_ptr<BinaryFDStream> release_stream();
	std::string hostname;
private:
	std::unique_ptr<BinaryFDStream> stream;
	std::unique_ptr<BinaryReadBuffer> buffer;
};

template<typename StateType>
class BaseConnection {
public:
	BaseConnection(StateType state, std::unique_ptr<BinaryFDStream> socket);
	virtual ~BaseConnection();
	// Called if data is available on the unerlying socket and this connection is not in writing mode
	bool input();
	// Called if data can be written to the unerlying socket and this connection is in writing-mode
	void output();

	// Returns the fd used for writes by this connection
	int get_write_fd() const;
	// Returns the fd used for reads by this connection
	int get_read_fd() const;
	// Tells whether an error occured on this connection
	bool is_faulty() const;
	// Tells whether this connection is currently writing data
	bool is_writing() const;

	StateType get_state() const;

	// This connection's id
	const uint64_t id;
protected:
	// Callback for commands read from the socket
	virtual void process_command( uint8_t cmd, BinaryStream& payload ) = 0;
	// Called by implementing classes to trigger a non-blocking write
	void begin_write( std::unique_ptr<BinaryWriteBuffer> buffer );
	// Callback for finished non-blocking writes
	virtual void write_finished() = 0;

	void set_state( StateType state );

	template<typename... States>
	void ensure_state( const States... states ) const;
private:
	template<typename... States>
	bool _ensure_state( StateType state, States... states) const;
	bool _ensure_state( StateType state ) const;
	StateType state;
	bool faulty;
	std::unique_ptr<BinaryFDStream> socket;
	std::unique_ptr<BinaryReadBuffer> reader;
	std::unique_ptr<BinaryWriteBuffer> writer;
	static uint64_t next_id;
};

enum class ClientState {
	IDLE, AWAIT_RESPONSE, WRITING_RESPONSE
};

class ClientConnection: public BaseConnection<ClientState> {
public:


	static const uint32_t MAGIC_NUMBER = 0x22345678;

	//
	// Expected data on stream is:
	// BaseRequest
	//
	static const uint8_t CMD_GET = 1;

	//
	// Response from index-server after successfully
	// processing a request. Data on stream is:
	// DeliveryResponse
	static const uint8_t RESP_OK = 10;

	//
	// Returned on errors by the index-server.
	// Data on stream is:
	// message:string -- a description of the error
	static const uint8_t RESP_ERROR = 19;

	ClientConnection(std::unique_ptr<BinaryFDStream> socket);
	virtual ~ClientConnection();

	// Sends the given response and resets the state to IDLE
	void send_response(const DeliveryResponse &response);
	// Sends the given error and resets the state to IDLE
	void send_error(const std::string &message);

	const BaseRequest& get_request() const;

protected:
	virtual void process_command( uint8_t cmd, BinaryStream& payload );
	virtual void write_finished();
private:
	std::unique_ptr<BaseRequest> request;
};



enum class WorkerState {
	IDLE,
	SENDING_REQUEST, PROCESSING, NEW_ENTRY,
	QUERY_REQUESTED, SENDING_QUERY_RESPONSE,
	DONE,
	SENDING_DELIVERY_QTY, WAITING_DELIVERY, DELIVERY_READY,
	ERROR
};


class WorkerConnection: public BaseConnection<WorkerState> {
public:

	static const uint32_t MAGIC_NUMBER = 0x32345678;

	//
	// Expected data on stream is:
	// request:BaseRequest
	//
	static const uint8_t CMD_CREATE = 20;

	//
	// Expected data on stream is:
	// request:DeliveryRequest
	//
	static const uint8_t CMD_DELIVER = 21;

	//
	// Expected data on stream is:
	// request:PuzzleRequest
	//
	static const uint8_t CMD_PUZZLE = 22;

	//
	// Expected data on stream is:
	// BaseRequest
	//
	static const uint8_t CMD_QUERY_CACHE = 23;

	//
	// Response from worker to signal finished computation
	//
	static const uint8_t RESP_RESULT_READY = 30;

	//
	// Response from worker to signal ready to deliver result
	// Data on stream is:
	// DeliveryResponse
	//
	static const uint8_t RESP_DELIVERY_READY = 31;

	//
	// Send if a new raster-entry is added to the local cache
	// Data on stream is:
	// key:STCacheKey
	// cube:RasterCacheCube
	//
	static const uint8_t RESP_NEW_CACHE_ENTRY = 32;

	//
	// Response from index-server after successfully
	// probing the cache for a RESP_QUERY_RASTER_CACHE.
	// Data on stream is:
	// ref:CacheRef
	static const uint8_t RESP_QUERY_HIT = 33;

	//
	// Response from index-server after unsuccessfuly
	// probing the cache for a CMD_INDEX_QUERY_CACHE.
	// Theres no data on stream.
	static const uint8_t RESP_QUERY_MISS = 34;

	//
	// Response from index-server after successfully
	// probing the cache for a CMD_INDEX_QUERY_CACHE.
	// Data on stream is:
	// puzzle-request: PuzzleRequest
	static const uint8_t RESP_QUERY_PARTIAL = 36;

	//
	// Response from index to tell delivery qty
	// Data on stream:
	// qty:uint32_t
	//
	static const uint8_t RESP_DELIVERY_QTY = 37;

	//
	// Send if a worker cannot fulfill the request
	// Data on stream is:
	// message:string -- a description of the error
	static const uint8_t RESP_ERROR = 39;

	WorkerConnection(std::unique_ptr<BinaryFDStream> socket, const std::shared_ptr<Node> &node);
	virtual ~WorkerConnection();

	void process_request(uint8_t command, const BaseRequest &request);
	void entry_cached();
	void send_hit( const CacheRef &cr );
	void send_partial_hit( const PuzzleRequest &pr );
	void send_miss();
	void send_delivery_qty(uint32_t qty);
	void release();

	const NodeCacheRef& get_new_entry() const;
	const BaseRequest& get_query() const;

	const DeliveryResponse &get_result() const;
	const std::string &get_error_message() const;

	const std::shared_ptr<Node> node;

protected:
	virtual void process_command( uint8_t cmd, BinaryStream &payload );
	virtual void write_finished();
private:
	void reset();
	std::unique_ptr<DeliveryResponse> result;
	std::unique_ptr<NodeCacheRef> new_entry;
	std::unique_ptr<BaseRequest> query;
	std::string error_msg;
};




enum class ControlState {
	SENDING_HELLO, IDLE,
	SENDING_REORG, REORGANIZING,
	READING_MOVE_RESULT, MOVE_RESULT_READ, SENDING_MOVE_CONFIRM,
	READING_REMOVE_REQUEST, REMOVE_REQUEST_READ, SENDING_REMOVE_CONFIRM,
	REORG_FINISHED,
	SENDING_STATS_REQUEST, STATS_REQUESTED, READING_STATS, STATS_RECEIVED
};


class ControlConnection: public BaseConnection<ControlState> {
public:
	static const uint32_t MAGIC_NUMBER = 0x42345678;

	//
	// Tells the node to fetch the attached
	// items and store them in its local cache
	// ReorgDescription
	//
	static const uint8_t CMD_REORG = 40;

	//
	// Tells the node to send an update
	// of the local cache stats
	//
	static const uint8_t CMD_GET_STATS = 41;

	//
	// Tells the node that the move on index was OK
	// There is no data on stream
	//
	static const uint8_t CMD_MOVE_OK = 42;

	//
	// Tells the node that the given item can
	// safely be removed from the cache
	//
	static const uint8_t CMD_REMOVE_OK = 43;

	//
	// Response from index-server after successful
	// registration of a new node. Data on stream is:
	// id:uint32_t -- the id assigned to the node
	static const uint8_t CMD_HELLO = 44;

	//
	// Tells the index that the node finished
	// reorganization of a single item.
	// Data on stream is:
	// ReorgResult
	//
	static const uint8_t RESP_REORG_ITEM_MOVED = 51;

	//
	// Response from worker to signal that the reorganization
	// is finished.
	//
	static const uint8_t RESP_REORG_DONE = 52;

	//
	// Response from worker including stats update
	//
	static const uint8_t RESP_STATS = 53;

	static const uint8_t RESP_REORG_REMOVE_REQUEST = 54;

	void send_reorg( const ReorgDescription &desc );
	void confirm_move();
	void confirm_remove();
	void send_get_stats();

	void release();

	const ReorgMoveResult& get_move_result() const;
	const TypedNodeCacheKey& get_remove_request() const;
	const NodeStats& get_stats() const;


	ControlConnection(std::unique_ptr<BinaryFDStream> socket, std::shared_ptr<Node> node);
	virtual ~ControlConnection();
	std::shared_ptr<Node> node;
protected:
	virtual void process_command( uint8_t cmd, BinaryStream &payload );
	virtual void write_finished();
private:
	void reset();
	std::unique_ptr<ReorgMoveResult> move_result;
	std::unique_ptr<TypedNodeCacheKey> remove_request;
	std::unique_ptr<NodeStats> stats;
};

////////////////////////////////////////////////////
//
// Worker
//
////////////////////////////////////////////////////

enum class DeliveryState {
	IDLE,
	DELIVERY_REQUEST_READ,
	CACHE_REQUEST_READ,
	MOVE_REQUEST_READ,
	AWAITING_MOVE_CONFIRM, MOVE_DONE,
	SENDING, SENDING_MOVE, SENDING_CACHE_ENTRY, SENDING_ERROR
};

class DeliveryConnection: public BaseConnection<DeliveryState> {
public:
	static const uint32_t MAGIC_NUMBER = 0x52345678;

	//
	// Command to pick up a delivery.
	// Expected data on stream is:
	// delivery_id:uint64_t
	//
	static const uint8_t CMD_GET = 60;

	//
	// Command to pick up a delivery.
	// Expected data on stream is:
	// TypedNodeCacheKey
	//
	static const uint8_t CMD_GET_CACHED_ITEM = 61;

	//
	// Command to pick up a delivery.
	// Expected data on stream is:
	// TypedNodeCacheKey
	//
	static const uint8_t CMD_MOVE_ITEM = 62;

	//
	// Command to pick up a delivery.
	// No expected data on stream.
	//
	static const uint8_t CMD_MOVE_DONE = 63;


	//
	// Response if delivery is send. Data:
	// GenericRaster
	//
	static const uint8_t RESP_OK = 79;

	//
	// Response if delivery faild. Data:
	// message:string -- a description of the error
	//
	static const uint8_t RESP_ERROR = 80;

	DeliveryConnection(std::unique_ptr<BinaryFDStream> socket);
	virtual ~DeliveryConnection();

	const TypedNodeCacheKey& get_key() const;

	uint64_t get_delivery_id() const;

	template <typename T>
	void send( std::shared_ptr<const T> item );

	template <typename T>
	void send_cache_entry( const MoveInfo &info, std::shared_ptr<const T> item );

	template <typename T>
	void send_move( const CacheEntry &info, std::shared_ptr<const T> item );

	void send_error( const std::string &msg );

	void release();

protected:
	virtual void process_command( uint8_t cmd, BinaryStream &payload );
	virtual void write_finished();
private:
	template<typename T>
	void write_data( BinaryStream &stream, std::shared_ptr<const T> &item );

	uint64_t delivery_id;
	TypedNodeCacheKey cache_key;
};

#endif /* CONNECTION_H_ */
