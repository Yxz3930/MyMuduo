#ifndef EVENT_LOOP_H
#define EVENT_LOOP_H

#include <memory>
#include <atomic>
#include <mutex>
#include <queue>
#include <thread>
#include <functional>
#include "TimeStamp.h"
#include "SmartPointer.h"


class Channel;
class Poller;

/// @brief EventLoop �����¼�ѭ������ ���а���Channel, Poller(EpollPoller�ĳ�����)����ģ��
///        1. Channel�����¼���ѭ�� 2. ��ͬ�߳�֮��eventfd��֪ͨ 3. mainloop����subloop���зַ�
class EventLoop
{
public:
    using Functor = std::function<void()>;

    EventLoop(std::thread::id thread_id = std::this_thread::get_id());
    ~EventLoop();

    /// @brief �¼�ѭ��
    void loop();

    /// @brief �˳�loop
    void quit();

    /// @brief ��eventfd��д��д������ ���ڻ���epoll_wait
    void wakeup();

    /// @brief ÿ��epoll_wait���������ʱ�� Ҳ�������¼�������ʱ��
    /// @return 
    TimeStamp revcTime();

    /// @brief �ڵ�ǰ�̺߳ͷǵ�ǰ�߳����������ִ�к��� ������eventfd���Ѳ���
    void runInLoop(Functor cb);

    /// @brief ���ص��������뵽��������������(vector����) ������eventfd���Ѳ���
    /// @param cb 
    void queueInLoop(Functor cb);

    /// @brief ����channel�¼� Channel::updateEvents -> EventLoop::updateChannel -> Poller::updateChannel
    /// @param channel 
    void updateChannel(Channel* channel);

    /// @brief �Ƴ�channel Channel::remove -> EventLoop::removeChannel -> Poller::removeChannel
    /// @param channel 
    void removeChannel(Channel* channel);

    /// @brief �鿴�Ƿ����channel 
    /// @param channel 
    void hasChannle(Channel* channel);

    /// @brief �жϵ�������������Ƿ��Ǳ�������̻߳���˵�Ƿ��Ǳ������loop
    /// @return
    bool isThisThread();

    /// @brief ��ȡmap�Ĵ�С
    /// @return 
    size_t getMapSize();

private:

    /// @brief ����eventfd
    /// @return
    int createEventFd();

    /// @brief ����eventfd�Ķ��¼�(��wakeup����ƥ��) ��Ϊeventfdֻ�����߳�֮����¼�֪ͨ
    void handleEventFd();

    /// @brief ִ����ӵ�m_FunctorVector�еĻص�����
    void doPendingFuntor();

private:
    // ����epoll_wait���ع����Ĵ洢Channel*��vector���� 
    using ChannelVector = std::vector<Channel *>;

    // ����loopѭ��
    std::atomic_bool m_looping; // ���ڽ���loopѭ��
    std::atomic_bool m_quit;    // �Ƿ��˳�loopѭ��

    std::thread::id m_threadId; // ���ڼ�¼��ǰ�߳�id

    // std::unique_ptr<Poller> m_pollerPtr; // �����Ӧ��Poller��������� ʵ������EpollPoller
    std::unique_ptr<Poller, PoolDeleter<Poller>> m_pollerPtr; // �����Ӧ��Poller��������� ʵ������EpollPoller

    /**
     * EventLoop�ڸ����߳������� ���ǲ�ͬ�߳�֮����Ҫ����֪ͨ �����Ҫeventfd
     * ��Ҫfd�ĵط�����������Ҫһ��Channel���������fd��Ӧ���¼�
     * ��Ȼ������һ��int��һ��unique_ptr<Channel>
     * Ҳ������һ��unique_ptr<Socket>��һ��unique_ptr<Channel>
     */
    int m_wakeupFd;                           // eventfd ����������֮��ͨ��eventfd������һ��subloop Ҳ��Ҫע�ᵽepoll��
    // std::unique_ptr<Channel> m_wakeupChannel; // ����eventfd��Ӧ���¼�����ͨ��
    std::unique_ptr<Channel, PoolDeleter<Channel>> m_wakeupChannel; // ����eventfd��Ӧ���¼�����ͨ��

    ChannelVector m_activeChannel; // �洢�����¼���Channel* ����ִ�лص�

    TimeStamp m_recvTime; // ͨ��Poller�����¼����¼�

    std::atomic_bool m_isDoingPendingFunctors; // ����ִ���ⲿ�̻߳��ߵ�ǰ�߳�ͨ��queueInLoop(cb)��ӽ����ĺ���
    std::vector<Functor> m_FunctorVector;      // ���ڴ洢��ӽ����ĺ��� std::function<void()>
    std::mutex m_mtx;                          // ������Ӻ��������е���Դ
    /**
     * EventLoop�м�����ԭ��
     * ��Ϊ���߳�mainloop���ܵ���runInLoop������subloop�Ϸ������ӣ��������queueInLoop�������������������ӻص�����
     * ����������subloop����ִ��doPendingFuntor�ͻ�ͨ��swap�����������գ�����ì��
     * һ���������߳�����������ӻص�����һ�������߳�Ҳ���ܶ������������Ӷ����Զ���̵߳������������Ҫ����
     */

};

#endif // EVENT_LOOP_H