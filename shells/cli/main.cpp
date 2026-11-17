#include "xen/Engine.h"
#include "xen/Version.h"
#include "xen/XPluginRegistration.h"

#include <exception>
#include <iostream>
#include <string>

namespace {

void printUsage() {
    std::cerr << "usage: xenakis version | xenakis load <file.xenproj>\n";
}

} // namespace

int main(int argc, char** argv) {
    try {
        const std::string command = argc >= 2 ? argv[1] : "version";
        if (command == "version" || command == "--version") {
            std::cout << "xenakis " << xen::kVersion << '\n';
            return 0;
        }
        if (command == "load") {
            if (argc < 3) {
                printUsage();
                return 2;
            }
            xen::Engine engine;
            engine.setPlugins(xen::createXPlugins());
            engine.startup();
            engine.loadProject(argv[2]);
            std::cout << "project " << engine.document().projectId()
                      << " entities " << engine.document().entities().size() << '\n';
            for (const auto& [id, version] : engine.plugins().versions())
                std::cout << id << ' ' << version << '\n';
            return 0;
        }
        if (command == "run") {
            std::cerr << "xenakis run arrives with transport in M2\n";
            return 2;
        }
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    printUsage();
    return 2;
}
