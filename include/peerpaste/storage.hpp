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
	StaticStorage(const std::string &id)
		: id_(id)
		, storage_path_("/tmp/peerpaste/" + id_ + '/')
	{
		if(not boost::filesystem::create_directories(storage_path_))
		{
			util::log(error, "Cant create directory");
		}
		sync_files(); //if on creation some files are not completed this causes problems
	}

	~StaticStorage()
	{
	}

	void sync_files()
	{
		std::cout << "STORAGE_PATH: " << storage_path_ << "\n";
		for(const auto &entry : std::filesystem::directory_iterator(storage_path_))
		{
			std::cout << "ADD " << entry.path().filename().string() << "\n";
			files_.emplace_back(entry.path().filename().string(),
															std::filesystem::file_size(entry.path()));
		}
	}

	std::optional<std::ofstream> create_file(const std::string& filename)
	{
		std::scoped_lock lk{mutex_};

		if(exists_internal(filename) || is_blocked_internal(filename))
		{
			util::log(error, "Tried creating existing file");
			return std::nullopt;
		}

		std::ofstream Output;
		Output.open(storage_path_ + filename, std::ios_base::binary);
		if (!Output) {
			util::log(error, "Failed to create file");
			return std::nullopt;
		}

		blocked_files_.push_back(filename);

		return Output;
	}

	bool finalize_file(const std::string& filename)
	{
		std::scoped_lock lk{mutex_};

		if(exists_internal(filename))
		{
			util::log(error, "Tried finalizing file that already exists");
			return false;
		}

		if(!is_blocked_internal(filename))
		{
			util::log(error, "Tried finalizing file that was not blocked");
			return false;
		}


		blocked_files_.erase(std::remove_if(blocked_files_.begin(), blocked_files_.end(),
																				[&filename](const auto& file){ return filename == file; }),
												 blocked_files_.end());

		std::cout << "FINALIZE FILE: " << storage_path_ + filename << " with size: " <<
								 std::filesystem::file_size(storage_path_ + filename) << "\n";
		files_.emplace_back(filename, std::filesystem::file_size(storage_path_ + filename));
		return true;
	}

	std::optional<std::ifstream> read_file(const std::string& filename)
	{
		if(!exists(filename))
		{
			util::log(error, "Tried reading nonexisting file");
			return std::nullopt;
		}

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

	bool is_blocked(const std::string& id) const
	{
		std::scoped_lock lk{mutex_};
		return is_blocked_internal(id);
	}

	bool exists(const std::string &id) const
	{
		std::scoped_lock lk{mutex_};
		return exists_internal(id);
		//std::ifstream f(storage_path_ + id);
		//return f.good();
	}

	auto get_files()
	{
		std::scoped_lock lk{mutex_};
		return files_;
		//std::vector<peerpaste::FileInfo> file_names;
		//for(const auto &entry : std::filesystem::directory_iterator(storage_path_))
		//{
		//	if(!exists(entry.path().filename().string()))
		//	{
		//		continue;
		//	}

		//	file_names.emplace_back(entry.path().filename().string(),
		//													std::filesystem::file_size(entry.path()));
		//}

		//return file_names;
	}

private:
	bool is_blocked_internal(const std::string& id) const
	{
		return std::any_of(blocked_files_.begin(), blocked_files_.end(),
											 [&id](const auto& file_name){ return file_name == id; });
	}

	bool exists_internal(const std::string &id) const
	{
		return std::any_of(files_.begin(), files_.end(),
											 [&id](const auto& file_info){ return file_info == id; });
	}


	const std::string id_;
	const std::string storage_path_;

	mutable std::mutex mutex_;
	std::vector<peerpaste::FileInfo> files_;
	std::vector<std::string> blocked_files_;
};
