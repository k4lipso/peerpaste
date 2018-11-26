#ifndef PEER_H
#define PEER_H

#include <string>
#include <chrono>

#include "proto/messages.pb.h"
/**
 * A Peer Object, used to be stored in a Routing or Neighbor Table.
 */
class Peer
{
public:
    Peer();
    Peer(std::string id, std::string ip) :
                m_id(id), m_ip(ip)
                {
                    m_peer = std::make_shared<PeerInfo>();
                    m_peer->set_peer_id(id);
                    m_peer->set_peer_ip(ip);
                }
    ~Peer() {}

    std::string getID()
    {
        return m_peer->peer_id();
    }

    void setID(std::string hashed)
    {
        m_peer->set_peer_id(hashed);
        /* m_id = hashed; */
    }

    std::string getIP()
    {
        return m_peer->peer_ip();
    }

    void setIP(std::string ip)
    {
        m_peer->set_peer_ip(ip);
    }

    auto getRTT()
    {
        return m_peer->peer_rtt();
    }

    auto getUptime()
    {
        return m_peer->peer_uptime();
    }

    /* std::shared_ptr<PeerInfo> getPeer() */
    /* { */
    /*     return m_peer; */
    /* } */

    std::shared_ptr<PeerInfo> getPeer()
    {
        return std::make_shared<PeerInfo>(*m_peer);
    }

private:
    std::shared_ptr<PeerInfo> m_peer;
    //The ID of the peer. Typically the hash of the IP Addr
    std::string m_id;
    //IP Addr
    std::string m_ip;
    //round-trip-time of the peer.
    std::chrono::milliseconds m_rtt;
    //uptime of the peer
    //TODO eva if timestamp of creation would be enough..
    std::chrono::milliseconds m_uptime;

};

#endif /* PEER_H */
