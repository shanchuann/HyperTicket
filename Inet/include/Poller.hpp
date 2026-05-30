//C++ STL
#include <map>
#include <vector>
using namespace std;
// OWNER
#include "Timestamp.hpp"
#include "Channel.hpp"
#include "EventLoop.hpp"

#ifndef POLLER_HPP
#define POLLER_HPP
namespace shanchuan
{
class Channel;

class Poller 
{
public:
    //Poller关注的Channel
    using ChannelList = std::vector<Channel*>;
    // 保存fd -> Channel的映射map集合
    using ChannelMap = std::map<int, Channel*>;
protected:
    ChannelMap channels_; // 需要事件监听的Channel集合
private:
    EventLoop* ownerLoop_; // 当前Poller 关联的 EventLoop
public:
    Poller(EventLoop* loop);
    virtual ~Poller();
    /// Polls the I/O events.
    /// Must be called in the loop thread.
    //需要交给派生类实现的接口
    //用于监听感兴趣的事件fd 和 socketfd(封装成了channel) 
    virtual Timestamp poll(int timeoutMs, ChannelList* activeChannels) = 0;
    /// Changes the interested I/O events.
    /// Must be called in the loop thread.
    // 需要交给派生类实现的接口（须在EventLoop所在的线程调用), 用于更新事件，
    virtual void updateChannel(Channel* channel) = 0;
    /// Remove the channel, when it destructs.
    /// Must be called in the loop thread.
    // 当Channel对象销毁时移除此Channel
    virtual void removeChannel(Channel* channel) = 0;
    
    virtual bool hasChannel(Channel* channel) const;
    //获取Poller的派生类型对象（内部实现可能是EPollPoller或PollPoller对象）
    static Poller* newDefaultPoller(EventLoop* loop);
    //断言是否在创建EventLoop的所在线程
    void assertInLoopThread() const
    {
        ownerLoop_->assertInLoopThread();
    }
};

}
#endif 