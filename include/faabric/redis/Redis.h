#pragma once

#include <faabric/util/config.h>
#include <faabric/util/exception.h>

#include <absl/container/flat_hash_map.h>
#include <chrono>
#include <hiredis/hiredis.h>
#include <memory>
#include <mutex>
#include <set>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_set>
#include <vector>

namespace faabric::redis {
enum RedisRole
{
    QUEUE,
    STATE,
};

class RedisInstance
{
  public:
    explicit RedisInstance(RedisRole role);

    std::string delifeqSha;
    std::string schedPublishSha;

    std::string ip;
    std::string hostname;
    int port;

  private:
    RedisRole role;

    std::mutex scriptsLock;

    std::string loadScript(redisContext* context,
                           const std::string_view scriptBody);

    // Script to delete a key if it equals a given value
    const std::string_view delifeqCmd = R"---(
if redis.call('GET', KEYS[1]) == ARGV[1] then
    return redis.call('DEL', KEYS[1])
else
    return 0
end
)---";

    // Script to push and expire function execution results avoiding extra
    // copies and round-trips
    const std::string_view schedPublishCmd = R"---(
local key = KEYS[1]
local status_key = KEYS[2]
local result = ARGV[1]
local result_expiry = tonumber(ARGV[2])
local status_expiry = tonumber(ARGV[3])
redis.call('RPUSH', key, result)
redis.call('EXPIRE', key, result_expiry)
redis.call('SET', status_key, result)
redis.call('EXPIRE', status_key, status_expiry)
return 0
)---";
};

using UniqueRedisReply =
  std::unique_ptr<redisReply, decltype(&freeReplyObject)>;

class Redis
{

  public:
    ~Redis();

    /**
     *  ------ Factories ------
     */

    static Redis& getQueue();

    static Redis& getState();

    /**
     *  ------ Standard Redis commands ------
     */
    void ping();

    std::vector<uint8_t> get(const std::string& key);

    size_t strlen(const std::string& key);

    void get(const std::string& key, uint8_t* buffer, size_t size);

    void set(const std::string& key, const std::vector<uint8_t>& value);

    void set(const std::string& key, const uint8_t* value, size_t size);

    void del(const std::string& key);

    long getCounter(const std::string& key);

    long incr(const std::string& key);

    long decr(const std::string& key);

    long incrByLong(const std::string& key, long val);

    long decrByLong(const std::string& key, long val);

    void setRange(const std::string& key,
                  long offset,
                  const uint8_t* value,
                  size_t size);

    void setRangePipeline(const std::string& key,
                          long offset,
                          const uint8_t* value,
                          size_t size);

    void flushPipeline(long pipelineLength);

    void getRange(const std::string& key,
                  uint8_t* buffer,
                  size_t bufferLen,
                  long start,
                  long end);

    void sadd(const std::string& key, const std::string& value);

    void srem(const std::string& key, const std::string& value);

    long scard(const std::string& key);

    bool sismember(const std::string& key, const std::string& value);

    std::string srandmember(const std::string& key);

    std::set<std::string> smembers(
      const std::string& key,
      std::chrono::milliseconds cacheFor = std::chrono::milliseconds(0));

    std::set<std::string> sdiff(const std::string& keyA,
                                const std::string& keyB);

    std::set<std::string> sinter(const std::string& keyA,
                                 const std::string& keyB);

    int lpushLong(const std::string& key, long val);

    int rpushLong(const std::string& key, long val);

    void flushAll();

    long listLength(const std::string& queueName);

    long getTtl(const std::string& key);

    void expire(const std::string& key, long expiry);

    void refresh();

    /**
     *  ------ Locking ------
     */

    uint32_t acquireLock(const std::string& key, int expirySeconds);

    void releaseLock(const std::string& key, uint32_t lockId);

    void delIfEq(const std::string& key, uint32_t value);

    bool setnxex(const std::string& key, long value, int expirySeconds);

    long getLong(const std::string& key);

    void setLong(const std::string& key, long value);

    /**
     * ------ Queueing ------
     */
    void enqueue(const std::string& queueName, const std::string& value);

    void enqueueBytes(const std::string& queueName,
                      const std::vector<uint8_t>& value);

    void enqueueBytes(const std::string& queueName,
                      const uint8_t* buffer,
                      size_t bufferLen);

    std::string dequeue(const std::string& queueName,
                        int timeout = DEFAULT_TIMEOUT);

    std::vector<uint8_t> dequeueBytes(const std::string& queueName,
                                      int timeout = DEFAULT_TIMEOUT);

    size_t dequeueBytes(const std::string& queueName,
                        uint8_t* buffer,
                        size_t bufferLen,
                        int timeout = DEFAULT_TIMEOUT);

    void dequeueMultiple(const std::string& queueName,
                         uint8_t* buff,
                         long buffLen,
                         long nElems);

    // Scheduler result publish
    void publishSchedulerResult(const std::string& key,
                                const std::string& status_key,
                                const std::vector<uint8_t>& result);

  private:
    explicit Redis(const RedisInstance& instance);

    redisContext* context;

    const RedisInstance& instance;

    UniqueRedisReply dequeueBase(const std::string& queueName, int timeout);

    std::mutex smembersCacheMx;
    absl::flat_hash_map<
      std::string,
      std::pair<std::chrono::steady_clock::time_point, std::set<std::string>>>
      smembersCache;
};

class RedisNoResponseException : public faabric::util::FaabricException
{
  public:
    explicit RedisNoResponseException(std::string message)
      : FaabricException(std::move(message))
    {}
};
};
