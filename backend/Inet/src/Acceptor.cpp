// C++
#include <functional>
using namespace std;

// C API 
#include <assert.h>
#include <string.h>
// Liunx API 
#include <fcntl.h> // open
#include <errno.h>
#include <unistd.h> //close;

// own
#include "Logger.hpp"
#include "SocketsOps.hpp"
#include "Acceptor.hpp"
namespace shanchuan
{

////处理新的连接请求
void Acceptor::handleRead()// 
{
    LOG_TRACE<<"处理新的连接请求";
    loop_->assertInLoopThread();
    InetAddress peerAddr{};
    int connfd = acceptSocket_.accept(&peerAddr);
    if(connfd >= 0)
    {
        LOG_TRACE<<"accept of ...connfd "<<connfd;
        if(newConnectionCallback_)
        {
            newConnectionCallback_(connfd,peerAddr);
        }
        else
        {
            LOG_TRACE<<"Call back emtpy ...";
            Sockets::close(connfd);
        }
    }
    else
    {
        LOG_ERROR<<"accept of fail"<<strerror(errno);
        LOG_SYSERR<<"in Acceptor::handleRead";
        if(errno == EMFILE)
        {
            ::close(idleFd_);
            idleFd_ = ::accept(acceptSocket_.fd(), NULL, NULL);
            ::close(idleFd_);
            idleFd_ = ::open("/dev/null", O_RDONLY | O_CLOEXEC);
        }
    }
    LOG_TRACE<<"END";
}
/*
Acceptor::Acceptor( const InetAddress& listenAddr)
    : acceptSocket_(Sockets::createNonblockingOrDie()),
    listenning_(false)
{
    LOG_TRACE<<"Create Acceptor";
    LOG_TRACE<<"accptor begn bind listenAddr"<<listenAddr.toIpPort();
    acceptSocket_.bindAddress(listenAddr); //
}
*/
Acceptor::Acceptor(EventLoop* loop, const InetAddress& listenAddr, bool reuseport)
    : loop_(loop),
    acceptSocket_(Sockets::createNonblockingOrDie()),
    acceptChannel_(loop, acceptSocket_.fd()),
    listenning_(false),
    idleFd_(::open("/dev/null", O_RDONLY | O_CLOEXEC))
{
    assert(idleFd_ >= 0);
    //acceptSocket_.setReuseAddr(true);
    //acceptSocket_.setReusePort(reuseport);
    acceptSocket_.bindAddress(listenAddr);
    acceptChannel_.setReadCallback(std::bind(&Acceptor::handleRead, this));
}
Acceptor::~Acceptor()
{
    acceptChannel_.disableAll();
    acceptChannel_.remove();
    ::close(idleFd_);
    LOG_TRACE<<"Acceptor::~Acceptor";
}

bool Acceptor::listenning() const 
{
    return listenning_;
}
void Acceptor::listen()
{
    LOG_TRACE<<"Acceptor::listen begin ...";
    loop_->assertInLoopThread();
    listenning_ = true;
    acceptSocket_.listen();
    acceptChannel_.enableReading();
    //handleRead();
}
}