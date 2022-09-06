#ifndef MUDUO_NET_TCPSERVER_H
#define MUDUO_NET_TCPSERVER_H
#include "muduo/base/Atomic.h"
#include "muduo/base/Types.h"
#include "lkpServer.h"
#include "IPCAcceptor.h"
#include "muduo/net/EventLoopThreadPool.h"
#include "IPCConnection.h"
#include <map>

using namespace muduo;
using namespace muduo::net;

class IPCConnection;
class IPCAcceptor;
class muduo::net::EventLoop;
class muduo::net::EventLoopThreadPool;

///
/// TCP server, supports single-threaded and thread-pool models.
///
/// This is an interface class, so don't expose too much details.
class IPCServer : noncopyable
{
public:
    typedef std::function<void(EventLoop *)> ThreadInitCallback;
    enum Option
    {
        kNoReusePort,
        kReusePort,
    };

    //IPCServer(EventLoop* loop, const InetAddress& listenAddr);
    IPCServer(EventLoop *loop, const char *sfd);
    ~IPCServer(); // force out-line dtor, for std::unique_ptr members.

    const string &ipPort() const { return ipPort_; }
    const string &name() const { return name_; }
    EventLoop *getLoop() const { return loop_; }

    /// Set the number of threads for handling input.
    ///
    /// Always accepts new connection in loop's thread.
    /// Must be called before @c start
    /// @param numThreads
    /// - 0 means all I/O in loop's thread, no thread will created.
    ///   this is the default value.
    /// - 1 means all I/O in another thread.
    /// - N means a thread pool with N threads, new connections
    ///   are assigned on a round-robin basis.
    void setThreadNum(int numThreads);
    void setThreadInitCallback(const ThreadInitCallback &cb)
    {
        threadInitCallback_ = cb;
    }
    /// valid after calling start()
    std::shared_ptr<EventLoopThreadPool> threadPool()
    {
        return threadPool_;
    }

    /// Starts the server if it's not listening.
    ///
    /// It's harmless to call it multiple times.
    /// Thread safe.
    void start();

    /// Set connection callback.
    /// Not thread safe.
    void setConnectionCallback(const ConnectionCallback &cb)
    {
        connectionCallback_ = cb;
    }

    /// Set message callback.
    /// Not thread safe.
    void setMessageCallback(const MessageCallback &cb)
    {
        messageCallback_ = cb;
    }

    /// Set write complete callback.
    /// Not thread safe.
    void setWriteCompleteCallback(const WriteCompleteCallback &cb)
    {
        writeCompleteCallback_ = cb;
    }

private:
    /// Not thread safe, but in loop
    void newConnection(int sockfd);
    /// Thread safe.
    void removeConnection(const IPCConnectionPtr &conn);
    /// Not thread safe, but in loop
    void removeConnectionInLoop(const IPCConnectionPtr &conn);

    typedef std::map<string, IPCConnectionPtr> ConnectionMap;

    EventLoop *loop_; // the acceptor loop
    const string ipPort_;
    const string name_;
    std::unique_ptr<IPCAcceptor> acceptor_; // avoid revealing Acceptor
    std::shared_ptr<EventLoopThreadPool> threadPool_;
    ConnectionCallback connectionCallback_;
    MessageCallback messageCallback_;
    WriteCompleteCallback writeCompleteCallback_;
    ThreadInitCallback threadInitCallback_;
    AtomicInt32 started_;
    // always in loop thread
    int nextConnId_;
    ConnectionMap connections_;

    typedef std::shared_ptr<IPCConnection> IPCConnectionPtr;
};

#endif // MUDUO_NET_TCPSERVER_H
