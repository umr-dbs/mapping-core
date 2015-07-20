/*
 * indexserver.h
 *
 *  Created on: 26.05.2015
 *      Author: mika
 */

#ifndef INDEX_INDEXSERVER_H_
#define INDEX_INDEXSERVER_H_

#include "cache/common.h"
#include "cache/index/querymanager.h"
#include "cache/priv/connection.h"
#include "cache/priv/transfer.h"
#include "cache/priv/redistribution.h"
#include "cache/cache.h"
#include "util/log.h"

#include <string>
#include <thread>
#include <map>
#include <vector>
#include <deque>


//
// Represents a cache-node.
//

class Node {
public:
	Node(uint32_t id, const std::string &host, uint32_t port);
	// The unique id of this node
	const uint32_t id;
	// The hostname of this node
	const std::string host;
	// The port for delivery connections on this node
	const uint32_t port;
};

//
// The heart of the cache. The index
// accepts connections from the client as well
// as from cache-nodes.
// Cache-nodes have to establish a so called control-connection
// on which they send their hostname and delivery port. After
// doing so, a unique id is assigned to the node. All workers of
// this node must use this id to register themselves at the index.
//
// Client-connections may issue requests to the server.
// The server handles everything in a single thread.
//


class IndexServer {
public:
	IndexServer( int port );
	virtual ~IndexServer();
	// Fires up the index-server and will return after
	// stop() is invoked by another thread
	void run();
	// Triggers the shutdown of the index-server
	// Subsequent calls to run or run_async have undefined
	// behaviour
	virtual void stop();
protected:
	// The currently known nodes
	std::map<uint32_t,std::shared_ptr<Node>> nodes;
	// Connections
	std::map<uint64_t,std::unique_ptr<ControlConnection>> control_connections;
	std::map<uint64_t,std::unique_ptr<WorkerConnection>>  worker_connections;
	std::map<uint64_t,std::unique_ptr<ClientConnection>>  client_connections;
private:
	// Adds the fds of all connections to the read-set
	// and kills faulty connections
	int setup_fdset( fd_set *readfds);

	// Processes the handshake on newly established connections
	void process_handshake( std::vector<int> &new_fds, fd_set *readfds);

	void process_control_connections(fd_set *readfds);
	void process_worker_connections(fd_set *readfds);
	void process_client_connections(fd_set *readfds);

	void process_client_request( ClientConnection &con );
	void process_worker_raster_query( WorkerConnection &con );

	// Reorg
	void handle_reorg_result( uint32_t new_node, const ReorgResult &res );

	// The port the index-server is listening on
	int port;

	// Indicator telling if the server should shutdown
	bool shutdown;

	// The next id to assign to a node
	uint32_t next_node_id;
	// Cache
	RasterRefCache raster_cache;

	QueryManager query_manager;
};

#endif /* INDEX_INDEXSERVER_H_ */
