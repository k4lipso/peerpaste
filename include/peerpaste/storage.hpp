#pragma once

#include <boost/filesystem.hpp>
#include <boost/range/iterator_range.hpp>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <iostream>
#include <map>
#include <mutex>

#include "peerpaste/cryptowrapper.hpp"
#include "peerpaste/message.hpp"

class StaticStorage
{
public:
	StaticStorage(const std::string &id);
	~StaticStorage();

	void sync_files();
	bool add_file(const std::string& filename);
	std::optional<std::ofstream> create_file(const std::string& filename);
	bool finalize_file(const std::string& filename);
	std::optional<std::ifstream> read_file(const std::string& filename);

	void put(const std::string &data, const std::string &id);
	void remove(const std::string &id);
	std::string get(const std::string &id);
	bool is_blocked(const std::string& id) const;
	bool exists(const std::string &id) const;
	std::vector<peerpaste::FileInfo> get_files();

private:
	bool is_blocked_internal(const std::string& id) const;
	bool exists_internal(const std::string &id) const;


	const std::string id_;
	const std::string storage_path_;

	mutable std::mutex mutex_;
	std::vector<peerpaste::FileInfo> files_;
	std::vector<std::string> blocked_files_;
};
