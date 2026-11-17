#pragma once

#include "rt/Timing.h"
#include "xen/RtChannel.h"
#include "xen/Snapshot.h"
#include "xen/contracts/PositionTap.h"
#include "xen/contracts/TempoSource.h"

#include <cstring>

namespace xen::paths::rt {

enum class NoticeKind : int { Trigger = 1, Started = 2, Pass = 3, Stopped = 4 };

struct Notice {
    NoticeKind kind = NoticeKind::Trigger;
    char triggerId[16]{};
    char cursorId[16]{};
    char pathId[16]{};
    char groupId[16]{};
    int dir = 0;
    int pass = 0;
    float u = 0;
    float x = 0;
    float y = 0;
    double time = 0;
    bool muted = false;
};

struct PlaySlot {
    char id[16]{};
    double lastTransport = 0;
    bool used = false;
    bool primed = false;
    bool started = false;
    bool stopped = false;
    int lastPass = 0;
};

struct TriggerPoint {
    char id[16]{};
    double u = 0;
    int direction = 0;
    bool enabled = true;
    bool allCursors = true;
    char cursors[8][16]{};
    int cursorCount = 0;
    int pathIndex = -1;
    bool muted = false;
};

struct CursorConfig {
    char id[16]{};
    char pathId[16]{};
    char groupId[16]{};
    int pathIndex = -1;
    CursorMath math{};
    bool armed = true;
    bool stream = false;
    bool muted = false;
};

inline void copyId(char* dest, const char* source) noexcept {
    std::size_t i = 0;
    if (source != nullptr) {
        for (; i < 15 && source[i] != '\0'; ++i)
            dest[i] = source[i];
    }
    dest[i] = '\0';
}

inline bool sameId(const char* left, const char* right) noexcept {
    return std::strcmp(left, right) == 0;
}

inline bool acceptsCursor(const TriggerPoint& trigger, const char* cursorId) noexcept {
    if (trigger.allCursors)
        return true;
    for (int i = 0; i < trigger.cursorCount; ++i) {
        if (sameId(trigger.cursors[i], cursorId))
            return true;
    }
    return false;
}

inline bool acceptsDirection(const TriggerPoint& trigger, int dir) noexcept {
    if (trigger.direction == 0)
        return true;
    return trigger.direction == dir;
}

class PathsNode final : public xen::RtObject {
public:
    PathsNode(std::vector<PathTable> paths, std::vector<CursorConfig> cursors, std::vector<TriggerPoint> triggers,
              xen::rt::TempoSource* tempo, PlaySlot* slots, int slotCount, xen::RtChannel<Notice, 2048>* notices,
              xen::RtChannel<xen::rt::CursorSample, 256>* positions)
        : paths_(std::move(paths)),
          cursors_(std::move(cursors)),
          triggers_(std::move(triggers)),
          tempo_(tempo),
          slots_(slots),
          slotCount_(slotCount),
          notices_(notices),
          positions_(positions) {}

