#ifndef POLLER_H
#define POLLER_H

#include <vector>
#include <unordered_map>
#include <TimeStamp.h>

class Channel;
class EventLoop;

// muduo���ж�·�¼��ַ����ĺ���IO����ģ��
// ������Ϊ�������Ƿ������н��ж���ͻ�����Ϣ�������
// �����ǳ�����
// ��epoll��װ��������Ҫ�̳��Ը���


/// @brief Poller�� �Ƕ�epoll�������������IO��·���õĳ�����
///        ���п��Լ�¼ע�ᵽepoll�ϵ�chanenl�Ͷ�Ӧfd ����channel��epoll�ϵĸ������ն������������͸������������
class Poller
{
public:
    // �洢Channel*
    using ChannelVector = std::vector<Channel*>;
    // ��¼ÿ��channel�Ͷ�Ӧ��fd <socket_fd, channel*>
    using ChannelMap = std::unordered_map<int, Channel*>;


    /// @brief Poller���캯��
    /// @param loop EventLoopָ�� ����Ȩ�����ڸ���
    Poller(EventLoop* loop)
        : m_loop(loop) { }


    /// @brief �������� ��Ϊû��ʲô��Ҫ�����ָ�� ���Լ�ʹ��������˶�̬������ Ҳ����ʹ��Ĭ������
    virtual ~Poller() = default;

    // �����麯�� 

    /// @brief ��epoll_wait�д������¼�д�뵽activeChannel����
    /// @param activeChannel ���ڱ��津����Channel
    /// @return 
    virtual TimeStamp wait(ChannelVector* activeChannel) = 0;
    virtual void updateChannel(Channel* channel) = 0;
    virtual void removeChannel(Channel* channel) = 0;
    virtual size_t getMapSize() = 0;


    /// @brief �жϴ���Ĳ������Ƿ������ôһ��ͨ��
    /// @param channel 
    /// @return bool
    bool hasChannel(Channel* channel) const;

    /// @brief ��̬���� ��ȡPollerʵ��(ָ��)
    /// @param loop �����EventLoop*ָ��
    /// @return Poller*
    static Poller* newPoller(EventLoop* loop);

protected:
    // ���಻�ɷ��ʸ����˽�г�Ա���� ����������Ҫ����Ϊ protected
    ChannelMap m_channelMap; // ���ڼ�¼ÿ���ͻ��˵�fd�Ͷ�Ӧ��channel*

private:
    EventLoop* m_loop;

};













#endif // POLLER_H