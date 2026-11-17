#include <juce_events/juce_events.h>
#include <juce_osc/juce_osc.h>

#include <chrono>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <string>
#include <thread>

namespace {

struct Options {
    int port = 17655;
    int latencyMs = 10;
    std::string out = "osc-sent.json";
};

Options parse(int argc, char** argv) {
    Options options;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--port" && i + 1 < argc)
            options.port = std::stoi(argv[++i]);
        else if (arg == "--latency-ms" && i + 1 < argc)
            options.latencyMs = std::stoi(argv[++i]);
        else if (arg == "--out" && i + 1 < argc)
            options.out = argv[++i];
    }
    return options;
}

} // namespace

int main(int argc, char** argv) {
    const Options options = parse(argc, argv);
    juce::ScopedJuceInitialiser_GUI juceInit;

    const auto sendAt = juce::Time::getCurrentTime() + juce::RelativeTime::milliseconds(options.latencyMs);
    const juce::OSCTimeTag timeTag(sendAt);
    juce::OSCBundle bundle(timeTag);
    juce::OSCMessage message("/xen/ping");
    message.addString("xenakis");
    message.addString("0.1.0-m1");
    bundle.addElement(message);

    juce::OSCSender sender;
    if (!sender.connect("127.0.0.1", options.port)) {
        std::cerr << "OSC connect failed\n";
        return 1;
    }
    // oscdump needs a moment to bind before the packet leaves.
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    if (!sender.send(bundle)) {
        std::cerr << "OSC send failed\n";
        return 1;
    }

    std::ofstream out(options.out);
    out << "{\n"
        << "  \"address\": \"/xen/ping\",\n"
        << "  \"arguments\": [\"xenakis\", \"0.1.0-m1\"],\n"
        << "  \"latency_ms\": " << options.latencyMs << ",\n"
        << "  \"time_tag_raw\": " << bundle.getTimeTag().getRawTimeTag() << ",\n"
        << "  \"time_tag_immediate\": " << (bundle.getTimeTag().isImmediately() ? "true" : "false") << ",\n"
        << "  \"port\": " << options.port << "\n"
        << "}\n";
    std::cout << "sent /xen/ping bundle to 127.0.0.1:" << options.port << '\n';
    return bundle.getTimeTag().isImmediately() ? 1 : 0;
}
