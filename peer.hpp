#ifndef PEER_H
#define PEER_H

#include <string>
#include <chrono>

/**
 * A Peer Object, used to be stored in a Routing or Neighbor Table.
 */
class Peer
{
public:
    Peer();
    Peer(size_t id, std::string ip) :
                m_id(id), m_ip(ip)
                {}
    ~Peer() {}

    size_t getID()
    {
        return m_id;
    }

    std::string getIP()
    {
        return m_ip;
    }

    auto getRTT()
    {
        return m_rtt;
    }

    auto getUptime()
    {
        return m_uptime;
    }

private:
    //The ID of the peer. Typically the hash of the IP Addr
    size_t m_id;
    //IP Addr
    std::string m_ip;
    //round-trip-time of the peer.
    std::chrono::milliseconds m_rtt;
    //uptime of the peer
    //TODO eva if timestamp of creation would be enough..
    std::chrono::milliseconds m_uptime;

};

#endif /* PEER_H */
