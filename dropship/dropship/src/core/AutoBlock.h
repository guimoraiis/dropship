#pragma once

#include <deque>
#include <string>
#include <chrono>
#include <optional>

namespace core {

enum class AutoBlockReason {
    None,
    PacketLoss,
    HighLatency,
};

struct AutoBlockThresholds {
    int packet_loss_threshold_percent { 20 };
    int latency_threshold_ms { 200 };
    int window_ping_count { 20 };
    int cooldown_minutes { 10 };
};

class AutoBlock {
public:
    explicit AutoBlock(const AutoBlockThresholds& thresholds);

    // Feed a ping result. ping_ms < 0 means timeout/loss.
    void recordPing(int ping_ms);

    // Returns true if threshold breached and not in cooldown.
    bool shouldBlock();

    // Call when block was applied to start cooldown.
    void onBlocked();

    // Call when user manually unblocks to reset cooldown/state.
    void resetCooldown();

    bool isInCooldown() const;

    // Stats for display and logging.
    float getPacketLossPercent() const;
    int getAvgLatencyMs() const;
    AutoBlockReason getLastReason() const;
    size_t getPingHistorySize() const;

    void setThresholds(const AutoBlockThresholds& thresholds);
    const AutoBlockThresholds& getThresholds() const;

private:
    AutoBlockThresholds _thresholds;
    std::deque<int> _ping_history;
    std::optional<std::chrono::steady_clock::time_point> _cooldown_until;
    AutoBlockReason _last_reason { AutoBlockReason::None };

    float _calcPacketLossPercent() const;
    int _calcAvgLatencyMs() const;
};

} // namespace core
