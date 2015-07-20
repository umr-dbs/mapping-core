/*
 * cacheserver.cpp
 *
 *  Created on: 11.05.2015
 *      Author: mika
 */

// project-stuff
#include "cache/node/nodeserver.h"
#include "cache/node/delivery.h"
#include "cache/index/indexserver.h"
#include "cache/priv/connection.h"
#include "cache/priv/transfer.h"
#include "cache/cache.h"
#include "util/exceptions.h"
#include "util/make_unique.h"
#include "util/log.h"
#include <sstream>

#include <sys/select.h>
#include <sys/socket.h>

////////////////////////////////////////////////////////////
//
// NODE SERVER
//
////////////////////////////////////////////////////////////

NodeServer::NodeServer(std::string my_host, uint32_t my_port, std::string index_host, uint32_t index_port,
	int num_threads) :
	shutdown(false), workers_up(false), my_id(-1), my_host(my_host), my_port(my_port), index_host(index_host), index_port(
		index_port), num_treads(num_threads), delivery_manager(my_port) {
}

void NodeServer::worker_loop() {
	while (workers_up && !shutdown) {
		try {
			UnixSocket sock(index_host.c_str(), index_port);
			Log::debug("Worker connected to index-server");
			BinaryStream &stream = sock;
			uint32_t magic = WorkerConnection::MAGIC_NUMBER;
			stream.write(magic);
			stream.write(my_id);

			CacheManager::remote_connection = &sock;

			while (workers_up && !shutdown) {
				try {
					uint8_t cmd;
					if (CacheCommon::read(&cmd, sock, 2, true)) {
						process_worker_command(cmd, stream);
					}
					else {
						Log::info("Disconnect on worker.");
						break;
					}
				} catch (TimeoutException &te) {
					//Log::trace("Read on worker-connection timed out. Trying again");
				} catch (InterruptedException &ie) {
					Log::info("Read on worker-connection interrupted. Trying again.");
				} catch (NetworkException &ne) {
					// Re-throw network-error to outer catch.
					throw;
				} catch (std::exception &e) {
					std::ostringstream os;
					os << "Unexpected error while processing request: " << e.what();
					std::string msg = os.str();
					stream.write(WorkerConnection::RESP_ERROR);
					stream.write(msg);
				}
			}
		} catch (NetworkException &ne) {
			Log::info("Worker lost connection to index... Reconnecting. Reason: %s", ne.what());
		}
		std::this_thread::sleep_for(std::chrono::seconds(2));
	}
	Log::info("Worker done.");
}

void NodeServer::process_worker_command(uint8_t cmd, BinaryStream& stream) {
	Log::debug("Received command: %d", cmd);
	switch (cmd) {
		case WorkerConnection::CMD_CREATE_RASTER:
		case WorkerConnection::CMD_PUZZLE_RASTER:
		case WorkerConnection::CMD_DELIVER_RASTER:
			process_raster_request(cmd, stream);
			break;
		default: {
			Log::error("Unknown command from index-server: %d. Dropping connection.", cmd);
			throw NetworkException("Unknown command from index-server");
		}
	}
	Log::debug("Finished processing command: %d", cmd);
}

void NodeServer::process_raster_request(uint8_t cmd, BinaryStream& stream) {
	std::unique_ptr<GenericRaster> result;
	switch (cmd) {
		case WorkerConnection::CMD_CREATE_RASTER: {
			BaseRequest rr(stream);
			QueryProfiler profiler;
			Log::debug("Processing request: %s", rr.to_string().c_str());
			result = GenericOperator::fromJSON(rr.semantic_id)->getCachedRaster(rr.query, profiler,
				GenericOperator::RasterQM::LOOSE);
			break;
		}
		case WorkerConnection::CMD_PUZZLE_RASTER: {
			PuzzleRequest rr(stream);
			Log::debug("Processing request: %s", rr.to_string().c_str());
			result = CacheCommon::process_raster_puzzle(rr, my_host, my_port);
			Log::debug("Adding puzzled raster to cache.");
			CacheManager::getInstance().put_raster(rr.semantic_id, result);
			break;
		}

		case WorkerConnection::CMD_DELIVER_RASTER: {
			DeliveryRequest rr(stream);
			Log::debug("Processing request: %s", rr.to_string().c_str());
			result = CacheManager::getInstance().get_raster_local(rr.semantic_id, rr.entry_id);
			break;
		}
	}
	Log::debug("Processing request finished. Asking for delivery-qty");
	stream.write(WorkerConnection::RESP_RESULT_READY);
	uint8_t cmd_qty;
	uint32_t qty;
	stream.read(&cmd_qty);
	stream.read(&qty);
	if (cmd_qty != WorkerConnection::RESP_DELIVERY_QTY)
		throw ArgumentException(
			concat("Expected command ", WorkerConnection::RESP_DELIVERY_QTY, " but received ", cmd_qty));

	uint64_t delivery_id = delivery_manager.add_raster_delivery(result, qty);
	Log::debug("Sending delivery_id.");
	stream.write(WorkerConnection::RESP_DELIVERY_READY);
	stream.write(delivery_id);
}

