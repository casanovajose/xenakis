#include <juce_gui_basics/juce_gui_basics.h>

#include <algorithm>
#include <chrono>
#include <fstream>
#include <vector>

namespace {

struct Curve {
    float x0, y0, c1x, c1y, c2x, c2y, x1, y1;
};

std::vector<Curve> makeCurves() {
    std::vector<Curve> curves;
    curves.reserve(50);
    for (int i = 0; i < 50; ++i) {
        const float y = 40.0f + static_cast<float>(i) * 13.0f;
        curves.push_back(Curve{
            40.0f,
            y,
            320.0f,
            y - 80.0f + static_cast<float>(i % 5) * 20.0f,
            760.0f,
            y + 70.0f,
            1200.0f,
            y + static_cast<float>((i % 7) - 3) * 6.0f,
        });
    }
    return curves;
}

void drawCurves(juce::Graphics& graphics, juce::Rectangle<float> area, const std::vector<Curve>& curves) {
    graphics.fillAll(juce::Colour(0xff141518));
    graphics.setColour(juce::Colour(0xffc8c8c8));
    const float sx = area.getWidth() / 1280.0f;
    const float sy = area.getHeight() / 720.0f;
    for (const auto& curve : curves) {
        juce::Path path;
        path.startNewSubPath(curve.x0 * sx, curve.y0 * sy);
        path.cubicTo(curve.c1x * sx, curve.c1y * sy, curve.c2x * sx, curve.c2y * sy, curve.x1 * sx, curve.y1 * sy);
        graphics.strokePath(path, juce::PathStrokeType(1.5f));
    }
}

struct PaintStats {
    int frames = 0;
    double p50Us = 0;
    double p99Us = 0;
    double maxUs = 0;
};

double percentile(std::vector<double> values, double percent) {
    if (values.empty())
        return 0;
    std::sort(values.begin(), values.end());
    const double rank = (percent / 100.0) * static_cast<double>(values.size() - 1);
    const auto low = static_cast<std::size_t>(rank);
    const auto high = std::min(low + 1, values.size() - 1);
    const double fraction = rank - static_cast<double>(low);
    return values[low] * (1.0 - fraction) + values[high] * fraction;
}

PaintStats timeOffscreen(const std::vector<Curve>& curves, int frames) {
    juce::Image image(juce::Image::ARGB, 1280, 720, true);
    std::vector<double> samples;
    samples.reserve(static_cast<std::size_t>(frames));
    for (int i = 0; i < frames; ++i) {
        const auto start = std::chrono::steady_clock::now();
        juce::Graphics graphics(image);
        drawCurves(graphics, image.getBounds().toFloat(), curves);
        const auto us = std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - start).count();
        samples.push_back(us);
    }
    PaintStats stats;
    stats.frames = frames;
    stats.p50Us = percentile(samples, 50);
    stats.p99Us = percentile(samples, 99);
    stats.maxUs = *std::max_element(samples.begin(), samples.end());
    return stats;
}

class CanvasComponent final : public juce::Component {
public:
    CanvasComponent() : curves_(makeCurves()) {
        setOpaque(true);
        vblank_ = std::make_unique<juce::VBlankAttachment>(this, [this] { onFrame(); });
    }

    void paint(juce::Graphics& graphics) override {
        drawCurves(graphics, getLocalBounds().toFloat(), curves_);
    }

    int frames() const { return frames_; }

private:
    void onFrame() {
        const auto now = std::chrono::steady_clock::now();
        if (frames_ > 0)
            intervalsUs_.push_back(std::chrono::duration<double, std::micro>(now - last_).count());
        last_ = now;
        ++frames_;
        repaint();
    }

public:
    PaintStats intervals() const {
        PaintStats stats;
        stats.frames = static_cast<int>(intervalsUs_.size());
        stats.p50Us = percentile(intervalsUs_, 50);
        stats.p99Us = percentile(intervalsUs_, 99);
        stats.maxUs = intervalsUs_.empty() ? 0 : *std::max_element(intervalsUs_.begin(), intervalsUs_.end());
        return stats;
    }

private:
    std::vector<Curve> curves_;
    std::unique_ptr<juce::VBlankAttachment> vblank_;
    std::chrono::steady_clock::time_point last_{};
    std::vector<double> intervalsUs_;
    int frames_ = 0;
};

