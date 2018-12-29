#include <krauler.hpp>
#include <ThreadPool.h>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>
#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl/error.hpp>
#include <boost/asio/ssl/stream.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>
#include <boost/log/sources/severity_logger.hpp>
#include <boost/log/sources/record_ostream.hpp>
#include <boost/log/core.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/sinks/sync_frontend.hpp>
#include <boost/log/sinks/text_file_backend.hpp>
#include <boost/log/sinks/text_ostream_backend.hpp>
#include <boost/log/sources/logger.hpp>
#include <boost/smart_ptr/make_shared_object.hpp>
#include <boost/smart_ptr/shared_ptr.hpp>
#include <boost/log/utility/setup/file.hpp>
#include "sertificate.hpp"
#include <stdexcept>
#include <chrono>
#include <thread>
#include <cstdlib>
#include <iostream>
using namespace std::chrono_literals;
namespace beast = boost::beast;
namespace http = beast::http;
namespace net = boost::asio;
namespace ssl = net::ssl;
using tcp = net::ip::tcp;
namespace logging = boost::log;
void krauler::download(std::string host, std::string target) {
    try {
        net::io_context ioc;
        ssl::context ctx{ssl::context::sslv23_client};
        load_root_certificates(ctx);
        ctx.set_verify_mode(ssl::verify_peer);
        tcp::resolver resolver{ioc};
        ssl::stream<tcp::socket> stream{ioc, ctx};
        auto const results = resolver.resolve(
                                 boost::asio::ip::tcp::resolver::query{host, "https"});
        net::connect(stream.next_layer(), results.begin(), results.end());
        stream.handshake(ssl::stream_base::client);
        http::request<http::string_body> req(http::verb::get, target, 11);
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
        parse_queue.push_front(msg);
    }
    catch (std::exception& e) {
        e.what();
    }
    notified = true;
    if (count_ref != -1)
        cv.notify_one();
}


void krauler::parse_thread_work( unsigned depth ) {
    std::unique_lock<std::recursive_mutex> lk(download_mutex);
    count_pages--;
    cv.wait(lk, [this]() {
        if ((this->notified) || (this->count_ref == 0)) return true;
        return false;
    });
    while (parse_queue.empty()) {
        done = true;
        notified = false;
        cv_write.notify_one();
        return;
    }
    if (parse_queue.front() != "") {
        GumboOutput* output = gumbo_parse(parse_queue.front().c_str());
        std::unique_lock<std::recursive_mutex> lock(file_filling_mutex);
        search_for_picture_links(output->root, depth);
        gumbo_destroy_output(&kGumboDefaultOptions, output);
    }
    parse_queue.pop_front();
    notified = false;
    done = true;
    cv_write.notify_one();
}

void krauler::search_for_links(GumboNode* node) {
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

void krauler::search_for_picture_links(GumboNode* node, unsigned depth) {
    if ((node->type != GUMBO_NODE_ELEMENT) || (depth == 0)) {
        return;
    }
    GumboAttribute* src;
    if (node->v.element.tag == GUMBO_TAG_IMG &&
            (src = gumbo_get_attribute(&node->v.element.attributes, "src"))) {
        file_queue.push_front(src->value);
    }
    GumboVector* children = &node->v.element.children;
    depth--;
    for (unsigned int i = 0; i < children->length; ++i) {
        search_for_picture_links(static_cast<GumboNode*> (children->data[i]), depth);
    }
}

std::string krauler::convert_url_host (std::string url) {
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

std::string krauler::convert_url_target (std::string url) {
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
    while (url[i] != '/') {
        ++i;
    }
    for (unsigned z = i; z < url.size(); z++) {
        result_target += url[z];
    }
    return result_target;
}

void krauler::make_writing() {
    std::unique_lock<std::recursive_mutex> lk(file_filling_mutex);
    while ((!done) && (count_pages != 0)) {
        cv_write.wait(lk);
    }
    done = false;
    if (file_queue.size() == 0) {
        std::cout << "good bue" << std::endl;
        return;
    }
    while (file_queue.size()!= 0) {
        BOOST_LOG_TRIVIAL(trace) << file_queue.front() << std::endl;
        std::cout << "[trace]" << file_queue.front() << std::endl;
        file_queue.pop_front();
    }
}


void krauler::make_krauling() {
    try {
        ThreadPool pool_downloader(network_threads_);
        ThreadPool pool_parser(parser_threads_);
        ThreadPool pool_writer(1);
        logging::add_file_log(output_);
        download(convert_url_host(url_), convert_url_target(url_));
        GumboOutput* output = gumbo_parse(parse_queue.front().c_str());
        parse_queue.pop_front();
        notified = false;
        search_for_links(output->root);
        gumbo_destroy_output(&kGumboDefaultOptions, output);
        for (unsigned z = 0; z < links.size(); z++) {
            for (unsigned i = 0; i < links.size(); i++) {
                if (links[i].find("https:") != 0) {
                    std::swap(links[i], links[links.size()-1]);
                    links.pop_back();
                }
            }
        }
        count_ref = count_pages = links.size();
        for (unsigned i = 0; i < links.size(); i++) {
            pool_downloader.enqueue(&krauler::download, this, convert_url_host(links[i]),
                                    convert_url_target(links[i]));
            pool_parser.enqueue(&krauler::parse_thread_work, this, depth_);
            pool_writer.enqueue(&krauler::make_writing, this);
        }
        return;
    }

    catch (std::exception& e) {
        e.what();
    }
}
