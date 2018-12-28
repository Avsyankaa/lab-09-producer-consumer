#include <header.hpp>
#include <deque>
#include <string>
#include <vector>
#include <gumbo.h>
#include <ThreadPool.h>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>
#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl/error.hpp>
#include <boost/asio/ssl/stream.hpp>
#include <boost/asio/ssl.hpp>
#include "sertificate.hpp"
#include <stdexcept>
#include <mutex>
#include <chrono>
#include <thread>
#include <cstdlib>
#include <iostream>
#include <fstream>
using namespace std::chrono_literals;
namespace beast = boost::beast;
namespace http = beast::http;
namespace net = boost::asio;
namespace ssl = net::ssl;
using tcp = net::ip::tcp;

std::vector<std::string> links;
std::deque<std::string> parse_queue;
std::deque<std::string> file_queue;
std::recursive_mutex download_mutex;
std::recursive_mutex parser_mutex;
std::recursive_mutex file_filling_mutex;
std::condition_variable_any cv;
std::condition_variable cv_write;
int count_ref = 0;
unsigned count_pages = 0;
unsigned first_links_lenght = 0;
std::vector<int> checks;
bool done = false;
bool notified = false;
bool find_picture (std::string href);

void download(std::string host, std::string target) {
//    if ((links.size()!=0) && (iter == links.size())) { flag = false; return;}
    std::unique_lock<std::recursive_mutex> lk(download_mutex);
    std::cout << "I am into downloading" << std::endl;
    std::cout << host <<std::endl;
    std::cout << target << std::endl;
    net::io_context ioc;
    ssl::context ctx{ssl::context::sslv23_client};
    load_root_certificates(ctx);
    ctx.set_verify_mode(ssl::verify_peer);
    tcp::resolver resolver{ioc};
    ssl::stream<tcp::socket> stream{ioc, ctx};
    auto const results = resolver.resolve(boost::asio::ip::tcp::resolver::query{host, "https"});
    net::connect(stream.next_layer(), results.begin(), results.end());
    stream.handshake(ssl::stream_base::client);
    http::request<http::string_body> req(http::verb::get, target, 11);
    //std::lock_guard<std::recursive_mutex> lk(download_mutex);
    req.set(http::field::host, host);
    req.set(http::field::user_agent, BOOST_BEAST_VERSION_STRING);
    req.set(http::field::accept, "text/html");
    req.set(http::field::connection, "close");
    http::write(stream, req);
    beast::flat_buffer buffer;
    http::response<http::string_body> res;
    http::read(stream, buffer, res);
    std::string msg = res.body();
    beast::error_code ec;
    stream.shutdown(ec);
    //download_mutex.lock();
    parse_queue.push_front(msg);
    notified = true;
    cv.notify_one();
//    flag = true;
   // download_mutex.unlock();
    count_ref --;
   std::cout << "download: " << count_ref <<  std::endl;
}


std::string convert_url_host(std::string);
std::string convert_url_target(std::string);
static void search_for_picture_links(GumboNode* node, unsigned depth);
void parse_thread_work( unsigned depth ) {
        std::unique_lock<std::recursive_mutex> lk(download_mutex);
	std::cout << "I am into parsing" << std::endl;
        while (!notified) {
        cv.wait(lk);
	std::cout << "I am waiting sv" << std::endl;
        }
        while (parse_queue.empty()) { 
	std::cout << "A queue is empty" << std::endl; 
	//parse_thread_work(depth);
	return;
	}
        if (parse_queue.front() != "") {
	GumboOutput* output = gumbo_parse(parse_queue.front().c_str());
	std::lock_guard<std::recursive_mutex> lock(file_filling_mutex);
	search_for_picture_links(output->root, depth);
	gumbo_destroy_output(&kGumboDefaultOptions, output);
        }
	parse_queue.pop_front();
	std::cout << count_ref << std::endl;
        count_pages--;
        //notified = false;
	std::cout << "I am living parsing" << std::endl;
}

static void search_for_links(GumboNode* node) {
  if (node->type != GUMBO_NODE_ELEMENT) {
    return;
  }
  GumboAttribute* href;
  if (node->v.element.tag == GUMBO_TAG_A &&
      (href = gumbo_get_attribute(&node->v.element.attributes, "href"))) {
        bool unique = true;
        for (unsigned i = 0; i < links.size(); i++) {
	if (links[i] == href->value) {
			unique = false;
                        break;
                                         }
        }
        if (unique)
	links.push_back(href->value);
  }
  GumboVector* children = &node->v.element.children;
  for (unsigned int i = 0; i < children->length; ++i) {
    search_for_links(static_cast<GumboNode*> (children->data[i]));
  }
}

