#pragma once

#include <faabric/util/gids.h>
#include <memory>

#include_next "faabric.pb.h"

namespace faabric {

// Wrapper for BatchExecuteRequest::messages[i] to avoid copying message storage
struct MessageInBatch final
{
    std::shared_ptr<BatchExecuteRequest> batch;
    Message& msg;

    MessageInBatch() = delete;
    MessageInBatch(std::shared_ptr<BatchExecuteRequest> batch, int idx)
      : batch(batch)
      , msg(batch->mutable_messages()->at(idx))
    {
    }
    explicit MessageInBatch(Message& rmsg)
      : batch(initBatch(rmsg))
      , msg(batch->mutable_messages()->at(0))
    {
    }

    Message* operator->() { return &msg; }
    const Message* operator->() const { return &msg; }
    Message& operator*() { return msg; }
    const Message& operator*() const { return msg; }
    operator Message&() { return msg; }
    operator const Message&() const { return msg; }

  private:
    static std::shared_ptr<BatchExecuteRequest> initBatch(Message& rmsg)
    {
        auto batch = std::make_shared<faabric::BatchExecuteRequest>();
        batch->set_id(faabric::util::generateGid());
        *batch->add_messages() = std::move(rmsg);
        return batch;
    }
};

// Summary of call information
struct MessageRecord final
{
    faabric_gid_t appId = 0;
    faabric_gid_t callId = 0;
    std::string user;
    std::string function;
    std::string pythonUser;
    std::string pythonFunction;
    std::string cmdline;
    bool forbidNdp = false;

    MessageRecord() = default;
    MessageRecord(const faabric::Message& msg)
      : appId(msg.appid())
      , callId(msg.id())
      , user(msg.user())
      , function(msg.function())
      , pythonUser(msg.pythonuser())
      , pythonFunction(msg.pythonfunction())
      , cmdline(msg.cmdline())
      , forbidNdp(msg.forbidndp())
    {
    }
    ~MessageRecord() = default;
};

}
