#include <boost/filesystem.hpp>
#include <iostream>

class Storage
{
public:
    Storage (const std::string& id) : id_(id), storage_path_("/tmp/" + id_ + '/')
    {
        if(not boost::filesystem::create_directories(storage_path_)){
            util::log(error, "Cant create directory");
        }
    }
    ~Storage () {}

    void put(const std::string& data, const std::string& id)
    {
        std::ofstream out(storage_path_ + id);
        out << data;
        out.close();
    }

    std::string get(const std::string& id){
        std::ifstream ifs(storage_path_ + id);
        assert(ifs.good());
        std::string content( (std::istreambuf_iterator<char>(ifs) ),
                             (std::istreambuf_iterator<char>()    ) );
        return content;
    }

    bool exists(const std::string& id){
        std::ifstream f(storage_path_ + id);
        return f.good();
    }

private:
    const std::string id_;
    const std::string storage_path_;
};
