#ifndef MUDUO_NET_ACCEPTOR_H
#define MUDUO_NET_ACCEPTOR_H
#include <functional>
#include "muduo/net/Channel.h"

using namespace muduo;
using namespace muduo::net;

class muduo::net::EventLoop;

///
/// IPCAcceptor of incoming TCP connections.
///
class IPCAcceptor : noncopyable
{
public:
    typedef std::function<void(int sockfd)> NewConnectionCallback;

    IPCAcceptor(EventLoop *loop);
    ~IPCAcceptor();

    void setNewConnectionCallback(const NewConnectionCallback &cb)
    {
        newConnectionCallback_ = cb;
    }

    //绑定套接字，调用::listen(fd,maxListenNum)，设置channel为enableReading，可读时调用handleRead
    void listen();

    bool listening() const { return listening_; }

    // Deprecated, use the correct spelling one above.
    // Leave the wrong spelling here in case one needs to grep it for error messages.
    // bool listenning() const { return listening(); }

private:
    void handleRead();

    EventLoop *loop_;
    int sfd_;
    Channel acceptChannel_;
    NewConnectionCallback newConnectionCallback_;
    bool listening_;
    int idleFd_;
};

#endif // MUDUO_NET_ACCEPTOR_H
