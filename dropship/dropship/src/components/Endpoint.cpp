#include "pch.h"

#include "Endpoint.h"
#include "core/Settings.h"
#include "core/AutoBlockLog.h"

extern std::unique_ptr<Settings> g_settings;
extern std::unique_ptr<core::AutoBlockLog> g_autoblock_log;

Endpoint2::Endpoint2(
	std::string title,
	std::string description,
	std::string ip_ping,
	bool blocked
):

	title(title),
	description(description),
	ip_ping(ip_ping.empty() ? std::nullopt : std::optional<std::string>{ ip_ping }),
	ping(std::make_unique<std::optional<int>>(std::nullopt)),
	ping_ms_display(-1),

	blocked(blocked),
	blocked_desired(blocked)
{
	if (this->ip_ping) {
		this->start_pinging();

		core::AutoBlockThresholds thresholds;
		if (g_settings) thresholds = g_settings->getAutoBlockThresholds();
		this->_auto_blocker = std::make_unique<core::AutoBlock>(thresholds);
	}
}

Endpoint2::~Endpoint2() {
	this->stop_pinging();
}


void Endpoint2::start_pinging() {
	try {
		if (!this->ip_ping) return;
		if (this->pinger.get() != nullptr) return;

		this->pinger = std::make_unique<util::ping::Pinger>(this->ip_ping.value(), ping);
		this->_ping_future = std::async(std::launch::async, [&]
		{
			try {
				(*(this->pinger)).start();
			}
			catch (const std::exception& ex) {
				std::println("{}", ex.what());
			}
			catch (...)
			{
				printf("error caught!");
			}
		});
	}
	catch (const std::exception& ex) {
		std::println("{}", ex.what());
	}
	catch (...)
	{
		printf("error caught!");
	}
}

void Endpoint2::stop_pinging() {
	if (this->pinger.get() != nullptr) {
		(*(this->pinger.get())).stop();
		this->_ping_future.get();
		(this->pinger).reset();
	}
}

std::string Endpoint2::getTitle() {
	return this->title;
}

bool Endpoint2::getBlockDesired() {
	return this->blocked_desired;
}

void Endpoint2::setBlockDesired(bool b) {
	this->blocked_desired = b;
}

bool Endpoint2::_getBlockedState() {
	return this->blocked;
}

void Endpoint2::_setBlockedState(bool b) {
	this->blocked = b;
}

void Endpoint2::resetAutoBlock() {
	_is_auto_blocked = false;
	_auto_block_reason = "";
	if (_auto_blocker) _auto_blocker->resetCooldown();
}

void Endpoint2::_checkAutoBlock(bool globally_enabled) {
	if (!globally_enabled || !_auto_blocker || !this->ip_ping) return;

	// Sample the current ping value at most once every 2 seconds.
	if (ImGui::GetTime() < _auto_block_last_check + 2.0) return;
	_auto_block_last_check = ImGui::GetTime();

	// Read current ping: -1 if not available (treat as timeout = loss).
	int current_ping = -1;
	if (this->ping && *(this->ping)) {
		current_ping = (*(this->ping)).value();
	}

	// Update thresholds in case settings changed.
	if (g_settings) {
		_auto_blocker->setThresholds(g_settings->getAutoBlockThresholds());
	}

	_auto_blocker->recordPing(current_ping);

	// Transition: healthy → should block.
	if (!_is_auto_blocked && _auto_blocker->shouldBlock()) {
		_is_auto_blocked = true;

		auto reason = _auto_blocker->getLastReason();
		if (reason == core::AutoBlockReason::PacketLoss) {
			_auto_block_reason = std::format("packet loss {:.0f}%",
				_auto_blocker->getPacketLossPercent());
		}
		else {
			_auto_block_reason = std::format("latency {}ms",
				_auto_blocker->getAvgLatencyMs());
		}

		this->setBlockDesired(true);
		_auto_blocker->onBlocked();

		if (g_autoblock_log) {
			core::AutoBlockEvent ev{
				.timestamp = std::chrono::system_clock::now(),
				.endpoint_title = this->title,
				.ip_address = this->ip_ping.value_or("?"),
				.packet_loss_percent = _auto_blocker->getPacketLossPercent(),
				.avg_latency_ms = _auto_blocker->getAvgLatencyMs(),
				.reason_text = _auto_block_reason,
			};
			g_autoblock_log->logEvent(ev);
		}

		println("[autoblock] triggered for '{}' — {}", this->title, _auto_block_reason);
	}
}
