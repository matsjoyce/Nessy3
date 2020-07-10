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
    auto execengine = ExecutionEngine();
    try {
        for (auto f : files) {
            execengine.exec_file(f);
        }
        execengine.finish();
    }
    catch (const ObjectRef& exc) {
        if (auto cast_exc = std::dynamic_pointer_cast<const Exception>(exc)) {
            std::cerr << cast_exc->to_str() << std::endl;
        }
        else {
            std::cerr << "NOT AN EXC!!! " << exc << std::endl;
        }
        return 1;
    }
    catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
        return 1;
    }
    catch (...) {
        std::cerr << "Unknown exception" << std::endl;
        return 1;
    }

    return 0;
}
