#include "EventLoop.h"
#include "Channel.h"
#include "myutils.h"
#include "Poller.h"
#include "Logger.h"
#include "EpollPoller.h"

#include <vector>
#include <thread>
#include <signal.h>
#include <sys/eventfd.h>
#include <string>
#include "myutils.h"

__thread EventLoop* thisThread = nullptr;


EventLoop::EventLoop(std::thread::id thread_id)
    : m_looping(false), m_quit(false),
      m_threadId(thread_id),
      m_pollerPtr(Poller::newPoller(this)), // ����ʹ���Լ�������ָ�����̫�鷳��
      m_wakeupFd(createEventFd()),
    //   m_wakeupChannel(new Channel(this, m_wakeupFd)),
      m_wakeupChannel(make_unique_from_pool<Channel>(this, m_wakeupFd))
{
    // ȷ��һ���߳�ֻ��һ��EventLoop 
    LOG_INFO << "EventLoop::EventLoop EventLoop threadId: " << std::to_string(std::hash<std::thread::id>{}(this->m_threadId));
    LOG_INFO << "EventLoop::EventLoop m_pollerPtr ptr: " << ptrToString(this->m_pollerPtr.get());

    // ����eventfd�Ļص���ע�� ��ע��ص� Ȼ������epoll��ע��
    this->m_wakeupChannel->setReadCallback(
        std::bind(&EventLoop::handleEventFd, this)
    );
    LOG_INFO << "loop eventfd register read event";
    this->m_wakeupChannel->enableRead(); // ע�ᵽepoll�� ��Ϊֻ������֪ͨ ����ֻ��Ҫ��עread����
}

EventLoop::~EventLoop()
{
    /**
     * ��Ϊeventfd���ڱ����д��� ��ô����Ҫ��Ӧ�Ĵ�ʩ������
     */
    this->m_wakeupChannel->disableAll();
    this->m_wakeupChannel->remove();
    // �ر� eventfd
    if (this->m_wakeupFd >= 0) { // ���õ�ϰ�ߣ�ȷ��fd��Ч
        ::close(this->m_wakeupFd);
        this->m_wakeupFd = -1; // ����˫�عر�
    }
    thisThread = nullptr;
}

void EventLoop::loop()
{
    this->m_looping.store(true);
    this->m_quit.store(false);
    size_t tid = std::hash<std::thread::id>{}(this->m_threadId);
    LOG_INFO << "thread " << tid << " start loop..." ;
    
    /**
     * EventLoop�����¼�ѭ�� �����¼�����֮����ͨ��Channel��ִ�и��Ե������
     */
    while(!this->m_quit)
    {
        // ���ڴ洢ʵ�ʴ����˵��¼�channel
        this->m_activeChannel.clear();
        this->m_recvTime = this->m_pollerPtr->wait(&this->m_activeChannel);
        // ����ִ�лص�
        for(Channel* channel : this->m_activeChannel)
        {
            channel->handlEvent(this->m_recvTime);
        }

        // ִ����ӵĺ��� ���û������vector����Ϊ�� ����һ��
        this->doPendingFuntor();

    }
    LOG_INFO << "thread " << tid << " stop loop..." ;
    this->m_looping.store(false);
}

void EventLoop::quit()
{
    // quitʵ����ֻ��Ҫͨ��eventfd���epoll_wait���� Ȼ��quit��Ϊtrue ��Ȼ�ͻ��˳�loopѭ����
    this->m_quit.store(true);

    if(! this->isThisThread())
        this->wakeup();
}

void EventLoop::wakeup()
{
    uint64_t buf = 1;
    size_t n = write(this->m_wakeupFd, &buf, sizeof(buf));
    ErrorIf(n != sizeof(buf), 
                    std::string("EventLoop::handleEventFd read " + std::to_string(n) + "bytes instead of 8 bytes").c_str());
}

