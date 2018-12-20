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
    Peer() {
        m_peer = std::make_shared<PeerInfo>();
    }
    Peer(std::string id, std::string ip, std::string port)
    {
        m_peer = std::make_shared<PeerInfo>();
        m_peer->set_peer_id(id);
        m_peer->set_peer_ip(ip);
        m_peer->set_peer_port(port);
    }

    ~Peer() {}

    void print()
    {
        std::cout << "IP: " << getIP() << '\n';
        std::cout << "ID: " << getID() << '\n';
        std::cout << "Port: " << getPort() << std::endl;
    }

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

    std::shared_ptr<PeerInfo> getPeer()
    {
        return std::make_shared<PeerInfo>(*m_peer);
    }

    void setPeer(std::shared_ptr<PeerInfo> peer)
    {
        m_peer = peer;
    }

    void setPort(std::string port)
    {
        m_peer->set_peer_port(port);
    }

    std::string getPort()
    {
        return m_peer->peer_port();
    }

private:
    std::shared_ptr<PeerInfo> m_peer;
};

#endif /* PEER_H */
