// Copyright 2018 Avsyankaa <Avsyankaa@gmail.com>

#ifndef INCLUDE_KRAULER_HPP_
#define INCLUDE_KRAULER_HPP_
#include <deque>
#include <string>
#include <vector>
#include <gumbo.h>
#include <mutex>
#include <condition_variable>

class krauler {

private:
    std::vector<std::string> links;
    std::deque<std::string> parse_queue;
    std::deque<std::string> file_queue;
    std::recursive_mutex download_mutex;
    std::recursive_mutex file_filling_mutex;
    std::condition_variable_any cv;
    std::condition_variable_any cv_write;
    unsigned depth_;
    std::string url_;
    std::string output_;
    unsigned network_threads_;
    unsigned parser_threads_;
    int count_ref;
    unsigned count_pages;
    bool done;
    bool notified;
    void download(std::string host, std::string target);
    std::string convert_url_host(std::string);
    std::string convert_url_target(std::string);
    void search_for_picture_links(GumboNode* node, unsigned depth);
    void parse_thread_work( unsigned depth );
    void search_for_links(GumboNode* node);
    void make_writing();

public:
    krauler(unsigned depth, unsigned network_threads,
            unsigned parser_threads, std::string output, std::string url):
        depth_(depth),
        url_(url),
        output_(output),
        network_threads_(network_threads),
        parser_threads_(parser_threads),
        count_ref(0),
        count_pages(0),
        done(false),
        notified(false) {}
    void make_krauling();
};





#endif // INCLUDE_KRAULER_HPP_
