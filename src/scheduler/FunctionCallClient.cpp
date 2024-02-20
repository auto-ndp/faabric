#include "faabric.pb.h"
#include <faabric/proto/faabric.pb.h>
#include <faabric/scheduler/FunctionCallClient.h>
#include <faabric/transport/common.h>
#include <faabric/transport/macros.h>
#include <faabric/util/queue.h>
#include <faabric/util/testing.h>
#include <faabric/util/timing.h>

namespace faabric::scheduler {

// -----------------------------------
// Mocking
// -----------------------------------

static std::mutex mockMutex;

static std::vector<std::pair<std::string, faabric::Message>> functionCalls;

static std::vector<std::pair<std::string, faabric::EmptyRequest>> flushCalls;

static std::vector<
  std::pair<std::string, std::shared_ptr<faabric::BatchExecuteRequest>>>
  batchMessages;

static std::vector<std::pair<std::string, faabric::EmptyRequest>>
  resourceRequests;

static std::unordered_map<std::string,
                          faabric::util::Queue<faabric::HostResources>>
  queuedResourceResponses;

static std::vector<
  std::pair<std::string, std::shared_ptr<faabric::PendingMigrations>>>
  pendingMigrationsRequests;

static std::vector<std::pair<std::string, faabric::UnregisterRequest>>
  unregisterRequests;

std::vector<std::pair<std::string, faabric::Message>> getFunctionCalls()
{
    faabric::util::UniqueLock lock(mockMutex);
    return functionCalls;
}

std::vector<std::pair<std::string, faabric::EmptyRequest>> getFlushCalls()
{
    faabric::util::UniqueLock lock(mockMutex);
    return flushCalls;
}

std::vector<
  std::pair<std::string, std::shared_ptr<faabric::BatchExecuteRequest>>>
getBatchRequests()
{
    faabric::util::UniqueLock lock(mockMutex);
    return batchMessages;
}

std::vector<std::pair<std::string, faabric::EmptyRequest>> getResourceRequests()
{
    faabric::util::UniqueLock lock(mockMutex);
    return resourceRequests;
}

std::vector<std::pair<std::string, std::shared_ptr<faabric::PendingMigrations>>>
getPendingMigrationsRequests()
{
    faabric::util::UniqueLock lock(mockMutex);
    return pendingMigrationsRequests;
}

std::vector<std::pair<std::string, faabric::UnregisterRequest>>
getUnregisterRequests()
{
    faabric::util::UniqueLock lock(mockMutex);
    return unregisterRequests;
}

void queueResourceResponse(const std::string& host, faabric::HostResources& res)
{
    faabric::util::UniqueLock lock(mockMutex);
    queuedResourceResponses[host].enqueue(res);
}

void clearMockRequests()
{
    faabric::util::UniqueLock lock(mockMutex);
    functionCalls.clear();
    batchMessages.clear();
    resourceRequests.clear();
    pendingMigrationsRequests.clear();
    unregisterRequests.clear();

    for (auto& p : queuedResourceResponses) {
        p.second.reset();
    }
    queuedResourceResponses.clear();
}

// -----------------------------------
// Message Client
// -----------------------------------
FunctionCallClient::FunctionCallClient(const std::string& hostIn)
  : faabric::transport::MessageEndpointClient(hostIn,
                                              FUNCTION_CALL_ASYNC_PORT,
                                              FUNCTION_CALL_SYNC_PORT)
{
}

void FunctionCallClient::sendFlush()
{
    faabric::EmptyRequest req;
    if (faabric::util::isMockMode()) {
        faabric::util::UniqueLock lock(mockMutex);
        flushCalls.emplace_back(host, req);
    } else {
        faabric::EmptyResponse resp;
        syncSend(faabric::scheduler::FunctionCalls::Flush, &req, &resp);
    }
}

faabric::HostResources FunctionCallClient::getResources()
{
    ZoneScopedNS("FunctionCallClient::getResources", 6);
    faabric::EmptyRequest request;
    faabric::HostResources response;

    if (faabric::util::isMockMode()) {
        faabric::util::UniqueLock lock(mockMutex);

        // Register the request
        resourceRequests.emplace_back(host, request);

        // See if we have a queued response
        if (queuedResourceResponses[host].size() > 0) {
            response = queuedResourceResponses[host].dequeue();
        }
    } else {
        syncSend(
          faabric::scheduler::FunctionCalls::GetResources, &request, &response);
    }

    return response;
}

// This function call is used by the master host of an application to let know
// other hosts running functions of the same application that a migration
// opportunity has been found.
void FunctionCallClient::sendPendingMigrations(
  std::shared_ptr<faabric::PendingMigrations> req)
{
    faabric::PendingMigrations request;
    faabric::EmptyResponse response;

    if (faabric::util::isMockMode()) {
        faabric::util::UniqueLock lock(mockMutex);
        pendingMigrationsRequests.emplace_back(host, req);
    } else {
        syncSend(faabric::scheduler::FunctionCalls::PendingMigrations,
                 req.get(),
                 &response);
    }
}

void FunctionCallClient::executeFunctions(
  const std::shared_ptr<faabric::BatchExecuteRequest> req)
{
    ZoneScopedNS("FunctionCallClient::executeFunctions", 6);
    if (faabric::util::isMockMode()) {
        faabric::util::UniqueLock lock(mockMutex);
        batchMessages.emplace_back(host, req);
    } else {
        asyncSend(faabric::scheduler::FunctionCalls::ExecuteFunctions,
                  req.get());
    }
}

void FunctionCallClient::sendDirectResult(faabric::Message msg)
{
    ZoneScopedNS("FunctionCallClient::sendDirectResult", 6);
    faabric::DirectResultTransmission drt;
    drt.mutable_result()->CopyFrom(msg);
    asyncSend(faabric::scheduler::FunctionCalls::DirectResult, &drt);
}

void FunctionCallClient::unregister(faabric::UnregisterRequest& req)
{
    if (faabric::util::isMockMode()) {
        faabric::util::UniqueLock lock(mockMutex);
        unregisterRequests.emplace_back(host, req);
    } else {
        asyncSend(faabric::scheduler::FunctionCalls::Unregister, &req);
    }
}

faabric::NdpDelta FunctionCallClient::requestNdpDelta(int msgId)
{
    SPDLOG_DEBUG("Requesting NDP delta for message {}", msgId);
    faabric::GetNdpDelta gnd;
    gnd.set_id(msgId);
    faabric::NdpDelta delta;
    syncSend(faabric::scheduler::FunctionCalls::NdpDeltaRequest, &gnd, &delta);
    SPDLOG_DEBUG("Received NDP delta for message {}", msgId);
    return delta;
}
}
