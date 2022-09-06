#include "IPCserver.h"



IPCServer::IPCServer(EventLoop *loop,const char*sfd)
    : loop_(CHECK_NOTNULL(loop)),
      acceptor_(new IPCAcceptor(loop)),
      threadPool_(new EventLoopThreadPool(loop, "IPCServer")), //如果设置了通信线程池，就在通信线程池中找下一个。否则直接用accept线程
      connectionCallback_(defaultConnectionCallback),
      messageCallback_(defaultMessageCallback),
      nextConnId_(1)
{
    acceptor_->setNewConnectionCallback(bind(&IPCServer::newConnection, this, boost::placeholders::_1));
}

IPCServer::~IPCServer()
{
    loop_->assertInLoopThread();
    LOG_TRACE << "IPCServer::~IPCServer [" << name_ << "] destructing";

    for (auto &item : connections_)
    {
        IPCConnectionPtr conn(item.second);
        item.second.reset();
        conn->getLoop()->runInLoop(
            std::bind(&IPCConnection::connectDestroyed, conn));
    }
}

void IPCServer::setThreadNum(int numThreads)
{
    assert(0 <= numThreads);
    threadPool_->setThreadNum(numThreads);
}

void IPCServer::start()
{
    if (started_.getAndSet(1) == 0)
    {
        threadPool_->start(threadInitCallback_);

        assert(!acceptor_->listening());
        loop_->runInLoop(
            std::bind(&IPCAcceptor::listen, get_pointer(acceptor_))); //::bind,::listen,epoll_wait + accept
    }
}

//accept后立刻调用，sockfd是accept得到的cfd，选定IO线程，把cfd加入
void IPCServer::newConnection(int sockfd)
{
    loop_->assertInLoopThread();
    EventLoop *ioLoop = threadPool_->getNextLoop(); //如果设置了线程池，就在通信线程池中找下一个。否则直接用accept线程
    char buf[64];
    snprintf(buf, sizeof buf, "-%s#%d", ipPort_.c_str(), nextConnId_);
    ++nextConnId_;
    string connName = name_ + buf;


    //设置cfd的回调函数
    IPCConnectionPtr conn(new IPCConnection(ioLoop,connName,sockfd));
    connections_[connName] = conn;
    conn->setConnectionCallback(connectionCallback_);
    conn->setMessageCallback(messageCallback_);
    conn->setWriteCompleteCallback(writeCompleteCallback_);
    conn->setCloseCallback(
        bind(&IPCServer::removeConnection, this, boost::placeholders::_1)); // FIXME: unsafe
    ioLoop->runInLoop(bind(&IPCConnection::connectEstablished, conn));
}

void IPCServer::removeConnection(const TcpConnectionPtr &conn)
{
    // FIXME: unsafe
    loop_->runInLoop(std::bind(&IPCServer::removeConnectionInLoop, this, conn));
}

void IPCServer::removeConnectionInLoop(const TcpConnectionPtr &conn)
{
    loop_->assertInLoopThread();
    LOG_INFO << "IPCServer::removeConnectionInLoop [" << name_
             << "] - connection " << conn->name();
    size_t n = connections_.erase(conn->name());
    (void)n;
    assert(n == 1);
    EventLoop *ioLoop = conn->getLoop();
    ioLoop->queueInLoop(
        std::bind(&IPCConnection::connectDestroyed, conn));
}
