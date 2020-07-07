#include <iostream>

#include "docopt/docopt.h"

#include "bytecode.hpp"
#include "frame.hpp"
#include "builtinfunction.hpp"
#include "builtins.hpp"


static const char USAGE[] =
R"(fs

    Usage:
        executor <files>...

    Options:
        -h --help                        Show this screen.
        --version                        Show version.
)";


int main(int argc, const char** argv) {
    auto args = docopt::docopt(USAGE, {argv + 1, argv + argc}, true, "executor 0.1");

    auto files = args["<files>"].asStringList();
    for (auto f : files) {
        std::cout << f << std::endl;
        auto c = Code::from_file(f);
        c->print();
        Frame frame(c);
        for (auto item : builtins) {
            frame.set_env(item.first, item.second);
        }
        frame.execute();
    }

    return 0;
}