static void search_for_picture_links(GumboNode* node, unsigned depth) {
  if ((node->type != GUMBO_NODE_ELEMENT) || (depth > 8)) {
    return;
  }
  GumboAttribute* src;
  if (node->v.element.tag == GUMBO_TAG_IMG &&
      (src = gumbo_get_attribute(&node->v.element.attributes, "src"))) {
	   //std::lock_guard<std::recursive_mutex> lock(file_filling_mutex);
           file_queue.push_front(src->value);
	   std::cout << "picture!!!" << src->value <<std::endl;
  }
  GumboVector* children = &node->v.element.children;
  depth++;
  for (unsigned int i = 0; i < children->length; ++i) {
    search_for_picture_links(static_cast<GumboNode*> (children->data[i]), depth);
  }
}

std::string convert_url_host (std::string url) {
	//if (url.size() < 4) throw std::runtime_error{"invalid host"};
	std::string slash = "";
	slash = slash+ '/'+'/';
	std::size_t pos = url.find(slash);
	if (pos == 0)
	url = url.substr(2);
	std::size_t pos_https = url.find("https:");
	if (pos_https == 0) url = url.substr(8);
	std::size_t pos_http = url.find("http:");
        if (pos_http == 0) url = url.substr(7);
	std::string result_host = "";
        for (unsigned i = 0; i < url.size(); i++) {
              if ((url[i] == '/') || (url[i] == '?')) break;
              result_host+=url[i];
	}
	return result_host;
}

std::string convert_url_target (std::string url) {
//if (url.size() < 4) throw std::runtime_error{"invalid target"};
       std::string slash = "";
	slash = slash + '/'+'/';
	std::size_t pos = url.find(slash);
        if (pos == 0) url = url.substr(2); 
        std::size_t pos_https = url.find("https:");
        if (pos_https == 0) url = url.substr(8);
        std::size_t pos_http = url.find("http:");
        if (pos_http == 0) url = url.substr(7);
        std::string result_target = "";
	unsigned i = 0;
	while (url[i] != '/') { ++i; }
	for (unsigned z = i; z < url.size(); z++) {
		result_target += url[z];
	}
	return result_target;
}

void make_writing(std::ofstream& out) {
if (file_queue.size() == 0) return;
std::lock_guard<std::recursive_mutex> lock(file_filling_mutex);
out << file_queue.front() << std::endl;
file_queue.pop_front();
}


int main() {
try {
        std::ofstream out;
        out.open("output.txt");
	ThreadPool pool_downloader(4);
	ThreadPool pool_parser(4);
//	pool_parser.enqueue(&parse_thread_work, 0);
	download("github.com", "/Avsyankaa/Tests");
	GumboOutput* output = gumbo_parse(parse_queue.front().c_str());
        parse_queue.pop_front();
	notified = false;
	search_for_links(output->root);
        gumbo_destroy_output(&kGumboDefaultOptions, output);
	for (unsigned z = 0; z < links.size(); z++){
	for (unsigned i = 0; i < links.size(); i++){
	if (links[i].find("https:") != 0) {
	std::swap(links[i], links[links.size()-1]);
	links.pop_back();
	}
	}
	}
	for (unsigned i = 0; i < links.size(); i++)
         std::cout << links[i] << std::endl;
        count_pages = count_ref = first_links_lenght = links.size();
	for (unsigned i = 0; i < links.size(); i++) {
	pool_downloader.enqueue(&download, convert_url_host(links[i]),
        convert_url_target(links[i]));
	pool_parser.enqueue(&parse_thread_work, 0);
	}
	//for (unsigned i = 0; i < links.size(); i++) {
	//pool_downloader.enqueue(&download, convert_url_host(links[i]),
	//convert_url_target(links[i]));  }
      //  std::this_thread::sleep_for(2s);
        //pool_parser.enqueue(&parse_thread_work, 0);
	//pool_parser.enqueue(&parse_thread_work, 0);
        //std::cout << "flag=" << flag << std::endl;
	//while (flag) {
        //pool_parser.enqueue(&parse_thread_work, 0);
        //download_mutex.lock();
        //if (count_ref < 0) flag = false;
        //download_mutex.unlock();
         //}
	//std::cout << "here" << std::endl;
     }

catch (std::exception& e) {
	e.what();
	}
}
