#include "pch.h"
#include "AutoBlock.h"

namespace core {

AutoBlock::AutoBlock(const AutoBlockThresholds& thresholds)
    : _thresholds(thresholds) {}

void AutoBlock::recordPing(int ping_ms) {
    _ping_history.push_back(ping_ms);
    while ((int)_ping_history.size() > _thresholds.window_ping_count) {
        _ping_history.pop_front();
    }
}

bool AutoBlock::shouldBlock() {
    if ((int)_ping_history.size() < _thresholds.window_ping_count) return false;
    if (isInCooldown()) return false;

    const float loss = _calcPacketLossPercent();
    if (loss >= (float)_thresholds.packet_loss_threshold_percent) {
        _last_reason = AutoBlockReason::PacketLoss;
        return true;
    }

    const int avg_lat = _calcAvgLatencyMs();
    if (avg_lat > 0 && avg_lat >= _thresholds.latency_threshold_ms) {
        _last_reason = AutoBlockReason::HighLatency;
        return true;
    }

    return false;
}

void AutoBlock::onBlocked() {
    _cooldown_until = std::chrono::steady_clock::now()
        + std::chrono::minutes(_thresholds.cooldown_minutes);
}

void AutoBlock::resetCooldown() {
    _cooldown_until = std::nullopt;
    _ping_history.clear();
}

bool AutoBlock::isInCooldown() const {
    if (!_cooldown_until) return false;
    return std::chrono::steady_clock::now() < *_cooldown_until;
}

float AutoBlock::getPacketLossPercent() const {
    return _calcPacketLossPercent();
}

int AutoBlock::getAvgLatencyMs() const {
    return _calcAvgLatencyMs();
}

AutoBlockReason AutoBlock::getLastReason() const {
    return _last_reason;
}

size_t AutoBlock::getPingHistorySize() const {
    return _ping_history.size();
}

void AutoBlock::setThresholds(const AutoBlockThresholds& thresholds) {
    _thresholds = thresholds;
}

const AutoBlockThresholds& AutoBlock::getThresholds() const {
    return _thresholds;
}

float AutoBlock::_calcPacketLossPercent() const {
    if (_ping_history.empty()) return 0.0f;
    int losses = 0;
    for (auto p : _ping_history)
        if (p < 0) ++losses;
    return (float)losses / (float)_ping_history.size() * 100.0f;
}

int AutoBlock::_calcAvgLatencyMs() const {
    long long sum = 0;
    int count = 0;
    for (auto p : _ping_history) {
        if (p >= 0) { sum += p; ++count; }
    }
    return count > 0 ? (int)(sum / count) : 0;
}

} // namespace core