void NodeServer::run() {
	Log::info("Starting Node-Server");

	delivery_thread = delivery_manager.run_async();

	while (!shutdown) {
		try {
			setup_control_connection();

			workers_up = true;
			for (int i = 0; i < num_treads; i++)
				workers.push_back(make_unique<std::thread>(&NodeServer::worker_loop, this));

			// Read on control
			while (!shutdown) {
				try {
					uint8_t cmd;
					if (CacheCommon::read(&cmd, *control_connection, 2, true)) {
						process_control_command(cmd, *control_connection);
					}
					else {
						Log::info("Disconnect on control-connection. Reconnecting.");
						break;
					}
				} catch (TimeoutException &te) {
					//Log::trace("Timeout on read from control-connection.");

				} catch (InterruptedException &ie) {
					Log::info("Interrupt on read from control-connection.");

				} catch (NetworkException &ne) {
					Log::error("Error reading on control-connection. Reconnecting");
					break;
				}
			}
			workers_up = false;
			Log::debug("Waiting for worker-threads to terminate.");
			for (auto &w : workers) {
				w->join();
			}
			workers.clear();
		} catch (NetworkException &ne_c) {
			Log::warn("Could not connect to index-server. Retrying in 5s. Reason: %s", ne_c.what());
			std::this_thread::sleep_for(std::chrono::seconds(5));
		}
	}
	delivery_manager.stop();
	delivery_thread->join();

	Log::info("Node-Server done.");
}

void NodeServer::process_control_command(uint8_t cmd, BinaryStream &stream) {
	switch (cmd) {
		case ControlConnection::RESP_REORG: {
			Log::debug("Received reorg command.");
			ReorgDescription d(stream);
			Log::debug("Read reorg description from stream.");
			for (auto &ri : d.get_items()) {
				switch (ri.type) {
					case ReorgItem::Type::RASTER:
						handle_raster_reorg_item(ri, stream);
						break;
					default:
						throw ArgumentException(concat("Type ", (int) ri.type, " not supported yet"));
				}
			}
			stream.write(ControlConnection::CMD_REORG_DONE);
		}
	}
}

void NodeServer::handle_raster_reorg_item(const ReorgItem& item, BinaryStream &index_stream) {
	std::unique_ptr<BinaryStream> del_stream;
	STCacheKey my_id("",0);

	// Send move request
	try {
		uint8_t nresp;
		del_stream.reset(new UnixSocket(item.from_host.c_str(), item.from_port));
		STCacheKey key(item.semantic_id, item.cache_id);
		del_stream->write(DeliveryConnection::MAGIC_NUMBER);
		del_stream->write(DeliveryConnection::CMD_MOVE_RASTER);
		key.toStream(*del_stream);

		del_stream->read(&nresp);
		switch (nresp) {
			case DeliveryConnection::RESP_OK: {
				auto res = GenericRaster::fromStream(*del_stream);
				// Store item
				my_id = CacheManager::getInstance().put_raster_local(item.semantic_id, res);
				break;
			}
			case DeliveryConnection::RESP_ERROR: {
				std::string msg;
				del_stream->read(&msg);
				throw NetworkException(
					concat("Could not move raster", item.semantic_id, ":", item.cache_id, " from ",
						item.from_host, ":", item.from_port, ": ", msg));
			}
			default:
				throw NetworkException(concat("Received illegal response from delivery-node: ", nresp));
		}
	} catch (NetworkException &ne) {
		Log::error("Could not initiate raster move: %s", ne.what());
		return;
	}

	// Notify index
	uint8_t iresp;
	ReorgResult rr(ReorgResult::Type::RASTER, my_id.semantic_id, my_id.entry_id, item.idx_cache_id);
	index_stream.write(ControlConnection::CMD_REORG_ITEM_MOVED);
	rr.toStream(index_stream);
	index_stream.read(&iresp);

	try {
		switch (iresp) {
			case ControlConnection::RESP_REORG_ITEM_OK: {
				Log::debug("Reorg of item finished. Notifying delivery instance.");
				del_stream->write(DeliveryConnection::CMD_MOVE_DONE);
				break;
			}
			default: {
				Log::warn("Index could not handle reorg of: %s:%d", item.semantic_id.c_str(), item.cache_id);
				break;
			}
		}
	} catch (NetworkException &ne) {
		Log::error("Could not confirm reorg of raster-item to delivery instance.");
	}
}

void NodeServer::setup_control_connection() {
	Log::info("Connecting to index-server: %s:%d", index_host.c_str(), index_port);

	// Establish connection
	this->control_connection.reset(new UnixSocket(index_host.c_str(), index_port));
	BinaryStream &stream = *this->control_connection;

	Log::debug("Sending hello to index-server");
	// Say hello
	uint32_t magic = ControlConnection::MAGIC_NUMBER;
	stream.write(magic);
	stream.write(my_host);
	stream.write(my_port);

	Log::debug("Waiting for response from index-server");
	// Read node-id
	uint8_t resp;
	stream.read(&resp);
	if (resp == ControlConnection::RESP_HELLO) {
		stream.read(&my_id);
		Log::info("Successfuly connected to index-server. My Id is: %d", my_id);
	}
	else {
		std::ostringstream msg;
		msg << "Index returned unknown response-code to: " << resp << ".";
		throw NetworkException(msg.str());
	}
}

std::unique_ptr<std::thread> NodeServer::run_async() {
	return make_unique<std::thread>(&NodeServer::run, this);
}

void NodeServer::stop() {
	Log::info("Node-server shutting down.");
	shutdown = true;
}

NodeServer::~NodeServer() {
	stop();
}
