#include "pch.h"
#include "AutoBlockLog.h"

#include <fstream>
#include <ctime>
#include <filesystem>

namespace core {

void AutoBlockLog::logEvent(const AutoBlockEvent& event) {
    _events.push_back(event);
    println("[autoblock] {} | {} ({}) | {}",
        _formatTimestamp(event.timestamp),
        event.endpoint_title,
        event.ip_address,
        event.reason_text);
}

void AutoBlockLog::saveToFile(const std::filesystem::path& path) const {
    try {
        std::filesystem::create_directories(path.parent_path());
        std::ofstream out(path, std::ios::trunc);
        if (!out) return;

        out << "# dropship auto-block log\n";
        out << "# format: timestamp | endpoint | ip | reason\n";
        out << "# ----\n";

        for (auto& e : _events) {
            out << std::format("{} | {} | {} | {}\n",
                _formatTimestamp(e.timestamp),
                e.endpoint_title,
                e.ip_address,
                e.reason_text);
        }
    }
    catch (...) {}
}

std::string AutoBlockLog::_formatTimestamp(const std::chrono::system_clock::time_point& tp) {
    auto t = std::chrono::system_clock::to_time_t(tp);
    std::tm tm_buf{};
    localtime_s(&tm_buf, &t);
    return std::format("{:04d}-{:02d}-{:02d} {:02d}:{:02d}:{:02d}",
        tm_buf.tm_year + 1900, tm_buf.tm_mon + 1, tm_buf.tm_mday,
        tm_buf.tm_hour, tm_buf.tm_min, tm_buf.tm_sec);
}

} // namespace core
