#ifndef MESSAGE_HPP
#define MESSAGE_HPP

#include "peer.hpp"
#include "header.hpp"

#include <vector>
#include <sstream>

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

    const std::string stringify() const
    {
        std::stringstream str;
        str << header_.stringify();
        for(const auto peer : peers_){
            str << peer.stringify();
        }
        return str.str();
    }

    //TODO: this function should generate a response
    //by setting t_flag and putting transaction_id
    //into correlational_id
    Message generate_response()
    {
        if(!header_.is_request()){
            std::cout << "Cant generate_response, is allready one"
                      << '\n';
        }
        Message response;
        response.set_header(header_.generate_response_header());
        return response;
    }

private:
    Header header_;
    std::vector<Peer> peers_;
};

#endif /* MESSAGE_HPP */
