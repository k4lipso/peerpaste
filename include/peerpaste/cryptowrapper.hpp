#pragma once

#include <boost/log/attributes/named_scope.hpp>
#include <boost/log/attributes/timer.hpp>
#include <boost/log/common.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/sources/logger.hpp>
#include <boost/log/support/date_time.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>
#include <boost/log/utility/setup/console.hpp>
#include <boost/log/utility/setup/file.hpp>
#include <cctype>
#include <iostream>

#include "cryptopp/filters.h"
#include "cryptopp/hex.h"
#include "cryptopp/sha.h"
#include "cryptopp/files.h"
#include "sodium.h"

#include "spdlog/spdlog.h"

namespace logging = boost::log;
namespace sinks = boost::log::sinks;
namespace attrs = boost::log::attributes;
namespace src = boost::log::sources;
namespace expr = boost::log::expressions;
namespace keywords = boost::log::keywords;

enum severity_level
{
	notify,
	info,
	warning,
	error,
	critical,
	debug,
	message_out,
	message_in
};

// The formatting logic for the severity level
template<typename CharT, typename TraitsT>
inline std::basic_ostream<CharT, TraitsT> &operator<<(std::basic_ostream<CharT, TraitsT> &strm, severity_level lvl)
{
	static const char *const str[] = {
		"notify", "info", "warning", "error", "critical", "debug", "message_out", "message_in"};
	if(static_cast<std::size_t>(lvl) < (sizeof(str) / sizeof(*str)))
		strm << str[lvl];
	else
		strm << static_cast<int>(lvl);
	return strm;
}

namespace util
{

/**
 * Generates an SHA256 hash from the given string using crypto++
 */
std::string generate_sha256(const std::string &data);
std::string generate_sha256(const std::string &ip, const std::string &port);
std::string sha256_from_file(const std::string& path);
std::string encrypt(const std::string &key_str, const std::string &data);
std::string decrypt(const std::string &key_str, const std::string &data);

bool between(const std::string &id_1, const std::string &id_2, const std::string &id_3);
size_t generate_hash(const std::string &data);
size_t generate_limited_hash(const std::string &data, const size_t limit);

void log(severity_level lvl, const std::string &message);

/* enum severity_level */
/* { */
/*     info, */
/*     warning, */
/*     error, */
/*     critical, */
/*     debug, */
/*     message_out, */
/*     message_in */
/* }; */

/* void log(severity_level lvl, const std::string& message); */

} // namespace util
