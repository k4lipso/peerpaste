#include <sha.h>
#include <filters.h>
#include <hex.h>

namespace util {

    /**
     * Generates an SHA256 hash from the given string using crypto++
     */
    const std::string generate_sha256(const std::string& data)
    {
        CryptoPP::SHA256 hash;
        std::string digest;

        CryptoPP::StringSource s(data, true,
                                  new CryptoPP::HashFilter(hash,
                                  new CryptoPP::HexEncoder(new CryptoPP::StringSink(digest))));

        return digest;
    }

    const std::string generate_sha256(const std::string& ip, const std::string& port)
    {
        CryptoPP::SHA256 hash;
        std::string digest;

        CryptoPP::StringSource s((ip + port), true,
                                  new CryptoPP::HashFilter(hash,
                                  new CryptoPP::HexEncoder(new CryptoPP::StringSink(digest))));

        return digest;
    }

    const bool between(const std::string& id_1,
                       const std::string& id_2,
                       const std::string& id_3)
    {
        if ( id_1.compare(id_3) < 0 ){
            return ( id_1.compare(id_2) < 0 ) &&
                   ( id_2.compare(id_3) < 0 );
        }

        return ( id_1.compare(id_2) < 0 ) ||
               ( id_2.compare(id_3) < 0 );
    }

    const size_t generate_hash(const std::string& data)
    {
        return std::hash<std::string>{}(data);
    }

    const size_t generate_limited_hash(const std::string& data, const size_t limit)
    {
        return std::hash<std::string>{}(data) % limit;
    }

    /**
     * Generates an SHA256 hash from the given string using crypto++
     */
    /* void generate_sha256(const std::string& data, std::string& hashed) */
    /* { */
    /*     CryptoPP::SHA256 hash; */

    /*     CryptoPP::StringSource s(data, true, */
    /*                               new CryptoPP::HashFilter(hash, */
    /*                               new CryptoPP::HexEncoder(new CryptoPP::StringSink(hashed)))); */

    /* } */

}
