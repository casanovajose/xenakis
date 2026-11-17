#include "xen/ControlClock.h"
#include "xen/Json.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace {

struct Options {
    double seconds = 8.0;
    double periodMs = 5.0;
    std::string out;
    bool strict = false;
    int warmup = 20;
};

Options parse(int argc, char** argv) {
    Options options;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        const auto need = [&](const char* name) {
            if (i + 1 >= argc)
                throw std::runtime_error(std::string("missing value for ") + name);
            return std::string(argv[++i]);
        };
        if (arg == "--seconds")
            options.seconds = std::stod(need("--seconds"));
        else if (arg == "--period-ms")
            options.periodMs = std::stod(need("--period-ms"));
        else if (arg == "--out")
            options.out = need("--out");
        else if (arg == "--warmup")
            options.warmup = std::stoi(need("--warmup"));
        else if (arg == "--strict")
            options.strict = true;
        else
            throw std::runtime_error("unknown argument " + arg);
    }
    return options;
}

double percentile(std::vector<double> values, double percent) {
    if (values.empty())
        return 0;
    std::sort(values.begin(), values.end());
    const double rank = (percent / 100.0) * static_cast<double>(values.size() - 1);
    const auto low = static_cast<std::size_t>(std::floor(rank));
    const auto high = std::min(low + 1, values.size() - 1);
    const double fraction = rank - static_cast<double>(low);
    return values[low] * (1.0 - fraction) + values[high] * fraction;
}

struct Probe final : xen::ControlClock::Target {
    std::vector<double> stampsUs;
    std::chrono::steady_clock::time_point origin{};
    bool started = false;

    Probe() { stampsUs.reserve(200000); }

    void onTick(const xen::TickInfo&) noexcept override {
        const auto now = std::chrono::steady_clock::now();
        if (!started) {
            origin = now;
            started = true;
        }
        if (stampsUs.size() == stampsUs.capacity())
            return;
        const auto us = std::chrono::duration<double, std::micro>(now - origin).count();
        stampsUs.push_back(us);
    }
};

} // namespace

int main(int argc, char** argv) {
    try {
        const Options options = parse(argc, argv);
        Probe probe;
        xen::ControlClock clock(probe);
        clock.start(options.periodMs / 1000.0);
        std::this_thread::sleep_for(std::chrono::duration<double>(options.seconds));
        clock.stop();

        std::vector<double> intervals;
        for (std::size_t i = 1; i < probe.stampsUs.size(); ++i)
            intervals.push_back(probe.stampsUs[i] - probe.stampsUs[i - 1]);

        const auto slice = [&](int skip) {
            if (static_cast<int>(intervals.size()) <= skip)
                return intervals;
            return std::vector<double>(intervals.begin() + skip, intervals.end());
        };
        const auto steady = slice(std::max(0, options.warmup));
        const double periodUs = options.periodMs * 1000.0;
        std::vector<double> errorUs;
        errorUs.reserve(steady.size());
        for (double interval : steady)
            errorUs.push_back(std::abs(interval - periodUs));

        const double p50 = percentile(errorUs, 50);
        const double p99 = percentile(errorUs, 99);
        const double worst = errorUs.empty() ? 0 : *std::max_element(errorUs.begin(), errorUs.end());
        const bool pass = !errorUs.empty() && p99 < 1000.0;

        xen::Json result = {
            {"backend", xen::preciseSleepBackend()},
            {"period_ms", options.periodMs},
            {"seconds", options.seconds},
            {"ticks", clock.ticks()},
            {"intervals", intervals.size()},
            {"warmup_intervals_skipped", options.warmup},
            {"steady_intervals", errorUs.size()},
            {"p50_abs_interval_error_us", p50},
            {"p99_abs_interval_error_us", p99},
            {"max_abs_interval_error_us", worst},
            {"p99_under_1ms", pass},
        };
        std::cout << result.dump(2) << '\n';
        if (!options.out.empty()) {
            std::ofstream file(options.out);
            file << result.dump(2) << '\n';
        }
        if (options.strict && !pass)
            return 1;
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 2;
    }
}
