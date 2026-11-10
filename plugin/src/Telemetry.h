// Lock-free audio -> GUI telemetry: an SPSC FIFO of meter frames and a triple
// buffer holding the latest Foresight Lens snapshot.
#pragma once

#include <juce_core/juce_core.h>
#include <array>
#include <atomic>
#include "td1/Kernel.h"

namespace td1 {

class Telemetry final : public MeterSink {
public:
    static constexpr int kFifoSize = 8192;

    // ---- audio thread
    void pushFrame(const MeterFrame& f) override {
        const auto scope = fifo_.write(1);
        if (scope.blockSize1 > 0) frames_[(size_t)scope.startIndex1] = f;
        else if (scope.blockSize2 > 0) frames_[(size_t)scope.startIndex2] = f;
        // full -> drop (GUI closed or stalled)
    }
    void pushLens(const LensSnapshot& s) override {
        const int back = back_;
        lens_[(size_t)back] = s;
        // publish: swap back buffer with the middle, mark fresh
        const int prev = middle_.exchange(back | kFresh);
        back_ = prev & kIndexMask;
    }

    // ---- GUI thread
    template <typename Fn> void drainFrames(Fn&& fn) {
        const int ready = fifo_.getNumReady();
        if (ready <= 0) return;
        const auto scope = fifo_.read(ready);
        for (int i = 0; i < scope.blockSize1; ++i) fn(frames_[(size_t)(scope.startIndex1 + i)]);
        for (int i = 0; i < scope.blockSize2; ++i) fn(frames_[(size_t)(scope.startIndex2 + i)]);
    }
    // Returns a pointer to the newest lens snapshot if one arrived since the last call.
    const LensSnapshot* takeLens() {
        if ((middle_.load() & kFresh) == 0) return nullptr;
        const int prev = middle_.exchange(front_);
        front_ = prev & kIndexMask;
        return &lens_[(size_t)front_];
    }

    void clearFrames() { fifo_.reset(); }

private:
    static constexpr int kFresh = 4, kIndexMask = 3;
    juce::AbstractFifo fifo_{ kFifoSize };
    std::array<MeterFrame, kFifoSize> frames_{};
    std::array<LensSnapshot, 3> lens_{};
    int back_ = 0, front_ = 1;
    std::atomic<int> middle_{ 2 };
};

} // namespace td1
