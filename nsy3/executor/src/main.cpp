#include <iostream>

#include "docopt/docopt.h"

#include "executionengine.hpp"
#include "exception.hpp"


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
    try {
        for (auto f : files) {
            execengine->exec_file(f);
        }
        execengine->finish();
    }
    catch (ObjectRef exc) {
        if (auto cast_exc = std::dynamic_pointer_cast<Exception>(exc)) {
            std::cerr << cast_exc->to_str() << std::endl;
        }
        else {
            std::cerr << "NOT AN EXC!!! " << exc << std::endl;
        }
    }
    catch (std::exception e) {
        std::cerr << e.what() << std::endl;
    }
    catch (...) {
        std::cerr << "Unknown exception" << std::endl;
    }

    return 0;
}
