#pragma once

#include <boost/filesystem.hpp>
#include <boost/range/iterator_range.hpp>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <iostream>
#include <map>
#include <mutex>

#include "sqlite_modern_cpp.h"
#include "peerpaste/message.hpp"

class OfstreamWrapper
{
public:
	OfstreamWrapper(const peerpaste::FileInfo& file_info, const std::string& db_path)
		: m_file_info_(file_info)
		, m_database_(db_path)
	{
	}

	~OfstreamWrapper()
	{
		if(auto shared_token = m_write_token_.lock())
		{
			*shared_token.get() = false;
		}

		m_ofstream_.close();
	}

	OfstreamWrapper(OfstreamWrapper&& other) = default;
	OfstreamWrapper& operator=(OfstreamWrapper&& other) = default;

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

	void write(const char* s, std::streamsize count, size_t offset = 0)
	{
		m_ofstream_.seekp(offset);
		m_ofstream_.write(s, count);
		m_ofstream_.flush();

		m_database_ << "begin;";
		m_database_ << "update file_transfer set bytes_written = ? where file_name = ?;"
								<< offset
								<< m_file_info_.file_name;
		m_database_ << "commit;";
	}

	void write(const peerpaste::FileChunk& chunk)
	{
		write(chunk.data.data(), chunk.size, chunk.offset);
	}

	std::ofstream::traits_type::pos_type tellp()
	{
		return m_ofstream_.tellp();
	}

	void flush()
	{
		m_ofstream_.flush();
	}

	void set_token(std::weak_ptr<std::atomic<bool>> token)
	{
		m_write_token_ = token;
	}

	std::ofstream m_ofstream_;

private:

	peerpaste::FileInfo m_file_info_;
	sqlite::database m_database_;
	bool m_is_valid_ = true;
	std::weak_ptr<std::atomic<bool>> m_write_token_;
};

class StaticStorage
{
public:
	StaticStorage(const std::string &id);
	~StaticStorage();

	void sync_files();
	void init_db();

	bool add_file(const std::string& filename);
	std::optional<OfstreamWrapper> create_file(peerpaste::FileInfo& file_info);
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
	std::vector<std::pair<std::string, std::shared_ptr<std::atomic<bool>>>> blocked_files_;
	sqlite::database database_;
};
