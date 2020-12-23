#include "peerpaste/cryptowrapper.hpp"

#include <fstream>

namespace util
{

/**
 * Generates an SHA256 hash from the given string using crypto++
 */
std::string generate_sha256(const std::string &data)
{
	CryptoPP::SHA256 hash;
	std::string digest;

	CryptoPP::StringSource s(
		data, true, new CryptoPP::HashFilter(hash, new CryptoPP::HexEncoder(new CryptoPP::StringSink(digest))));

	return digest;
}

std::string generate_sha256(const std::string &ip, const std::string &port)
{
	CryptoPP::SHA256 hash;
	std::string digest;

	CryptoPP::StringSource s(
		(ip + port), true, new CryptoPP::HashFilter(hash, new CryptoPP::HexEncoder(new CryptoPP::StringSink(digest))));

	return digest;
}

std::string sha256_from_file( const std::string& path ) {
	std::ifstream file_stream( path );
	CryptoPP::FileSource fsource( file_stream, true );
	CryptoPP::SHA256 sha;
	unsigned char digest[sha.DigestSize()];
	unsigned char *data = new unsigned char[ fsource.MaxRetrievable() ];
	size_t size = fsource.Get( data, fsource.MaxRetrievable() );
	sha.CalculateDigest( digest, data, size );
	std::stringstream ss;
	for(int i = 0; i < sha.DigestSize(); i++)
		ss << std::hex << std::setw(2) << std::setfill('0') << (int) digest[i];
	delete[] data;

	return ss.str();
}

std::string encrypt(const std::string &key_str, const std::string &data_str)
{
	unsigned char message[data_str.length()];
	std::copy(data_str.data(), data_str.data() + data_str.length(), message);

	std::vector<uint8_t> key(key_str.begin(), key_str.end());
	std::vector<uint8_t> nonce(crypto_secretbox_NONCEBYTES, 0);
	unsigned char ciphertext[crypto_secretbox_MACBYTES + data_str.length()];

	crypto_secretbox_easy(ciphertext, message, data_str.length(), nonce.data(), key.data());
	return std::string(&ciphertext[0], &ciphertext[crypto_secretbox_MACBYTES + data_str.length()]);
}

std::string decrypt(const std::string &key_str, const std::string &data_str)
{
	std::vector<uint8_t> key(key_str.begin(), key_str.end());
	std::vector<uint8_t> nonce(crypto_secretbox_NONCEBYTES, 0);

	unsigned char ciphertext[data_str.length()];
	std::copy(data_str.data(), data_str.data() + data_str.length(), ciphertext);

	unsigned char decrypted[data_str.length() - crypto_secretbox_MACBYTES];

	if(crypto_secretbox_open_easy(decrypted, ciphertext, data_str.length(), nonce.data(), key.data()) != 0)
	{
		util::log(notify, "Message forged, or wrong Key. It cant be decrypted");
	}

	return std::string(&decrypted[0], &decrypted[data_str.length() - crypto_secretbox_MACBYTES]);
}

bool between(const std::string &id_1, const std::string &id_2, const std::string &id_3)
{
	if(id_1.compare(id_3) < 0)
	{
		return (id_1.compare(id_2) < 0) && (id_2.compare(id_3) < 0);
	}

	return (id_1.compare(id_2) < 0) || (id_2.compare(id_3) < 0);
}

size_t generate_hash(const std::string &data)
{
	return std::hash<std::string>{}(data);
}

size_t generate_limited_hash(const std::string &data, const size_t limit)
{
	return std::hash<std::string>{}(data) % limit;
}

void log(severity_level lvl, const std::string &message)
{
	src::severity_logger<severity_level> slg;
	slg.add_attribute("Uptime", attrs::timer());
	BOOST_LOG_SEV(slg, lvl) << message;
}

} // namespace util
