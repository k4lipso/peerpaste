#ifndef CRYPTOWRAPPER_HPP
#define CRYPTOWRAPPER_HPP

#include <sha.h>
#include <filters.h>
#include <hex.h>
#include <sodium.h>
#include <cctype>
#include <iostream>

namespace util {

    /**
     * Generates an SHA256 hash from the given string using crypto++
     */
    const std::string generate_sha256(const std::string& data);
    const std::string generate_sha256(const std::string& ip, const std::string& port);
    const std::string encrypt(const std::string& key_str, const std::string& data);
    const std::string decrypt(const std::string& key_str, const std::string& data);

    const bool between(const std::string& id_1, const std::string& id_2,
                                                const std::string& id_3);
    const size_t generate_hash(const std::string& data);
    const size_t generate_limited_hash(const std::string& data, const size_t limit);
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

#endif /* CRYPTOWRAPPER_HPP */
