#ifndef MESSAGE_HPP
#define MESSAGE_HPP

#include "peer.hpp"
#include "header.hpp"

#include <vector>

class Message
{
public:
    Message() {}

    const void print() const
    {
        std::cout << "########## MESSAGE BEGIN ##########" << '\n';
        std::cout << "##### HEADER #####" << '\n';
        header_.print();
        std::cout << "##### PEERS #####" << '\n';
        int i = 0;
        for(const auto& peer : peers_){
            std::cout << "PEER " << i << std::endl;
            peer.print();
            i++;
        }
        std::cout << "########## MESSAGE END ##########" << '\n';
    }

    const void set_header(const Header& header)
    {
        header_ = header;
    }

    Header get_header() const
    {
        return header_;
    }

    std::vector<Peer> get_peers() const
    {
        return peers_;
    }

    const void set_peers(const std::vector<Peer>& peers)
    {
        peers_ = peers;
    }

    const void add_peer(const Peer& peer)
    {
        peers_.push_back(peer);
    }

private:
    Header header_;
    std::vector<Peer> peers_;
};

#endif /* MESSAGE_HPP */
