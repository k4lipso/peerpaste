#ifndef MESSAGE_HPP
#define MESSAGE_HPP

#include <sstream>
#include <vector>

#include "cryptowrapper.hpp"
#include "header.hpp"
#include "peer.hpp"

namespace peerpaste
{

struct FileInfo
{
	FileInfo(std::string name, size_t size)
		: file_name(std::move(name))
		, file_size(size)
	{
	}

	FileInfo(std::string name, std::string sha256sum_, size_t size)
		: file_name(std::move(name))
		, sha256sum(std::move(sha256sum_))
		, file_size(size)
	{
	}

	FileInfo(std::string name, std::string sha256sum_, size_t size, size_t offset_)
		: file_name(std::move(name))
		, sha256sum(std::move(sha256sum_))
		, file_size(size)
		, offset(offset_)
	{
	}

	bool operator==(const std::string& other) const
	{
		return file_name == other;
	}

	std::string file_name;
	std::string sha256sum;
	size_t file_size = 0;
	size_t offset = 0;
};

struct FileChunk
{
	FileChunk() = default;
	FileChunk(char* data_ptr, size_t data_size)
		: size(data_size)
		,	data{data_ptr, data_ptr + data_size}
	{
	}

	FileChunk(char* data_ptr, size_t data_size, size_t data_offset)
		: size(data_size)
		, offset(data_offset)
		,	data{data_ptr, data_ptr + data_size}
	{
	}

	static constexpr size_t DEFAULT_BUF_SIZE = 1024;
	size_t size = 0;
	size_t offset = 0;
	std::vector<char> data;
};

}

class Message
{
public:
	Message()
		: data_("")
	{
	}

	/*
	 * generates a Message object. it should not be modified anymore,
	 * otherwise transaction_id would be invalid and must be regenerated
	 */
	static Message create_request(std::string request_type)
	{
		Message msg;
		msg.set_header(Header(true, 0, 0, request_type, "", "", ""));
		msg.generate_transaction_id();
		return msg;
	}

	static Message create_request(std::string request_type, std::vector<Peer> peers)
	{
		Message msg;
		msg.set_header(Header(true, 0, 0, request_type, "", "", ""));
		msg.set_peers(std::move(peers));
		msg.generate_transaction_id();
		return msg;
	}

	void print() const
	{
		/* return; */
		if(get_request_type() == "get_predecessor" || get_request_type() == "notify" ||
			 get_request_type() == "check_predecessor")
		{
			return;
		}
		std::cout << "########## MESSAGE BEGIN ##########" << '\n';
		std::cout << "##### HEADER #####" << '\n';
		header_.print();
		std::cout << "##### PEERS #####" << '\n';
		int i = 0;
		for(const auto &peer : peers_)
		{
			std::cout << "PEER " << i << std::endl;
			peer.print();
			i++;
		}
		std::cout << "##### DATA ######" << std::endl;
		std::cout << data_ << std::endl;
		std::cout << "########## MESSAGE END ##########" << '\n';
	}

	void set_header(const Header &header)
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

	void set_peers(const std::vector<Peer> &peers)
	{
		peers_ = peers;
	}

	void add_peer(const Peer &peer)
	{
		peers_.push_back(peer);
	}

	std::string stringify() const
	{
		std::stringstream str;
		str << header_.stringify();
		for(const auto peer : peers_)
		{
			str << peer.stringify();
		}
		return str.str();
	}

	// TODO: this function should generate a response
	// by setting t_flag and putting transaction_id
	// into correlational_id
	std::shared_ptr<Message> generate_response()
	{
		if(!is_request())
		{
			std::cout << "Cant generate_response, is allready one"
								<< " Type: " << header_.get_request_type() << '\n';
		}
		Message response;
		response.set_header(header_.generate_response_header());
		return std::make_shared<Message>(response);
	}

	bool is_request() const
	{
		return header_.is_request();
	}

	std::string generate_transaction_id()
	{
		static std::atomic<unsigned int> Counter = 0;


		if(Counter == std::numeric_limits<unsigned int>::max())
		{
			Counter = 0;
		}

		const auto now = std::chrono::system_clock::now();
		const auto now_time_t = std::chrono::system_clock::to_time_t(now);
		const auto salt = std::to_string(now_time_t) + std::to_string(++Counter);

		const auto transaction_id = util::generate_sha256(stringify() + salt);

		header_.set_transaction_id(transaction_id);
		return transaction_id;
	}

	std::string get_transaction_id() const
	{
		return get_header().get_transaction_id();
	}

	void set_correlational_id(const std::string &id)
	{
		get_header().set_correlational_id(id);
	}

	std::string get_correlational_id() const
	{
		return get_header().get_correlational_id();
	}

	std::string get_request_type() const
	{
		return get_header().get_request_type();
	}

	void set_data(const std::string &data)
	{
		data_ = data;
	}

	std::string get_data()
	{
		return data_;
	}

	void add_file(peerpaste::FileInfo file_info)
	{
		files_.push_back(std::move(file_info));
	}

	void add_file(const std::string &filename, int size = 0)
	{
		files_.emplace_back(filename, size);
	}

	void set_filelist(const std::vector<peerpaste::FileInfo> &files)
	{
		files_ = files;
	}

	auto get_files() const
	{
		return files_;
	}

	void set_file_chunk(const peerpaste::FileChunk& file_chunk)
	{
		file_chunk_ = file_chunk;
	}

	auto& get_file_chunk()
	{
		return file_chunk_;
	}

private:
	Header header_;
	std::vector<Peer> peers_;
	std::vector<peerpaste::FileInfo> files_;
	std::string data_;
	std::optional<peerpaste::FileChunk> file_chunk_;
};

#endif /* MESSAGE_HPP */
