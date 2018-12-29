#include <krauler.hpp>

#include <boost/program_options/cmdline.hpp>
#include <boost/program_options/config.hpp>
#include <boost/program_options/environment_iterator.hpp>
#include <boost/program_options/eof_iterator.hpp>
#include <boost/program_options/errors.hpp>
#include <boost/program_options/option.hpp>
#include <boost/program_options/options_description.hpp>
#include <boost/program_options/parsers.hpp>
#include <boost/program_options/positional_options.hpp>
#include <boost/program_options/value_semantic.hpp>
#include <boost/program_options/variables_map.hpp>
#include <boost/program_options/version.hpp>


namespace po = boost::program_options;

int main(int argc, char* argv[]) {
    unsigned depth = 8;
    std::string url = "github.com/Avsyankaa/Tests";
    std::string output = "Pictures_link.log";
    unsigned network_threads = 4;
    unsigned parser_threads = 4;
    po::options_description desc("Allowed options");
    desc.add_options()("help", "produce help message")(
        "depth", po::value<unsigned>(&depth), "set compression level")(
            "url", po::value<std::string>(&url), "URL start page")(
                "network_threads", po::value<unsigned>(&network_threads), "")(
                    "parser_threads", po::value<unsigned>(&parser_threads), "")(
                        "output", po::value<std::string>(&output), "");
    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, desc), vm);
    po::notify(vm);
    krauler k(depth, network_threads, parser_threads, output, url);
    k.make_krauling();
}
