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
#include "sqlite_modern_cpp.h"

class OfstreamWrapper
{
public:	OfstreamWrapper(const peerpaste::FileInfo& file_info, const std::string& db_path)
		: m_file_info_(file_info)
		, m_database_(db_path)
	{
	}

	bool operator!() const
	{
		return !m_ofstream_;
	}

	void open(const std::string& filename, std::ios_base::openmode mode = std::ios_base::out)
	{
		m_ofstream_.open(filename.c_str(), mode);
	}

	void open(const char *filename, std::ios_base::openmode mode = std::ios_base::out)
	{
		m_ofstream_.open(filename, mode);
	}

	void write(const char* s, std::streamsize count)
	{
		m_ofstream_.write(s, count);
	}

	std::ofstream::traits_type::pos_type tellp()
	{
		return m_ofstream_.tellp();
	}

	void flush()
	{
		m_ofstream_.flush();
	}

private:

	peerpaste::FileInfo m_file_info_;
	std::ofstream m_ofstream_;
	sqlite::database m_database_;
	bool m_is_valid_ = true;
};

class StaticStorage
{
public:
	StaticStorage(const std::string &id);
	~StaticStorage();

	void sync_files();
	bool add_file(const std::string& filename);
	std::optional<OfstreamWrapper> create_file(const peerpaste::FileInfo& file_info);
	bool finalize_file(const peerpaste::FileInfo& file_info);
	std::optional<std::ifstream> read_file(const peerpaste::FileInfo& file_info);

	void put(const std::string &data, const std::string &id);
	void remove(const std::string &id);
	std::string get(const std::string &id);
	bool is_blocked(const std::string& id) const;
	bool exists(const std::string &id) const;
	bool exists(const peerpaste::FileInfo &id) const;
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
