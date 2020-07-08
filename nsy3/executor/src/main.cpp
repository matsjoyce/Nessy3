#include <iostream>

#include "docopt/docopt.h"

#include "bytecode.hpp"
#include "frame.hpp"
#include "builtinfunction.hpp"
#include "builtins.hpp"
#include "executionengine.hpp"


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
    auto execengine = std::make_shared<ExecutionEngine>();
    for (auto f : files) {
        std::cout << f << std::endl;
        auto c = Code::from_file(f);
        c->print();
        auto start_env = builtins;
        start_env["__exec__"] = execengine;
        Frame frame(c, 0, start_env);
        frame.execute();
    }
    execengine->finish();

    return 0;
}
