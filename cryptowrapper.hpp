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

    /**
     * Generates an SHA256 hash from the given string using crypto++
     */
    void generate_sha256(const std::string& data, std::string& hashed)
    {
        CryptoPP::SHA256 hash;

        CryptoPP::StringSource s(data, true,
                                  new CryptoPP::HashFilter(hash,
                                  new CryptoPP::HexEncoder(new CryptoPP::StringSink(hashed))));

    }

}
