#include "peerpaste/storage.hpp"
#include "peerpaste/cryptowrapper.hpp"

StaticStorage::StaticStorage(const std::string &id)
	: id_(id)
	, storage_path_("/tmp/peerpaste/" + id_ + '/')
	, database_("")
{
	if(not boost::filesystem::create_directories(storage_path_))
	{
		spdlog::debug("Cant create directory");
	}

	database_ = sqlite::database(storage_path_ + std::string("db.sqlite"));
	init_db();
	sync_files(); //if on creation some files are not completed this causes problems

}

StaticStorage::~StaticStorage()
{
}

void StaticStorage::sync_files()
{
	spdlog::info("start syncing storage...");
	for(const auto &entry : std::filesystem::directory_iterator(storage_path_))
	{
		const std::string name = entry.path().filename().string();

		if(name == "db.sqlite" || name == "db.sqlite-journal")
		{
			continue;
		}

		bool IsInProgress = false;
		database_ << "select bytes_written from file_transfer where file_name = ? ;"
					<< name
					>> [&]([[maybe_unused]] long int bytes_written) {
						 IsInProgress = true;
					};

		if(IsInProgress)
		{
			continue;
		}

		spdlog::info("add {}", name);

		if(!add_file(name))
		{
			spdlog::info("failed to add {}", name);
		}
	}

	spdlog::info("done syncing storage");
}

void StaticStorage::init_db()
{
  database_ <<
    "create table if not exists file_transfer ("
    "   file_name text not null unique,"
    "   sha256sum text,"
    "   file_size int,"
    "   bytes_written int"
    ");";
}

bool StaticStorage::add_file(const std::string& filename)
{
	std::ifstream f(storage_path_ + filename);
	if(!f.good())
	{
		return false;
	}

	std::scoped_lock lk{mutex_};
	files_.emplace_back(filename, util::sha256_from_file(storage_path_ + filename),
											std::filesystem::file_size(storage_path_ + filename));

	return true;
}

std::optional<OfstreamWrapper> StaticStorage::create_file(peerpaste::FileInfo& file_info)
{
	const std::string& filename = file_info.file_name;

	std::scoped_lock lk{mutex_};
	if(exists_internal(filename) || is_blocked_internal(filename))
	{
		spdlog::debug("Tried creating existing file");
		return std::nullopt;
	}


	//Update FileInfo::offset, this way GetFile::create_request requests with correct offset
	//if file was partly received before
	bool file_exists = false;
	database_ << "select bytes_written from file_transfer where file_name = ? ;"
						<< file_info.file_name
						>> [&](long int bytes_written) {

							 spdlog::info("Found File: {}, setting offset to {} bytes", filename, bytes_written);

							 file_exists = true;
							 file_info.offset = bytes_written;
						};

	OfstreamWrapper Output(file_info, storage_path_ + "db.sqlite");

	if(file_exists)
	{
		Output.open(storage_path_ + filename, std::ios::in|std::ios::out|std::ios::binary);
	}
	else
	{
		Output.open(storage_path_ + filename, std::ios::out|std::ios::binary);
	}



	if (!Output) {
		spdlog::debug("Failed to create file");
		return std::nullopt;
	}


	Output.m_ofstream_.seekp(file_info.offset, Output.m_ofstream_.beg);

	database_ << "begin;";
	database_ << "insert or ignore into file_transfer (file_name,sha256sum,file_size,bytes_written) values (?,?,?,?);"
					 << filename
					 << file_info.sha256sum
					 << file_info.file_size
					 << file_info.offset;
	database_ << "commit;";

	auto token = std::make_shared<std::atomic<bool>>(true);
	Output.set_token(token);
	blocked_files_.emplace_back(filename, std::move(token));

	return Output;
}

bool StaticStorage::finalize_file(const peerpaste::FileInfo& file_info)
{
	std::scoped_lock lk{mutex_};

	const auto& filename = file_info.file_name;

	if(exists_internal(filename))
	{
		spdlog::debug("Tried finalizing file that already exists");
		return false;
	}

	if(!is_blocked_internal(filename))
	{
		spdlog::debug("Tried finalizing file that was not blocked");
		return false;
	}

	const auto sha256sum = util::sha256_from_file(storage_path_ + filename);

	if(sha256sum != file_info.sha256sum)
	{
		spdlog::warn("Received file has different sha256sum than requested file! Aborting");
		return false;
	}

	spdlog::info("finalizing file: {}", sha256sum);

	blocked_files_.erase(std::remove_if(blocked_files_.begin(), blocked_files_.end(),
										[&filename](const auto& file){ return filename == file.first; }),
										blocked_files_.end());

	files_.emplace_back(filename, sha256sum, std::filesystem::file_size(storage_path_ + filename));


	database_ << "begin;";
	database_ << "delete from file_transfer where file_name = ? ;"
						<< filename;
	database_ << "commit;";

	return true;
}


std::optional<std::ifstream> StaticStorage::read_file(const peerpaste::FileInfo& file_info)
{
	const std::string& filename = file_info.file_name;

	if(!exists(filename))
	{
		spdlog::debug("Tried reading nonexisting file");
		return std::nullopt;
	}

	std::ifstream Input;
	Input.open(storage_path_ + filename, std::ios_base::binary | std::ios_base::ate);

	if(!Input)
	{
		spdlog::error("Failed to read file");
		spdlog::error("{}",storage_path_ + filename);
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
	spdlog::debug("[storage] could not remove file");
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
}

bool StaticStorage::exists(const peerpaste::FileInfo &id) const
{
	std::scoped_lock lk{mutex_};
	return exists_internal(id.file_name);
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
										 [&id](const auto& file_name){ return file_name.first == id && *file_name.second.get() == true; });
}

bool StaticStorage::exists_internal(const std::string &id) const
{
	return std::any_of(files_.begin(), files_.end(),
										 [&id](const auto& file_info){ return file_info == id; });
}