    void tick(const xen::TickInfo&) noexcept override {
        if (tempo_ == nullptr || slots_ == nullptr)
            return;
        const xen::rt::TransportTime end = tempo_->end();
        const xen::rt::TransportTime start = tempo_->at({});
        for (const CursorConfig& cursor : cursors_) {
            if (!cursor.armed || cursor.pathIndex < 0)
                continue;
            PlaySlot* slot = slotFor(cursor.id);
            if (slot == nullptr)
                continue;
            if (end.jumped) {
                slot->lastTransport = end.seconds;
                slot->primed = true;
                const CursorMath::Place place = cursor.math.placeAt(end.seconds - cursor.math.delay);
                slot->started = place.moving || place.finished || end.seconds > cursor.math.delay;
                slot->stopped = place.finished;
                slot->lastPass = place.pass;
                continue;
            }
            if (!slot->primed) {
                slot->lastTransport = start.seconds;
                slot->primed = true;
            }
            if (!(end.seconds > slot->lastTransport)) {
                slot->lastTransport = end.seconds;
                continue;
            }
            TriggerPoint mine[64];
            int mineCount = 0;
            for (const TriggerPoint& trigger : triggers_) {
                if (!trigger.enabled || trigger.pathIndex != cursor.pathIndex || mineCount >= 64)
                    continue;
                if (!acceptsCursor(trigger, cursor.id))
                    continue;
                mine[mineCount++] = trigger;
            }
            Hit hits[512];
            const int hitCount = cursor.math.crossings(slot->lastTransport, end.seconds, mine, mineCount, hits, 512);
            const PathTable* table = cursor.pathIndex < static_cast<int>(paths_.size()) ? &paths_[cursor.pathIndex] : nullptr;
            for (int i = 0; i < hitCount; ++i) {
                const Hit& hit = hits[i];
                if (hit.started) {
                    if (!slot->started)
                        push(cursor, NoticeKind::Started, nullptr, hit, false);
                    slot->started = true;
                    continue;
                }
                if (hit.passEnded) {
                    if (hit.pass != slot->lastPass)
                        push(cursor, NoticeKind::Pass, nullptr, hit, false);
                    slot->lastPass = hit.pass;
                    continue;
                }
                if (hit.stopped) {
                    if (!slot->stopped)
                        push(cursor, NoticeKind::Stopped, nullptr, hit, false);
                    slot->stopped = true;
                    continue;
                }
                if (hit.triggerIndex < 0 || hit.triggerIndex >= mineCount)
                    continue;
                const TriggerPoint& trigger = mine[hit.triggerIndex];
                if (!trigger.enabled || !acceptsDirection(trigger, hit.dir))
                    continue;
                push(cursor, NoticeKind::Trigger, &trigger, hit, trigger.muted || cursor.muted);
            }
            if (cursor.stream && positions_ != nullptr && table != nullptr) {
                const CursorMath::Place place = cursor.math.placeAt(end.seconds - cursor.math.delay);
                const Vec2 point = table->pointAt(place.u);
                xen::rt::CursorSample sample{};
                copyId(sample.cursorId, cursor.id);
                copyId(sample.pathId, cursor.pathId);
                sample.u = static_cast<float>(place.u);
                sample.x = static_cast<float>(point.x);
                sample.y = static_cast<float>(point.y);
                sample.pass = place.pass;
                sample.time = end.seconds;
                sample.moving = place.moving;
                sample.muted = cursor.muted;
                positions_->push(sample);
            }
            slot->lastTransport = end.seconds;
        }
    }

private:
    PlaySlot* slotFor(const char* id) noexcept {
        PlaySlot* free = nullptr;
        for (int i = 0; i < slotCount_; ++i) {
            if (slots_[i].used && sameId(slots_[i].id, id))
                return &slots_[i];
            if (!slots_[i].used && free == nullptr)
                free = &slots_[i];
        }
        if (free == nullptr)
            return nullptr;
        *free = PlaySlot{};
        copyId(free->id, id);
        free->used = true;
        return free;
    }

    void push(const CursorConfig& cursor, NoticeKind kind, const TriggerPoint* trigger, const Hit& hit, bool muted) noexcept {
        if (notices_ == nullptr)
            return;
        const PathTable* table = cursor.pathIndex >= 0 && cursor.pathIndex < static_cast<int>(paths_.size())
                                     ? &paths_[static_cast<std::size_t>(cursor.pathIndex)]
                                     : nullptr;
        const Vec2 point = table != nullptr ? table->pointAt(hit.u) : Vec2{};
        Notice notice;
        notice.kind = kind;
        if (trigger != nullptr)
            copyId(notice.triggerId, trigger->id);
        copyId(notice.cursorId, cursor.id);
        copyId(notice.pathId, cursor.pathId);
        copyId(notice.groupId, cursor.groupId);
        notice.dir = hit.dir;
        notice.pass = hit.pass;
        notice.u = static_cast<float>(hit.u);
        notice.x = static_cast<float>(point.x);
        notice.y = static_cast<float>(point.y);
        notice.time = hit.time;
        notice.muted = muted || cursor.muted;
        notices_->push(notice);
    }

    std::vector<PathTable> paths_;
    std::vector<CursorConfig> cursors_;
    std::vector<TriggerPoint> triggers_;
    xen::rt::TempoSource* tempo_ = nullptr;
    PlaySlot* slots_ = nullptr;
    int slotCount_ = 0;
    xen::RtChannel<Notice, 2048>* notices_ = nullptr;
    xen::RtChannel<xen::rt::CursorSample, 256>* positions_ = nullptr;
};

}