TimeStamp EventLoop::revcTime()
{
    return this->m_recvTime;
}

void EventLoop::runInLoop(Functor cb)
{
    // ����Ǳ��̵߳� ��ô����ֱ��ִ�лص�
    if(this->isThisThread())
        cb();

    // ������ǵ�ǰ�̵߳�loop ����Ҫ���뵽loop���еĺ���������
    else
        this->queueInLoop(cb);
    
}

void EventLoop::queueInLoop(Functor cb)
{
    /**
     * EventLoop�м�����ԭ��
     * ��Ϊ���߳�mainloop���ܵ���runInLoop������subloop�Ϸ������ӣ��������queueInLoop�������������������ӻص�����
     * ����������subloop����ִ��doPendingFuntor�ͻ�ͨ��swap�����������գ�����ì��
     * һ���������߳�����������ӻص�����һ�������߳�Ҳ���ܶ������������Ӷ����Զ���̵߳������������Ҫ����
     */
    {
        std::lock_guard<std::mutex> lock(this->m_mtx);
        this->m_FunctorVector.push_back(cb);
    }

    // ������Ǳ��̵߳ĺ������� ���� ���̵߳�loop����ִ��֮ǰ��ӵ��������еĺ���
    // ֻ��! this->isThisThread()Ҳ����, ֻ�������￼�ǵ��˱��߳��е�loop����ִ����ӵĺ��� ��ʱ���ڱ��̵߳��� ����ӵĻ��Ͳ�����л�����
    if(! this->isThisThread() || this->m_isDoingPendingFunctors)
        wakeup();
}

/**
 * ������������������Channel���н��е��� Ȼ�����EventLoop���еķ��� ���յ���EpollPoller���еķ���
 * ��ΪEpollPoller(Poller�ǳ�����)�Ƕ�epfd�ķ�װ, ��¼�����е�channel
 */

void EventLoop::updateChannel(Channel *channel)
{
    this->m_pollerPtr->updateChannel(channel);
}

void EventLoop::removeChannel(Channel *channel)
{
    this->m_pollerPtr->removeChannel(channel);
}

void EventLoop::hasChannle(Channel *channel)
{
    this->m_pollerPtr->hasChannel(channel);
}

bool EventLoop::isThisThread()
{
    return this->m_threadId == std::this_thread::get_id();
}

size_t EventLoop::getMapSize()
{
    return this->m_pollerPtr->getMapSize();
}

int EventLoop::createEventFd()
{
    int eventfd = ::eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
    ErrorIf(eventfd < 0, "eventfd create error");
    return eventfd;
}

void EventLoop::handleEventFd()
{
    // ʹ�õ��� eventfd ���ƣ�read() ʱ��Ҫ��ȡ 8 �ֽڣ�uint64_t����д��ʱҲ������ 8 �ֽ�
    // ֻ�����ڶ��������źŶ��ѣ����ݲ�����Ҫ����Ҫ��������Ķ�ȡʵ������������� eventfd �ں˻�������ʹ���´��ܼ��������¼�
    uint64_t buf = 1; // 8�ֽ�
    size_t n = read(this->m_wakeupFd, &buf, sizeof(buf));
    ErrorIf(n != sizeof(buf), 
                    std::string("EventLoop::handleEventFd read " + std::to_string(n) + "bytes instead of 8 bytes").c_str());
}

void EventLoop::doPendingFuntor()
{
    std::vector<Functor> funtionVec;
    this->m_isDoingPendingFunctors.store(true);

    {
        // ��m_FunctorVector�е����񽻻�����
        std::lock_guard<std::mutex> lock(this->m_mtx);
        funtionVec.swap(this->m_FunctorVector);
    }

    // ����ִ�лص����� һ����˵�����߳�mainloop��subloop����ӵĺ��� ��Ҫsubloop��ִ��
    for(Functor func : funtionVec)
        func();

    this->m_isDoingPendingFunctors.store(false);
}


