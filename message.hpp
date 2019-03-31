#ifndef MESSAGE_HPP
#define MESSAGE_HPP

#include "cryptowrapper.hpp"
#include "peer.hpp"
#include "header.hpp"
#include <vector>
#include <sstream>
class Message
{
public:
    Message() : data_("") {}

    const void print() const
    {
        return;
        if(get_request_type() == "get_predecessor" ||
                    get_request_type() == "notify"){
            return;
        }
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
        std::cout << "##### DATA ######" << std::endl;
        std::cout << data_ << std::endl;
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
    std::shared_ptr<Message> generate_response()
    {
        if(!is_request()){
            std::cout << "Cant generate_response, is allready one"
                      << '\n';
        }
        Message response;
        response.set_header(header_.generate_response_header());
        return std::make_shared<Message>(response);
    }

    const bool is_request() const
    {
        return header_.is_request();
    }

    void generate_transaction_id()
    {
        auto transaction_id = util::generate_sha256(stringify());
        header_.set_transaction_id(transaction_id);
    }

    const std::string get_transaction_id() const
    {
        return get_header().get_transaction_id();
    }

    const std::string get_correlational_id() const
    {
        return get_header().get_correlational_id();
    }

    const std::string get_request_type() const
    {
        return get_header().get_request_type();
    }

    void set_data(const std::string& data)
    {
        data_ = data;
    }

    std::string get_data()
    {
        return data_;
    }

private:
    Header header_;
    std::vector<Peer> peers_;
    std::string data_;
};

#endif /* MESSAGE_HPP */
