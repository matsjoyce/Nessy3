#include <iostream>
#include <fstream>

#include "docopt/docopt.h"

#include "executionengine.hpp"
#include "exception.hpp"
#include "serialisation.hpp"


static const char USAGE[] =
R"(fs

    Usage:
        executor runspec <rsfile> [--nocatch]
        executor run <files>...

    Options:
        -h --help                        Show this screen.
        --version                        Show version.
)";


int main(int argc, const char** argv) {
    auto args = docopt::docopt(USAGE, {argv + 1, argv + argc}, true, "executor 0.1");
    ObjectRef runspec;
    if (args["runspec"].asBool()) {
        if (args["<rsfile>"].asString() == "-") {
            runspec = deserialise_from_file(std::cin);
        }
        else {
            std::ifstream f(args["<rsfile>"].asString());
            runspec = deserialise_from_file(f);
        }
    }
    else {
        auto files = args["<files>"].asStringList();
//         runspec = create<Dict>({
//         {create<String>("files"), create}
//         })
//         runspec[create<String>("files"] = files;
    }
    auto execengine = ExecutionEngine();

    if (args["--nocatch"].asBool()) {
        execengine.exec_runspec(runspec);
        execengine.finish();
    }
    else {
        try {
            execengine.exec_runspec(runspec);
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
    }

    return 0;
}
