#ifndef SERVER_H
#define SERVER_H

#include <boost/asio.hpp>

class server
{
public:
    server(boost::asio::io_context& io_context, short port)
        : acceptor_(io_context, tcp::endpoint(tcp::v4(), port))
    {
        do_accept();
    }

private:

    void create();

    void join();

    void stabilize();

    void notify();

    void fix_fingers();

    void check_predecessor();

    void do_accept()
    {
        acceptor_.async_accept(
            [this](boost::system::error_code ec, tcp::socket socket)
            {
                if (!ec)
                {
                    std::make_shared<session>(std::move(socket))->start();
                }

                do_accept();
            });
    }

    tcp::acceptor acceptor_;
    std::shared_ptr<RoutingTable> m_fingerTable;
};

#endif /* SERVER_H */
