#include "peerpaste/boost_session.hpp"
#include "peerpaste/concurrent_queue.hpp"
#include "peerpaste/message_converter.hpp"

#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <boost/thread.hpp>
#include <boost/thread/future.hpp>

#include <iostream>
#include <memory>

using boost::asio::ip::tcp;

BoostSession::BoostSession(
	boost::asio::io_context &io_context,
	std::shared_ptr<peerpaste::ConcurrentQueue<std::pair<std::vector<uint8_t>, SessionPtr>>> msg_queue)
	: service_(io_context)
	, write_strand_(io_context)
	, read_strand_(io_context)
	, socket_(io_context)
{
	msg_queue_ = std::move(msg_queue);
}

BoostSession::~BoostSession()
{
}

boost::asio::ip::tcp::socket &BoostSession::get_socket()
{
	return socket_;
}

void BoostSession::stop()
{
	socket_.close();
}

void BoostSession::write(const std::vector<uint8_t> &encoded_message)
{
	service_.post(
		write_strand_.wrap([me = shared_from_this(), encoded_message]() { me->queue_message(encoded_message); }));
}

void BoostSession::write_to(const std::vector<uint8_t> &encoded_message,
														const std::string &address,
														const std::string &port)
{
	tcp::resolver resolver(service_);
	auto endpoint = resolver.resolve(address, port);

	boost::asio::async_connect(
		socket_,
		endpoint,
		[this, me = shared_from_this(), endpoint, encoded_message](boost::system::error_code ec, tcp::endpoint) {
			if(!ec)
			{
				me->write(encoded_message);
			}
			else
			{
				std::cout << "error: " << ec << std::endl;
			}
		});
}

void BoostSession::read()
{
	do_read_header();
}

const std::string BoostSession::get_client_ip() const
{
	return socket_.remote_endpoint().address().to_string();
}

const unsigned BoostSession::get_client_port() const
{
	return socket_.remote_endpoint().port();
}

void BoostSession::queue_message(const std::vector<uint8_t> &message)
{
	bool write_in_progress = !send_packet_queue.empty();
	send_packet_queue.push_back(std::move(message));

	if(!write_in_progress)
	{
		start_packet_send();
	}
}

void BoostSession::start_packet_send()
{
	async_write(socket_,
							boost::asio::buffer(send_packet_queue.front()),
							write_strand_.wrap([me = shared_from_this()](boost::system::error_code const &ec, std::size_t) {
								me->packet_send_done(ec);
							}));
}

void BoostSession::packet_send_done(boost::system::error_code const &error)
{
	if(!error)
	{
		/* auto vec = send_packet_queue.front(); */
		send_packet_queue.pop_front();

		// TODO: move out of if statement so it gets called everytime?
		if(!send_packet_queue.empty())
		{
			start_packet_send();
		}
	}
	else if(error != boost::asio::error::operation_aborted)
	{
		stop();
	}
}

void BoostSession::handle_read_message(const boost::system::error_code &ec)
{
	if(!ec)
	{
		auto begin = readbuf_.begin() + header_size_;
		auto end = readbuf_.end();
		std::vector<uint8_t> message_buf(begin, end);
		msg_queue_->push(std::make_pair(std::move(message_buf), shared_from_this()));
	}
	else
	{
		std::cout << "error in handle read message: " << ec << std::endl;
		do_read_header();
	}
}

void BoostSession::do_read_message(unsigned msg_len)
{
	readbuf_.resize(header_size_ + msg_len);
	boost::asio::mutable_buffers_1 buf = boost::asio::buffer(&readbuf_[header_size_], msg_len);
	boost::asio::async_read(socket_, buf, [me = shared_from_this()](boost::system::error_code const &error, std::size_t) {
		me->handle_read_message(error);
	});
}

void BoostSession::handle_read_header(const boost::system::error_code &error)
{
	if(!error)
	{
		unsigned msg_len = decode_header(readbuf_);

		do_read_message(msg_len);
		return;
	}
	else if(error != boost::asio::error::operation_aborted)
	{
		stop();
	}

	return;
}

// TODO: this function has to post to the read_strand_
void BoostSession::do_read_header()
{
	readbuf_.resize(header_size_);

	boost::asio::async_read(
		socket_,
		boost::asio::buffer(readbuf_),
		[me = shared_from_this()](boost::system::error_code const &error, std::size_t) { me->handle_read_header(error); });
}

unsigned BoostSession::decode_header(const DataBuffer &buf) const
{
	if(buf.size() < header_size_)
		return 0;
	unsigned msg_size = 0;
	for(unsigned i = 0; i < header_size_; ++i)
	{
		msg_size = msg_size * 256 + (static_cast<unsigned>(buf[i]) & 0xFF);
	}
	return msg_size;
}

void BoostSession::encode_header(DataBuffer &buf, unsigned size) const
{
	assert(buf.size() >= header_size_);
	buf[0] = static_cast<boost::uint8_t>((size >> 24) & 0xFF);
	buf[1] = static_cast<boost::uint8_t>((size >> 16) & 0xFF);
	buf[2] = static_cast<boost::uint8_t>((size >> 8) & 0xFF);
	buf[3] = static_cast<boost::uint8_t>(size & 0xFF);
}
