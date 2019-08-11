#include "server_http.hpp"
#include "peerpaste/peerpaste.hpp"

// Added for the json-example
#define BOOST_SPIRIT_THREADSAFE
#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ptree.hpp>

// Added for the default_resource example
#include <algorithm>
#include <boost/filesystem.hpp>
#include <fstream>
#include <vector>
#include <memory>
#ifdef HAVE_OPENSSL
#include "crypto.hpp"
#endif

using namespace std;
// Added for the json-example:
using namespace boost::property_tree;

using HttpServer = SimpleWeb::Server<SimpleWeb::HTTP>;

int main() {
  // HTTP-server at port 8080 using 1 thread
  // Unless you do more heavy non-threaded processing in the resources,
  // 1 thread is usually faster than several threads
  HttpServer server;
  std::string host_ip = "localhost";
  std::string host_port = "1337";
  server.config.port = 8080;

  auto peerpaste = std::make_shared<peerpaste::PeerPaste>();
  peerpaste->init(1234, 4);
  peerpaste->run();

  server.resource["^/get/([a-zA-Z0-9]+)$"]["GET"] =
      [&](shared_ptr<HttpServer::Response> response,
          shared_ptr<HttpServer::Request> request) {
        thread work_thread([&, response] {
            auto data = request->path_match[1].str();
            auto data_key = data.substr(0, 64);
            auto data_hash = data.substr(64, 64);
            auto data_future = peerpaste->async_get(host_ip,
                                                    host_port,
                                                    data_hash);
            data_future.wait();
            response->write(util::decrypt(data_key, data_future.get()));
        });
        work_thread.detach();
      };

  server.resource["^/put$"]["POST"] =
      [&](shared_ptr<HttpServer::Response> response,
          shared_ptr<HttpServer::Request> request) {
        thread work_thread([&, request, response] {
            // Retrieve string:
            auto data = request->content.string();

            auto data_key = util::generate_sha256(data, "");
            auto data_encrypted = util::encrypt(data_key, data);
            auto data_future = peerpaste->async_put(host_ip,
                                                    host_port,
                                                    data_encrypted);
            data_future.wait();
            response->write(data_key + data_future.get() + "\n");
        });
        work_thread.detach();
      };

  server.resource["^/get-unencrypted/([a-zA-Z0-9]+)$"]["GET"] =
      [&](shared_ptr<HttpServer::Response> response,
          shared_ptr<HttpServer::Request> request) {
        thread work_thread([&, response] {
                             std::cout << request->path_match[1].str();
          auto data_future = peerpaste->async_get(host_ip, host_port,
                                                  request->path_match[1].str());
          data_future.wait();
          response->write(data_future.get());
        });
        work_thread.detach();
      };

  server.resource["^/put-unencrypted$"]["POST"] =
      [&](shared_ptr<HttpServer::Response> response,
          shared_ptr<HttpServer::Request> request) {
        thread work_thread([&, request, response] {
            // Retrieve string:
            auto content = request->content.string();

            auto data_future = peerpaste->async_put(host_ip, host_port, content);
            data_future.wait();
            response->write(data_future.get() + "\n");
        });
        work_thread.detach();
      };

  // Default GET-example. If no other matches, this anonymous function will be called.
  // Will respond with content in the web/-directory, and its subdirectories.
  // Default file: index.html
  // Can for instance be used to retrieve an HTML 5 client that uses REST-resources on this server
  server.default_resource["GET"] = [](shared_ptr<HttpServer::Response> response, shared_ptr<HttpServer::Request> request) {
    try {
      auto web_root_path = boost::filesystem::canonical("web");
      auto path = boost::filesystem::canonical(web_root_path / request->path);
      // Check if path is within web_root_path
      if(distance(web_root_path.begin(), web_root_path.end()) > distance(path.begin(), path.end()) ||
         !equal(web_root_path.begin(), web_root_path.end(), path.begin()))
        throw invalid_argument("path must be within root path");
      if(boost::filesystem::is_directory(path))
        path /= "index.html";

      SimpleWeb::CaseInsensitiveMultimap header;

      // Uncomment the following line to enable Cache-Control
      // header.emplace("Cache-Control", "max-age=86400");

#ifdef HAVE_OPENSSL
//    Uncomment the following lines to enable ETag
//    {
//      ifstream ifs(path.string(), ifstream::in | ios::binary);
//      if(ifs) {
//        auto hash = SimpleWeb::Crypto::to_hex_string(SimpleWeb::Crypto::md5(ifs));
//        header.emplace("ETag", "\"" + hash + "\"");
//        auto it = request->header.find("If-None-Match");
//        if(it != request->header.end()) {
//          if(!it->second.empty() && it->second.compare(1, hash.size(), hash) == 0) {
//            response->write(SimpleWeb::StatusCode::redirection_not_modified, header);
//            return;
//          }
//        }
//      }
//      else
//        throw invalid_argument("could not read file");
//    }
#endif

      auto ifs = make_shared<ifstream>();
      ifs->open(path.string(), ifstream::in | ios::binary | ios::ate);

      if(*ifs) {
        auto length = ifs->tellg();
        ifs->seekg(0, ios::beg);

        header.emplace("Content-Length", to_string(length));
        response->write(header);

        // Trick to define a recursive function within this scope (for example purposes)
        class FileServer {
        public:
          static void read_and_send(const shared_ptr<HttpServer::Response> &response, const shared_ptr<ifstream> &ifs) {
            // Read and send 128 KB at a time
            static vector<char> buffer(131072); // Safe when server is running on one thread
            streamsize read_length;
            if((read_length = ifs->read(&buffer[0], static_cast<streamsize>(buffer.size())).gcount()) > 0) {
              response->write(&buffer[0], read_length);
              if(read_length == static_cast<streamsize>(buffer.size())) {
                response->send([response, ifs](const SimpleWeb::error_code &ec) {
                  if(!ec)
                    read_and_send(response, ifs);
                  else
                    cerr << "Connection interrupted" << endl;
                });
              }
            }
          }
        };
        FileServer::read_and_send(response, ifs);
      }
      else
        throw invalid_argument("could not read file");
    }
    catch(const exception &e) {
      response->write(SimpleWeb::StatusCode::client_error_bad_request, "Could not open path " + request->path + ": " + e.what());
    }
  };

  server.on_error = [](shared_ptr<HttpServer::Request> /*request*/, const SimpleWeb::error_code & /*ec*/) {
    // Handle errors here
    // Note that connection timeouts will also call this handle with ec set to SimpleWeb::errc::operation_canceled
  };

  thread server_thread([&server]() {
    // Start server
    server.start();
  });

  // Wait for server to start so that the client can connect
  this_thread::sleep_for(chrono::seconds(1));

  server_thread.join();
}
