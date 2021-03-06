#ifndef PEER_H
#define PEER_H

#include <chrono>
#include <iostream>
#include <sstream>
#include <string>

#include "proto/messages.pb.h"

/**
 * A Peer Object, used to be stored in a Routing or Neighbor Table.
 */
class Peer
{
public:
	Peer()
		: id_("")
		, ip_("")
		, port_("")
	{
	}
	Peer(std::string id, std::string ip, std::string port)
		: id_(id)
		, ip_(ip)
		, port_(port)
	{
	}

	bool operator==(const Peer &peer)
	{
		return get_id() == peer.get_id() && get_ip() == peer.get_ip() && get_port() == peer.get_port();
	}

	bool operator!=(const Peer &peer)
	{
		return get_id() != peer.get_id() || get_ip() != peer.get_ip() || get_port() != peer.get_port();
	}

	void print() const
	{
		std::cout << "IP: " << get_ip() << '\n';
		std::cout << "ID: " << get_id() << '\n';
		std::cout << "Port: " << get_port() << std::endl;
	}

	void set_id(const std::string &id)
	{
		id_ = id;
	}

	std::string get_id() const
	{
		return id_;
	}

	void set_ip(const std::string &ip)
	{
		ip_ = ip;
	}

	std::string get_ip() const
	{
		return ip_;
	}

	void set_port(const std::string &port)
	{
		port_ = port;
	}

	std::string get_port() const
	{
		return port_;
	}

	const std::string stringify() const
	{
		std::stringstream str;
		str << id_ << ip_ << port_;
		return str.str();
	}

	bool is_valid()
	{
		return (id_ != "") && (ip_ != "") && (port_ != "");
	}

private:
	std::string id_;
	std::string ip_;
	std::string port_;
};

#endif /* PEER_H */
