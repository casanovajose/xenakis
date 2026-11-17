#include "TransportXPlugin.h"
#include "xen/Engine.h"
#include "xen/Version.h"
#include "xen/XPluginRegistration.h"

#include <atomic>
#include <chrono>
#include <cmath>
#include <cstring>
#include <exception>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace {

std::atomic<bool> gStop{false};

#if defined(_WIN32)
BOOL WINAPI handleConsole(DWORD) {
    gStop.store(true);
    return TRUE;
}
#else
#include <csignal>
void handleSignal(int) { gStop.store(true); }
#endif

void printUsage() {
    std::cerr << "usage: xenakis version | xenakis load <file.xenproj> | xenakis run <file.xenproj> --no-audio "
                 "[--commands play;wait 1;pause;seek 2;stop] [--seconds N]\n";
}

std::string trim(std::string text) {
    while (!text.empty() && (text.front() == ' ' || text.front() == '\t'))
        text.erase(text.begin());
    while (!text.empty() && (text.back() == ' ' || text.back() == '\t' || text.back() == '\r'))
        text.pop_back();
    return text;
}

void printStatus(const char* command, const xen::transport::TransportXPlugin& transport) {
    const auto reading = transport.reading();
    std::cout << "command=" << command << " state=" << reading.state << std::fixed << std::setprecision(6)
              << " seconds=" << reading.seconds << " beats=" << reading.beats
              << " position=" << transport.formatPosition() << '\n';
    std::cout.flush();
}

void perform(xen::Engine& engine, const xen::transport::TransportXPlugin& transport, const std::string& raw) {
    const auto command = trim(raw);
    if (command.empty())
        return;
    if (command == "play") {
        engine.execute("transport.play", xen::Json::object());
    } else if (command == "pause") {
        engine.execute("transport.pause", xen::Json::object());
    } else if (command == "stop") {
        engine.execute("transport.stop", xen::Json::object());
    } else if (command == "loop") {
        engine.execute("transport.toggleLoop", xen::Json::object());
    } else if (command.rfind("seek", 0) == 0) {
        const auto time = std::stod(trim(command.substr(4)));
        engine.execute("transport.seek", xen::Json{{"time", time}});
    } else if (command.rfind("wait", 0) == 0) {
        const auto delay = std::stod(trim(command.substr(4)));
        if (delay < 0.0)
            throw std::runtime_error("wait time is negative");
        std::this_thread::sleep_for(std::chrono::duration<double>(delay));
    } else {
        throw std::runtime_error("unknown run command " + command);
    }
    engine.pump();
    const auto label = command.rfind("wait", 0) == 0 ? "wait" : command.substr(0, command.find(' '));
    printStatus(label.c_str(), transport);
}

struct RunOptions {
    std::string file;
    bool noAudio = false;
    std::string commands;
    double seconds = -1;
};

RunOptions parseRun(int argc, char** argv) {
    RunOptions options;
    for (int i = 2; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--no-audio") {
            options.noAudio = true;
        } else if (arg == "--commands" && i + 1 < argc) {
            options.commands = argv[++i];
        } else if (arg == "--seconds" && i + 1 < argc) {
            options.seconds = std::stod(argv[++i]);
        } else if (!arg.empty() && arg[0] != '-' && options.file.empty()) {
            options.file = arg;
        } else {
            throw std::runtime_error("unknown argument " + arg);
        }
    }
    return options;
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
            std::cout << "project " << engine.document().projectId() << " entities " << engine.document().entities().size()
                      << '\n';
            for (const auto& [id, version] : engine.plugins().versions())
                std::cout << id << ' ' << version << '\n';
            return 0;
        }
        if (command == "run") {
            const RunOptions options = parseRun(argc, argv);
            if (options.file.empty() || !options.noAudio) {
                printUsage();
                return 2;
            }
#if defined(_WIN32)
            SetConsoleCtrlHandler(handleConsole, TRUE);
#else
            std::signal(SIGINT, handleSignal);
            std::signal(SIGTERM, handleSignal);
#endif
            xen::Engine engine;
            engine.setPlugins(xen::createXPlugins());
            engine.startup();
            engine.loadProject(options.file);
            const auto* transport = xen::transport::TransportXPlugin::find(engine.plugins());
            if (transport == nullptr)
                throw std::runtime_error("transport is not registered");
            engine.startClock(0.005);
            if (!options.commands.empty()) {
                std::stringstream script(options.commands);
                std::string step;
                while (std::getline(script, step, ';'))
                    perform(engine, *transport, step);
            } else if (options.seconds >= 0.0) {
                perform(engine, *transport, "play");
                perform(engine, *transport, "wait " + std::to_string(options.seconds));
                perform(engine, *transport, "stop");
            } else {
                perform(engine, *transport, "play");
                while (!gStop.load()) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(20));
                    engine.pump();
                }
                perform(engine, *transport, "stop");
            }
            engine.stopClock();
            return 0;
        }
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    printUsage();
    return 2;
}
