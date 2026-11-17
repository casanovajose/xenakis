#pragma once

#include "xen/RtChannel.h"
#include "xen/contracts/TempoMath.h"
#include "xen/contracts/TempoSource.h"

#include <chrono>
#include <cstdint>

namespace xen::transport::rt {

enum class RunState : int { Stopped = 0, Playing = 1, Paused = 2 };

enum class NoticeKind : int { Looped = 1, Beat = 2 };

struct Notice {
    NoticeKind kind = NoticeKind::Looped;
    double seconds = 0;
    double beats = 0;
    int bar = 1;
    int beat = 1;
    int wraps = 1;
};

struct Settings {
    double bpm = 120;
    double gridUnit = 1;
    double loopIn = 0;
    double loopOut = 4;
    double position = 0;
    double absoluteUnitSeconds = 1;
    bool loop = false;
    bool bpmMode = true;
    bool jumped = false;
    int state = 0;
    int epoch = 0;
    int beatsPerBar = 4;
};

struct Readout {
    double seconds = 0;
    double beats = 0;
    double bpm = 120;
    double gridUnitSeconds = 0.5;
    double loopIn = 0;
    double loopOut = 4;
    int state = 0;
    int beatsPerBar = 4;
    bool loop = false;
    bool bpmMode = true;
    bool playing = false;
};

inline double wrapSeconds(double seconds, double loopIn, double loopOut, int& wraps) noexcept {
    wraps = 0;
    if (!(loopOut > loopIn) || !(seconds >= loopOut))
        return seconds;
    const double span = loopOut - loopIn;
    const double relative = seconds - loopIn;
    wraps = static_cast<int>(relative / span);
    if (wraps < 1)
        wraps = 1;
    double wrapped = loopIn + (relative - static_cast<double>(wraps) * span);
    if (!(wrapped < loopOut))
        wrapped = loopIn;
    if (wrapped < loopIn)
        wrapped = loopIn;
    return wrapped;
}

// Shared by the plugin and every compiled node. The seqlock keeps the message
// thread and the clock thread off each other's writes. Tick never waits.
class TransportClock {
public:
    Readout read() const noexcept {
        for (int attempt = 0; attempt < 8; ++attempt) {
            const auto sequence = seq_.load(std::memory_order_acquire);
            if ((sequence & 1u) != 0u)
                continue;
            Readout copy;
            copy.seconds = seconds_;
            copy.beats = beats_;
            copy.bpm = bpm_;
            copy.gridUnitSeconds = gridUnitSeconds_;
            copy.loopIn = loopIn_;
            copy.loopOut = loopOut_;
            copy.state = state_;
            copy.beatsPerBar = beatsPerBar_;
            copy.loop = loop_;
            copy.bpmMode = bpmMode_;
            copy.playing = state_ == static_cast<int>(RunState::Playing);
            const auto stamp = stamp_;
            if (seq_.load(std::memory_order_acquire) != sequence)
                continue;
            if (copy.playing) {
                const auto now = std::chrono::steady_clock::now();
                copy.seconds += std::chrono::duration<double>(now - stamp).count();
                int wraps = 0;
                if (copy.loop)
                    copy.seconds = wrapSeconds(copy.seconds, copy.loopIn, copy.loopOut, wraps);
                copy.beats = contracts::secondsToBeats(copy.seconds, copy.bpm);
            }
            return copy;
        }
        return {};
    }

    void invalidate() noexcept {
        if (!lock())
            return;
        forceSnap_ = true;
        unlock();
    }

    void apply(const Settings& settings) noexcept {
        if (!lock())
            return;
        bpm_ = settings.bpm > 0.0 ? settings.bpm : 120.0;
        loop_ = settings.loop;
        loopIn_ = settings.loopIn;
        loopOut_ = settings.loopOut;
        bpmMode_ = settings.bpmMode;
        beatsPerBar_ = settings.beatsPerBar > 0 ? settings.beatsPerBar : 4;
        gridUnitSeconds_ = contracts::gridUnitToSeconds(
            bpmMode_, settings.gridUnit, bpm_, settings.absoluteUnitSeconds);
        if (forceSnap_ || settings.epoch != appliedEpoch_) {
            seconds_ = settings.position;
            beats_ = contracts::secondsToBeats(seconds_, bpm_);
            state_ = settings.state;
            stamp_ = std::chrono::steady_clock::now();
            appliedEpoch_ = settings.epoch;
            pendingJump_ = settings.jumped;
            resetBeat_ = true;
            forceSnap_ = false;
        }
        unlock();
    }

