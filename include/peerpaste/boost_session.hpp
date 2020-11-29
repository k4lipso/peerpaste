#ifndef BOOSTSESSION_HPP
#define BOOSTSESSION_HPP

#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <boost/thread.hpp>
#include <boost/thread/future.hpp>
#include <iostream>
#include <memory>
#include <string>

#include "peerpaste/concurrent_queue.hpp"
#include "peerpaste/message.hpp"
#include "peerpaste/session.hpp"

using boost::asio::ip::tcp;
/* using DataBuffer = std::vector<uint8_t>; */

// Forward declaration
class MessageQueue;

class BoostSession : public Session, public std::enable_shared_from_this<BoostSession>
{
public:
	BoostSession(boost::asio::io_context &io_context,
							 std::shared_ptr<peerpaste::ConcurrentQueue<std::pair<DataBuffer, SessionPtr>>> msg_queue);
	~BoostSession();
	void write(const DataBuffer &encoded_message) override;
	void write_direct(const DataBuffer& encoded_message, const std::function<void(bool)>& handler) override;
	void write_to(const DataBuffer &encoded_message, const std::string &address, const std::string &port) override;
	void read() override;
	const std::string get_client_ip() const override;

	const unsigned get_client_port() const;
	boost::asio::ip::tcp::socket &get_socket();

private:
	void stop();
	void queue_message(const DataBuffer &message);
	void start_packet_send();
	void packet_send_done(boost::system::error_code const &error);
	void handle_read_message(const boost::system::error_code &ec);
	void do_read_message(unsigned msg_len);
	void handle_read_header(const boost::system::error_code &error);
	// TODO: this function has to post to the read_strand_
	void do_read_header();
	unsigned decode_header(const DataBuffer &buf) const;
	void encode_header(DataBuffer &buf, unsigned size) const;

	boost::asio::io_service &service_;
	boost::asio::io_service::strand write_strand_;
	boost::asio::io_service::strand read_strand_;
	std::deque<DataBuffer> send_packet_queue;
	DataBuffer readbuf_;

	tcp::socket socket_;
	std::string name_;
	const size_t header_size_ = 4;
};

#endif /* ifndef BOOSTSESSION_HPP */
