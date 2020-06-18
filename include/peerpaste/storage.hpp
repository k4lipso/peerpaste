#pragma once

#include <boost/filesystem.hpp>
#include <boost/range/iterator_range.hpp>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <iostream>
#include <map>

#include "peerpaste/cryptowrapper.hpp"

class StaticStorage
{
public:
	StaticStorage(const std::string &id)
		: id_(id)
		, storage_path_("/tmp/" + id_ + '/')
	{
		if(not boost::filesystem::create_directories(storage_path_))
		{
			util::log(error, "Cant create directory");
		}
	}

	~StaticStorage()
	{
	}

	void put(const std::string &data, const std::string &id)
	{
		std::ofstream out(storage_path_ + id);
		out << data;
		out.close();
	}

	void remove(const std::string &id)
	{
		if(exists(storage_path_ + id))
		{
			std::remove(std::string(storage_path_ + id).c_str());
		}
		util::log(debug, "[storage] could not remove file");
	}

	std::string get(const std::string &id)
	{
		std::ifstream ifs(storage_path_ + id);
		assert(ifs.good());
		std::string content((std::istreambuf_iterator<char>(ifs)), (std::istreambuf_iterator<char>()));
		return content;
	}

	bool exists(const std::string &id)
	{
		std::ifstream f(storage_path_ + id);
		return f.good();
	}

	auto get_files()
	{
		std::vector<std::string> file_names;
		for(const auto &entry : std::filesystem::directory_iterator(storage_path_))
		{
			file_names.push_back(entry.path().filename().string());
		}

		return file_names;
	}

private:
	const std::string id_;
	const std::string storage_path_;
};

class Storage
{
public:
	Storage(const std::string &id)
		: id_(id)
		, storage_path_("/tmp/" + id_ + '/')
	{
		if(not boost::filesystem::create_directories(storage_path_))
		{
			util::log(error, "Cant create directory");
		}
	}
	~Storage()
	{
	}

	void put(const std::string &data, const std::string &id)
	{
		std::ofstream out(storage_path_ + id);
		out << data;
		out.close();
		files_[id] = std::chrono::steady_clock::now();
	}

	void remove(const std::string &id)
	{
		if(exists(storage_path_ + id))
		{
			std::remove(std::string(storage_path_ + id).c_str());
			files_.erase(files_.find(id));
		}
		util::log(debug, "[storage] could not remove file");
	}

	std::string get(const std::string &id)
	{
		std::ifstream ifs(storage_path_ + id);
		assert(ifs.good());
		std::string content((std::istreambuf_iterator<char>(ifs)), (std::istreambuf_iterator<char>()));
		return content;
	}

	bool exists(const std::string &id)
	{
		std::ifstream f(storage_path_ + id);
		return f.good();
	}

	auto get_map() const
	{
		return files_;
	}

	void refresh_validity(const std::string &id)
	{
		files_[id] = std::chrono::steady_clock::now();
	}

	bool is_valid(const std::string &id)
	{
		std::chrono::steady_clock::time_point end = std::chrono::steady_clock::now();
		auto dur = std::chrono::duration_cast<std::chrono::seconds>(end - files_[id]).count();
		// older than 60seconds is invalid
		if(dur > 60)
		{
			return false;
		}
		return true;
	}

private:
	const std::string id_;
	const std::string storage_path_;
	// a map of files that keeps track of the validity.
	// when the validity is out of scope the file will be removed
	std::map<std::string, std::chrono::steady_clock::time_point> files_;
};
