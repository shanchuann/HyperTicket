#include "../backend/Server/include/RedisOrderQueue.hpp"
#include "../backend/Server/include/RedisConnPool.hpp"

#include <atomic>
#include <cassert>
#include <chrono>
#include <iostream>
#include <thread>
#include <vector>

using namespace hyperticket;

int main() {
#ifdef USE_HIREDIS
    std::unique_ptr<RedisConnPool> pool;
    try { pool=std::make_unique<RedisConnPool>("127.0.0.1",6379,8); }
    catch (...) { std::cout << "Redis unavailable, skipping\n"; return 0; }

    const auto suffix=std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
    const std::string stream="test:orders:"+suffix;
    const int64_t ticketId=900000+(std::stoll(suffix)%90000);
    RedisOrderQueue queue(pool.get(),stream,"test-workers","worker-1");
    assert(queue.ensureGroup());
    assert(queue.initializeStock(ticketId,10));

    std::atomic<int> queued{0}, soldOut{0};
    std::vector<std::thread> threads;
    for(int i=0;i<100;++i) threads.emplace_back([&,i]{
        std::string id;
        auto r=queue.enqueue(ticketId,1,1,"req_"+suffix+"_"+std::to_string(i),"{}",id);
        if(r==IOrderQueue::EnqueueResult::Queued) ++queued;
        else if(r==IOrderQueue::EnqueueResult::SoldOut) ++soldOut;
    });
    for(auto &t:threads)t.join();
    assert(queued==10); assert(soldOut==90);

    std::string duplicateId;
    auto duplicate=queue.enqueue(ticketId,1,1,"req_"+suffix+"_0","{}",duplicateId);
    assert(duplicate==IOrderQueue::EnqueueResult::Duplicate);

    int consumed=0; IOrderQueue::Message msg;
    while(queue.consume(msg)) { assert(queue.acknowledge(msg.id)); ++consumed; }
    assert(consumed==10);

    IOrderQueue::Status status;
    assert(queue.getStatus("req_"+suffix+"_0",status));
    assert(status.state=="QUEUED" && status.userId==1);

    redisContext *ctx=nullptr; RedisConnGuard guard(&ctx,pool.get());
    redisReply *reply=(redisReply*)redisCommand(ctx,"DEL %s stock:%lld",stream.c_str(),(long long)ticketId);
    if(reply)freeReplyObject(reply);
    for(int i=0;i<100;++i) {
        const std::string key="order:req:req_"+suffix+"_"+std::to_string(i);
        reply=(redisReply*)redisCommand(ctx,"DEL %s",key.c_str()); if(reply)freeReplyObject(reply);
    }
    std::cout << "Redis Streams atomic stock+queue test PASSED\n";
#else
    std::cout << "hiredis unavailable, skipping\n";
#endif
}
