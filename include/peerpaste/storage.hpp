#pragma once

#include <boost/filesystem.hpp>
#include <boost/range/iterator_range.hpp>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <iostream>
#include <map>

#include "peerpaste/cryptowrapper.hpp"
#include "peerpaste/message.hpp"

class StaticStorage
{
public:
	StaticStorage(const std::string &id)
		: id_(id)
		, storage_path_("/tmp/peerpaste/" + id_ + '/')
	{
		if(not boost::filesystem::create_directories(storage_path_))
		{
			util::log(error, "Cant create directory");
		}
	}

	~StaticStorage()
	{
	}


	std::optional<std::ofstream> create_file(const std::string& filename)
	{
		std::ofstream Output;
		Output.open(storage_path_ + filename, std::ios_base::binary);
		if (!Output) {
			util::log(error, "Failed to create file");
			return std::nullopt;
		}

		return Output;
	}

	std::optional<std::ifstream> read_file(const std::string& filename)
	{
		std::ifstream Input;
		Input.open(storage_path_ + filename, std::ios_base::binary | std::ios_base::ate);

		if(!Input)
		{
			util::log(error, "Failed to read file");
			util::log(error, storage_path_ + filename);
			return std::nullopt;
		}

		return Input;
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
		std::vector<peerpaste::FileInfo> file_names;
		for(const auto &entry : std::filesystem::directory_iterator(storage_path_))
		{
			file_names.emplace_back(entry.path().filename().string(), std::filesystem::file_size(entry.path()));
		}

		return file_names;
	}

private:
	const std::string id_;
	const std::string storage_path_;
};