    void tick() noexcept {
        double seconds = 0;
        double bpm = 120;
        double loopIn = 0;
        double loopOut = 0;
        int state = 0;
        bool loop = false;
        bool bpmMode = true;
        bool pendingJump = false;
        bool resetBeat = false;
        int beatsPerBar = 4;
        std::chrono::steady_clock::time_point stamp{};
        std::uint32_t sequence = 0;
        if (!loadRaw(seconds, bpm, loopIn, loopOut, state, loop, bpmMode, pendingJump, resetBeat, beatsPerBar, stamp,
                     sequence)) {
            return;
        }

        const auto startSeconds = seconds;
        const auto startBeats = contracts::secondsToBeats(startSeconds, bpm);
        const bool playing = state == static_cast<int>(RunState::Playing);

        if (pendingJump || resetBeat) {
            if (!commit(sequence, seconds, contracts::secondsToBeats(seconds, bpm), state, stamp, false, false))
                return;
            publishBlock(startSeconds, startBeats, playing, pendingJump, seconds, contracts::secondsToBeats(seconds, bpm),
                         playing, pendingJump);
            if (resetBeat)
                armBeat(contracts::secondsToBeats(seconds, bpm));
            return;
        }

        if (!playing) {
            publishBlock(startSeconds, startBeats, false, false, startSeconds, startBeats, false, false);
            return;
        }

        const auto now = std::chrono::steady_clock::now();
        double next = seconds + std::chrono::duration<double>(now - stamp).count();
        int wraps = 0;
        if (loop)
            next = wrapSeconds(next, loopIn, loopOut, wraps);
        const double nextBeats = contracts::secondsToBeats(next, bpm);
        if (!commit(sequence, next, nextBeats, state, now, false, false))
            return;
        publishBlock(startSeconds, startBeats, true, false, next, nextBeats, true, false);
        if (wraps > 0) {
            Notice notice;
            notice.kind = NoticeKind::Looped;
            notice.seconds = next;
            notice.beats = nextBeats;
            notice.wraps = wraps;
            notices_.push(notice);
        }
        if (bpmMode)
            emitBeats(startBeats, nextBeats, next, beatsPerBar);
    }

    bool pop(Notice& notice) noexcept { return notices_.pop(notice); }

    xen::rt::TransportTime blockStart() const noexcept { return blockStart_; }
    xen::rt::TransportTime blockEnd() const noexcept { return blockEnd_; }
    double gridUnitSeconds() const noexcept { return gridUnitSeconds_; }
    double bpm() const noexcept { return bpm_; }

    double wallUnixAt(double transportSeconds) const noexcept {
        const Readout readout = read();
        const double now = std::chrono::duration<double>(std::chrono::system_clock::now().time_since_epoch()).count();
        return now + (transportSeconds - readout.seconds);
    }

private:
    bool lock() noexcept {
        for (int attempt = 0; attempt < 64; ++attempt) {
            auto sequence = seq_.load(std::memory_order_acquire);
            if ((sequence & 1u) != 0u)
                continue;
            if (seq_.compare_exchange_weak(sequence, sequence + 1u, std::memory_order_acq_rel))
                return true;
        }
        return false;
    }

    void unlock() noexcept {
        seq_.fetch_add(1u, std::memory_order_release);
    }

    bool loadRaw(double& seconds, double& bpm, double& loopIn, double& loopOut, int& state, bool& loop, bool& bpmMode,
                 bool& pendingJump, bool& resetBeat, int& beatsPerBar, std::chrono::steady_clock::time_point& stamp,
                 std::uint32_t& sequence) const noexcept {
        for (int attempt = 0; attempt < 8; ++attempt) {
            sequence = seq_.load(std::memory_order_acquire);
            if ((sequence & 1u) != 0u)
                continue;
            seconds = seconds_;
            bpm = bpm_;
            loopIn = loopIn_;
            loopOut = loopOut_;
            state = state_;
            loop = loop_;
            bpmMode = bpmMode_;
            pendingJump = pendingJump_;
            resetBeat = resetBeat_;
            beatsPerBar = beatsPerBar_;
            stamp = stamp_;
            if (seq_.load(std::memory_order_acquire) == sequence)
                return true;
        }
        return false;
    }

