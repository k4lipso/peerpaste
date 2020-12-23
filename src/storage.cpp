#include "peerpaste/storage.hpp"

StaticStorage::StaticStorage(const std::string &id)
	: id_(id)
	, storage_path_("/tmp/peerpaste/" + id_ + '/')
{
	if(not boost::filesystem::create_directories(storage_path_))
	{
		util::log(error, "Cant create directory");
	}
	sync_files(); //if on creation some files are not completed this causes problems
}

StaticStorage::~StaticStorage()
{
}

void StaticStorage::sync_files()
{
	util::log(info, "start syncing storage...");
	for(const auto &entry : std::filesystem::directory_iterator(storage_path_))
	{
		const std::string name = entry.path().filename().string();
		util::log(info, std::string("add ") + name);
		util::log(info, util::sha256_from_file(storage_path_ + name));

		if(!add_file(name))
		{
			util::log(info, std::string("failed to add ") + name);
		}
	}

	util::log(info, "done syncing storage");
}

bool StaticStorage::add_file(const std::string& filename)
{
	std::ifstream f(storage_path_ + filename);
	if(!f.good())
	{
		return false;
	}

	std::scoped_lock lk{mutex_};
	files_.emplace_back(filename, std::filesystem::file_size(storage_path_ + filename));

	return true;
}

std::optional<std::ofstream> StaticStorage::create_file(const std::string& filename)
{
	std::scoped_lock lk{mutex_};

	if(exists_internal(filename) || is_blocked_internal(filename))
	{
		util::log(error, "Tried creating existing file"); return std::nullopt;
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

bool StaticStorage::finalize_file(const std::string& filename)
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

	files_.emplace_back(filename, std::filesystem::file_size(storage_path_ + filename));
	return true;
}

std::optional<std::ifstream> StaticStorage::read_file(const std::string& filename)
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


void StaticStorage::put(const std::string &data, const std::string &id)
{
	std::ofstream out(storage_path_ + id);
	out << data;
	out.close();
}

void StaticStorage::remove(const std::string &id)
{
	if(exists(storage_path_ + id))
	{
		std::remove(std::string(storage_path_ + id).c_str());
	}
	util::log(debug, "[storage] could not remove file");
}

std::string StaticStorage::get(const std::string &id)
{
	std::ifstream ifs(storage_path_ + id);
	assert(ifs.good());
	std::string content((std::istreambuf_iterator<char>(ifs)), (std::istreambuf_iterator<char>()));
	return content;
}

bool StaticStorage::is_blocked(const std::string& id) const
{
	std::scoped_lock lk{mutex_};
	return is_blocked_internal(id);
}

bool StaticStorage::exists(const std::string &id) const
{
	std::scoped_lock lk{mutex_};
	return exists_internal(id);
	//std::ifstream f(storage_path_ + id);
	//return f.good();
}

std::vector<peerpaste::FileInfo> StaticStorage::get_files()
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

bool StaticStorage::is_blocked_internal(const std::string& id) const
{
	return std::any_of(blocked_files_.begin(), blocked_files_.end(),
										 [&id](const auto& file_name){ return file_name == id; });
}

bool StaticStorage::exists_internal(const std::string &id) const
{
	return std::any_of(files_.begin(), files_.end(),
										 [&id](const auto& file_info){ return file_info == id; });
}
