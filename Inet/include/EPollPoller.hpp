//C++ STL
#include <vector>
using namespace std;
// OWNER
#include "Poller.hpp"

#ifndef EPOLLPOLLER_HPP
#define EPOLLPOLLER_HPP


struct epoll_event;

namespace shanchuan
{

///
/// IO Multiplexing with epoll(4).
///
class EPollPoller : public Poller
{
public:
    using EventList = std::vector<struct epoll_event> ;
private:
    // epoll监视的文件描述符
    int epollfd_; 
    // 用来存储活跃文件描述符的epoll_event结构体数组
    EventList events_;  //epoll_wait 收集事件的集合
public:
    EPollPoller(EventLoop* loop);
    virtual ~EPollPoller();
    //// 重写基类Poller的抽象方法
    virtual Timestamp poll(int timeoutMs, ChannelList* activeChannels);
    
    virtual void updateChannel(Channel* channel);
    
    virtual void removeChannel(Channel* channel);

private:
    //// 默认监听事件数量
    static const int kInitEventListSize = 16;
    // 填充活跃的连接
    // EventLoop内部调用此方法，会将发生了事件的Channel填充到activeChannels中
    void fillActiveChannels(int numEvents,ChannelList* activeChannels) const;
    // 更新channel通道，本质是调用了epoll_ctl
    void update(int operation, Channel* channel);
};

}
#endif 