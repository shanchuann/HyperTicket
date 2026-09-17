#include "test_util.hpp"
#include "SyncQueue.hpp"

using shanchuan::QueueResult;
using shanchuan::SyncQueue;

static void test_non_blocking_capacity()
{
    SyncQueue<int> queue(1);
    CHECK(queue.try_put(1) == QueueResult::Ok);
    CHECK(queue.try_put(2) == QueueResult::Full);
    int value = 0;
    CHECK(queue.take(value) == QueueResult::Ok);
    CHECK_EQ(value, 1);
}

int main()
{
    RUN_TEST(test_non_blocking_capacity);
    return TEST_SUMMARY();
}
