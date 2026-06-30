#pragma once

#include <string>
#include <vector>
#include <chrono>
#include <filesystem>

namespace core {

struct AutoBlockEvent {
    std::chrono::system_clock::time_point timestamp;
    std::string endpoint_title;
    std::string ip_address;
    float packet_loss_percent;
    int avg_latency_ms;
    std::string reason_text;
};

class AutoBlockLog {
public:
    AutoBlockLog() = default;

    void logEvent(const AutoBlockEvent& event);
    const std::vector<AutoBlockEvent>& getEvents() const { return _events; }

    void saveToFile(const std::filesystem::path& path) const;

private:
    std::vector<AutoBlockEvent> _events;

    static std::string _formatTimestamp(const std::chrono::system_clock::time_point& tp);
};

} // namespace core
