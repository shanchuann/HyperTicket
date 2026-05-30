//Acceptor.hpp
//C++ STL
#include<functional>
using namespace std;
//own
#include"InetAddress.hpp"
#include"Socket.hpp"
#include"Channel.hpp"
#include"EventLoop.hpp"

#ifndef ACCEPTOR_HPP
#define ACCEPTOR_HPP
namespace shanchuan
{
class InetAddress;

class Acceptor
{
public:
    using NewConnectionCallback = std::function<void(int connfd,const InetAddress &)>;
private:
    NewConnectionCallback newConnectionCallback_;
public:
    void setNewConnectionCallback(const NewConnectionCallback &cb)
    {
        newConnectionCallback_ = cb;
    }
private:
    Socket acceptSocket_;
    bool listenning_;
    ///3
    EventLoop *loop_;
    Channel acceptChannel_;
    int idleFd_;

    //处理对端连接请求
    void handleRead();

public:
    //Acceptor(const InetAddress& listenAddr);
    Acceptor(EventLoop *loop,const InetAddress&,bool reuseport);
    ~Acceptor();
    bool listenning() const ;
    void listen();
};

}

#endif  

#if 0 

// Acceptor.hpp
// C++ STL
#include <functional>
using namespace std;
// own
#include "InetAddress.hpp"
#include "ListenSocket.hpp"

#ifndef ACCEPTOR_HPP
#define ACCEPTOR_HPP


namespace shanchuan
{
    class InetAddress;

    class Acceptor
    {
    private:
        Socket acceptSocket_;
        bool listenning_;
        // 处理对端连接请求
        void handleRead();
        int connfd_;
        InetAddress peerAddr_;

    public:
        Acceptor(const InetAddress &listenAddr);
        ~Acceptor();
        bool listenning() const;
        void listen();
        int getConnfd() const { return connfd_; }
        const InetAddress &getPeerAddr() const { return peerAddr_; }
    };

}
#endif

#endif 