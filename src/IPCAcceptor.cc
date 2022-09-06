#include "IPCAcceptor.h"
#include "muduo/base/Logging.h"
#include "muduo/net/EventLoop.h"
#include "muduo/net/InetAddress.h"
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include"lkpServer.h"

//::socket()
int createNonblockingOrDie(const char *sockName)
{

    int sockfd = ::socket(PF_UNIX, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC,0);
    if (sockfd < 0)
    {
        LOG_SYSFATAL << "sockets::createNonblockingOrDie";
    }
    return sockfd;
}

IPCAcceptor::IPCAcceptor(EventLoop *loop)
    : loop_(loop),
      sfd_(createNonblockingOrDie(CMDIPC)),
      acceptChannel_(loop, sfd_), //loop,fd
      listening_(false),
      idleFd_(::open("/dev/null", O_RDONLY | O_CLOEXEC))
{
    assert(idleFd_ >= 0);


    //::bind
    struct sockaddr_un server;
    server.sun_family = AF_UNIX;
    strcpy(server.sun_path, CMDIPC);
    unlink(CMDIPC);
    int ret = bind(sfd_, (struct sockaddr *)&server, sizeof(server));
    if (ret < 0)
    {
        perror("bind fail");
        close(sfd_);
        unlink(CMDIPC);
        return;
    }
    


    acceptChannel_.setReadCallback(
        std::bind(&IPCAcceptor::handleRead, this));
}

IPCAcceptor::~IPCAcceptor()
{
    acceptChannel_.disableAll();
    acceptChannel_.remove();
    ::close(idleFd_);
}

//绑定套接字，调用::listen(fd,maxListenNum)，设置channel为enableReading，可读时调用handleRead
void IPCAcceptor::listen()
{
    loop_->assertInLoopThread();
    listening_ = true;

    //::listen(fd,maxListenNum)
    int ret = ::listen(sfd_, 300);
    if (ret < 0)
    {
        perror("listen sfd_ fail");
        close(sfd_);
        unlink(CMDIPC);
        return;
    }
    CMDsfd = sfd_;
    
    acceptChannel_.enableReading();
}

//lfd有信息，直接调用accept形成cfd，调用顶层设置的newConnectionCallback_，可以是IPCServer的newConnection函数
void IPCAcceptor::handleRead()
{
    loop_->assertInLoopThread();

    //::accept
    struct sockaddr_in client;
    socklen_t len = sizeof(client);
    CMDcfd = accept(CMDsfd, (struct sockaddr *)&client, &len);
    printf("IPC建立， CMDcfd：%d\n", CMDcfd);
    if (CMDcfd >= 0)
    {
        if (newConnectionCallback_)
        {
            newConnectionCallback_(CMDcfd); //调用TcpServer的newConnection函数
        }
        else
        {
            perror("accept error\n");
            ::close(CMDcfd);
        }
    }
    else
    {
        LOG_SYSERR << "in IPCAcceptor::handleRead";
        if (errno == EMFILE)
        {
            ::close(idleFd_);
            idleFd_ = ::accept(CMDsfd, NULL, NULL);
            ::close(idleFd_);
            idleFd_ = ::open("/dev/null", O_RDONLY | O_CLOEXEC);
        }
    }
}
