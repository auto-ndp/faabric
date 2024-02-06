#include "faabric.pb.h"
#include <faabric/scheduler/FunctionCallServer.h>
#include <faabric/snapshot/SnapshotRegistry.h>
#include <faabric/state/State.h>
#include <faabric/transport/common.h>
#include <faabric/transport/macros.h>
#include <faabric/util/concurrent_map.h>
#include <faabric/util/config.h>
#include <faabric/util/func.h>
#include <faabric/util/logging.h>
#include <faabric/util/timing.h>

namespace faabric::scheduler {
FunctionCallServer::FunctionCallServer()
  : faabric::transport::MessageEndpointServer(
      FUNCTION_CALL_ASYNC_PORT,
      FUNCTION_CALL_SYNC_PORT,
      FUNCTION_INPROC_LABEL,
      faabric::util::getSystemConfig().functionServerThreads)
  , scheduler(getScheduler())
{
}

static faabric::util::ConcurrentMap<int, std::function<std::vector<uint8_t>()>>
  ndpDeltaHandlers;

void FunctionCallServer::registerNdpDeltaHandler(
  int id,
  std::function<std::vector<uint8_t>()> handler)
{
    ndpDeltaHandlers.insertOrAssign(id, std::move(handler));
}

void FunctionCallServer::removeNdpDeltaHandler(int id)
{
    ndpDeltaHandlers.erase(id);
}

void FunctionCallServer::doAsyncRecv(transport::Message& message)
{
    uint8_t header = message.getMessageCode();
    switch (header) {
        case faabric::scheduler::FunctionCalls::ExecuteFunctions: {
            recvExecuteFunctions(message.udata());
            break;
        }
        case faabric::scheduler::FunctionCalls::Unregister: {
            recvUnregister(message.udata());
            break;
        }
        case faabric::scheduler::FunctionCalls::DirectResult: {
            recvDirectResult(message.udata());
            break;
        }
        default: {
            throw std::runtime_error(
              fmt::format("Unrecognized async call header: {}", header));
        }
    }
}

std::unique_ptr<google::protobuf::Message> FunctionCallServer::doSyncRecv(
  transport::Message& message)
{
    uint8_t header = message.getMessageCode();
    switch (header) {
        case faabric::scheduler::FunctionCalls::Flush: {
            return recvFlush(message.udata());
        }
        case faabric::scheduler::FunctionCalls::GetResources: {
            return recvGetResources(message.udata());
        }
        case faabric::scheduler::FunctionCalls::PendingMigrations: {
            return recvPendingMigrations(message.udata());
        }
        case faabric::scheduler::FunctionCalls::NdpDeltaRequest: {
            return recvNdpDeltaRequest(message.udata());
        }
        default: {
            throw std::runtime_error(
              fmt::format("Unrecognized sync call header: {}", header));
        }
    }
}

std::unique_ptr<google::protobuf::Message> FunctionCallServer::recvFlush(
  std::span<const uint8_t> buffer)
{
    // Clear out any cached state
    faabric::state::getGlobalState().forceClearAll(false);

    // Clear the scheduler
    scheduler.flushLocally();

    return std::make_unique<faabric::EmptyResponse>();
}

void FunctionCallServer::recvExecuteFunctions(std::span<const uint8_t> buffer)
{
    ZoneScopedNS("FunctionCallServer::recvExecuteFunctions", 6);
    PARSE_MSG(faabric::BatchExecuteRequest, buffer.data(), buffer.size())

    // This host has now been told to execute these functions no matter what
    // TODO - avoid this copy
    parsedMsg.mutable_messages()->at(0).set_topologyhint("FORCE_LOCAL");
    scheduler.callFunctions(
      std::make_shared<faabric::BatchExecuteRequest>(std::move(parsedMsg)));
}

void FunctionCallServer::recvUnregister(std::span<const uint8_t> buffer)
{
    PARSE_MSG(faabric::UnregisterRequest, buffer.data(), buffer.size())

    SPDLOG_DEBUG("Unregistering host {} for {}/{}",
                 parsedMsg.host(),
                 parsedMsg.user(),
                 parsedMsg.function());

    // Remove the host from the warm set
    scheduler.removeRegisteredHost(
      parsedMsg.host(), parsedMsg.user(), parsedMsg.function());
}

std::unique_ptr<google::protobuf::Message> FunctionCallServer::recvGetResources(
  std::span<const uint8_t> buffer)
{
    ZoneScopedNS("FunctionCallServer::recvGetResources", 6);
    auto response = std::make_unique<faabric::HostResources>(
      scheduler.getThisHostResources());
    return response;
}

void FunctionCallServer::recvDirectResult(std::span<const uint8_t> buffer)
{
    ZoneScopedNS("FunctionCallServer::recvDirectResult", 6);
    PARSE_MSG(faabric::DirectResultTransmission, buffer.data(), buffer.size())

    std::unique_ptr<faabric::Message> result{ parsedMsg.release_result() };
    try {
        scheduler.setFunctionResult(std::move(result));
    } catch (const std::exception& e) {
        SPDLOG_ERROR("Failed to set direct result: {}", e.what());
    }
    scheduler.setFunctionResult(std::move(result));
}

std::unique_ptr<google::protobuf::Message>
FunctionCallServer::recvPendingMigrations(std::span<const uint8_t> buffer)
{
    PARSE_MSG(faabric::PendingMigrations, buffer.data(), buffer.size());

    auto msgPtr =
      std::make_shared<faabric::PendingMigrations>(std::move(parsedMsg));

    scheduler.addPendingMigration(msgPtr);

    return std::make_unique<faabric::EmptyResponse>();
    
}

std::unique_ptr<google::protobuf::Message>
FunctionCallServer::recvNdpDeltaRequest(std::span<const uint8_t> buffer)
{
    PARSE_MSG(faabric::GetNdpDelta, buffer.data(), buffer.size());

    auto ndpDelta = ndpDeltaHandlers.get(parsedMsg.id());

    if (!ndpDelta.has_value()) {
        SPDLOG_ERROR("No NDP delta handler found for id {}", parsedMsg.id());
        return std::make_unique<faabric::EmptyResponse>();
    }

    std::vector<uint8_t> ndpDeltaData = ndpDelta.value()();

    auto response = std::make_unique<faabric::NdpDelta>();
    response->mutable_delta()->assign(
      reinterpret_cast<const char*>(ndpDeltaData.data()), ndpDeltaData.size());
    return response;
}
}
