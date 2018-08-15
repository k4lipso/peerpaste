#ifndef SESSION_H
#define SESSION_H

class session
    : public std::enable_shared_from_this<session>
{
public:
    session(tcp::socket socket)
        : socket_(std::move(socket))
    {
    }

    void start();

private:

    void find_successor(/*ID*/);

    void check_precending_node(/*ID*/);

    void do_read_header();

    void do_read_body();

    std::string Put(int level_of_decryption);

    std::string Get(size_t hash);

    void do_write(std::size_t length);

    paste m_paste;
    tcp::socket socket_;
    char data_[paste::header_length + paste::max_body_length];
    std::string hash_;
    char msg_[paste::max_body_length];
};

#endif /* SESSION_H */