class CanvasWindow final : public juce::DocumentWindow {
public:
    CanvasWindow()
        : DocumentWindow("Xenakis canvas spike", juce::Colour(0xff141518), DocumentWindow::closeButton) {
        setUsingNativeTitleBar(true);
        setContentOwned(new CanvasComponent(), true);
        centreWithSize(1280, 720);
        setVisible(true);
    }

    CanvasComponent& canvas() { return *static_cast<CanvasComponent*>(getContentComponent()); }
    void closeButtonPressed() override { juce::JUCEApplication::getInstance()->systemRequestedQuit(); }
};

juce::String argumentValue(const juce::String& parameters, const juce::String& name, const juce::String& fallback) {
    const auto parts = juce::StringArray::fromTokens(parameters, true);
    for (int i = 0; i < parts.size() - 1; ++i) {
        if (parts[i] == name)
            return parts[i + 1];
    }
    return fallback;
}

} // namespace

class CanvasApplication final : public juce::JUCEApplication, private juce::Timer {
public:
    const juce::String getApplicationName() override { return "Xenakis canvas spike"; }
    const juce::String getApplicationVersion() override { return "0.1.0-m1"; }
    bool moreThanOneInstanceAllowed() override { return true; }

    void initialise(const juce::String& parameters) override {
        outPath_ = argumentValue(parameters, "--out", "canvas-result.json");
        offscreenOnly_ = parameters.contains("--offscreen-only");
        curves_ = makeCurves();
        offscreen_ = timeOffscreen(curves_, 180);
        if (offscreenOnly_) {
            writeAndQuit(PaintStats{});
            return;
        }
        window_ = std::make_unique<CanvasWindow>();
        startTimer(16);
    }

    void shutdown() override { window_.reset(); }

    void timerCallback() override {
        if (window_ == nullptr)
            return;
        if (window_->canvas().frames() >= 180)
            writeAndQuit(window_->canvas().intervals());
    }

private:
    void writeAndQuit(const PaintStats& presented) {
        const double offscreenFps = offscreen_.p99Us > 0 ? 1.0e6 / offscreen_.p99Us : 0;
        const double presentedFps = presented.p50Us > 0 ? 1.0e6 / presented.p50Us : 0;
        const bool pass = offscreenFps >= 60.0 && (offscreenOnly_ || presentedFps >= 55.0);
        std::ofstream out(outPath_.toStdString());
        out << "{\n"
            << "  \"paths\": 50,\n"
            << "  \"offscreen_frames\": " << offscreen_.frames << ",\n"
            << "  \"offscreen_paint_p50_us\": " << offscreen_.p50Us << ",\n"
            << "  \"offscreen_paint_p99_us\": " << offscreen_.p99Us << ",\n"
            << "  \"offscreen_paint_max_us\": " << offscreen_.maxUs << ",\n"
            << "  \"offscreen_fps_from_p99\": " << offscreenFps << ",\n"
            << "  \"presented_intervals\": " << presented.frames << ",\n"
            << "  \"presented_interval_p50_us\": " << presented.p50Us << ",\n"
            << "  \"presented_interval_p99_us\": " << presented.p99Us << ",\n"
            << "  \"presented_fps_from_p50\": " << presentedFps << ",\n"
            << "  \"offscreen_only\": " << (offscreenOnly_ ? "true" : "false") << ",\n"
            << "  \"pass\": " << (pass ? "true" : "false") << "\n"
            << "}\n";
        quit();
    }

    juce::String outPath_;
    bool offscreenOnly_ = false;
    std::vector<Curve> curves_;
    PaintStats offscreen_;
    std::unique_ptr<CanvasWindow> window_;
};

START_JUCE_APPLICATION(CanvasApplication)
