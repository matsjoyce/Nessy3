#include <iostream>

#include "docopt/docopt.h"

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
    auto execengine = create<ExecutionEngine>();
    for (auto f : files) {
        execengine->exec_file(f);
    }
    execengine->finish();

    return 0;
}