    bool commit(std::uint32_t sequence, double seconds, double beats, int state,
                std::chrono::steady_clock::time_point stamp, bool pendingJump, bool resetBeat) noexcept {
        auto expected = sequence;
        if ((expected & 1u) != 0u || !seq_.compare_exchange_strong(expected, expected + 1u, std::memory_order_acq_rel))
            return false;
        seconds_ = seconds;
        beats_ = beats;
        state_ = state;
        stamp_ = stamp;
        pendingJump_ = pendingJump;
        resetBeat_ = resetBeat;
        seq_.store(expected + 2u, std::memory_order_release);
        return true;
    }

    void publishBlock(double startSeconds, double startBeats, bool startPlaying, bool startJumped, double endSeconds,
                      double endBeats, bool endPlaying, bool endJumped) noexcept {
        blockStart_.seconds = startSeconds;
        blockStart_.beats = startBeats;
        blockStart_.playing = startPlaying;
        blockStart_.jumped = startJumped;
        blockEnd_.seconds = endSeconds;
        blockEnd_.beats = endBeats;
        blockEnd_.playing = endPlaying;
        blockEnd_.jumped = endJumped;
    }

    void armBeat(double beats) noexcept {
        const int index = beatIndex(beats);
        if (onBeatBoundary(beats))
            lastBeat_ = index - 1;
        else
            lastBeat_ = index;
        beatArmed_ = true;
    }

    void emitBeats(double startBeats, double endBeats, double endSeconds, int beatsPerBar) noexcept {
        if (!beatArmed_) {
            armBeat(startBeats);
            return;
        }
        int index = beatIndex(endBeats);
        if (endBeats + 1e-9 < startBeats) {
            lastBeat_ = index;
            return;
        }
        int guard = 0;
        while (lastBeat_ < index && guard < 8) {
            ++lastBeat_;
            Notice notice;
            notice.kind = NoticeKind::Beat;
            notice.seconds = endSeconds;
            notice.beats = endBeats;
            notice.bar = lastBeat_ / beatsPerBar + 1;
            notice.beat = lastBeat_ % beatsPerBar + 1;
            notices_.push(notice);
            ++guard;
        }
        lastBeat_ = index;
    }

    static int beatIndex(double beats) noexcept {
        if (beats <= 0.0)
            return 0;
        return static_cast<int>(beats + 1e-9);
    }

    static bool onBeatBoundary(double beats) noexcept {
        const double whole = static_cast<double>(beatIndex(beats));
        const double fraction = beats - whole;
        return fraction < 1e-6 || fraction > 1.0 - 1e-6;
    }

    std::atomic<std::uint32_t> seq_{0};
    double seconds_ = 0;
    double beats_ = 0;
    double bpm_ = 120;
    double gridUnitSeconds_ = 0.5;
    double loopIn_ = 0;
    double loopOut_ = 4;
    int state_ = 0;
    int appliedEpoch_ = -1;
    int beatsPerBar_ = 4;
    int lastBeat_ = -1;
    bool loop_ = false;
    bool bpmMode_ = true;
    bool pendingJump_ = false;
    bool resetBeat_ = true;
    bool forceSnap_ = true;
    bool beatArmed_ = false;
    std::chrono::steady_clock::time_point stamp_{};
    xen::rt::TransportTime blockStart_{};
    xen::rt::TransportTime blockEnd_{};
    xen::RtChannel<Notice, 128> notices_{};
};

class TransportNode final : public xen::RtObject, public xen::rt::TempoSource {
public:
    explicit TransportNode(TransportClock* clock) : clock_(clock) {}

    void tick(const xen::TickInfo&) noexcept override {
        if (clock_ != nullptr)
            clock_->tick();
    }

    xen::rt::TransportTime at(xen::rt::RtBlock) const noexcept override {
        return clock_ != nullptr ? clock_->blockStart() : xen::rt::TransportTime{};
    }

    xen::rt::TransportTime end() const noexcept override {
        return clock_ != nullptr ? clock_->blockEnd() : xen::rt::TransportTime{};
    }

    double gridUnitSeconds() const noexcept override {
        return clock_ != nullptr ? clock_->gridUnitSeconds() : 0.0;
    }

    double beatsToSeconds(double beats) const noexcept override {
        return clock_ != nullptr ? contracts::beatsToSeconds(beats, clock_->bpm()) : 0.0;
    }

    double wallUnixAt(double transportSeconds) const noexcept override {
        return clock_ != nullptr ? clock_->wallUnixAt(transportSeconds) : 0.0;
    }

private:
    TransportClock* clock_ = nullptr;
};

}
