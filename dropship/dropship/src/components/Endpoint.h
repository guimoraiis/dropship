#pragma once

// target windows 7+
//#define _WIN32_WINNT_WIN7                   0x0601 // Windows 7
//#define _WIN32_WINNT = 0x0601

//#define _WIN32_WINNT_WINTHRESHOLD           0x0A00 // Windows 10
//#define _WIN32_WINNT_WIN10                  0x0A00 // Windows 10

//#include <SDKDDKVer.h>
// fix me ^

#include "util/ping/Pinger.h"
#include "core/AutoBlock.h"

// TODO
// 1. firewall->blockEndpoint()
// 2. firewall->unblockEndpoint()
// 3. firewall->delete()

// TODO
// 1. schedule task to periodically sync firewall state

struct _FirewallMeta {
	std::string _firewall_rule_address;
};

class Endpoint2
{

	public:

		Endpoint2(
			std::string title,
			std::string description,
			std::string ip_ping = std::string(), // this will be converted to an std::optional<>, defaults to empty which will be converted to std::nullopt in endpoint.cpp
			bool blocked = { false }
		);
		~Endpoint2();

		/* display a visual for the endpoint */
		void render(int hue_index = 0);

		/* is this endpoint blocked */
		//bool isBlocked();

		/* start pinging */
		void start_pinging();

		/* stop pinging */
		void stop_pinging();

		/* ping */
		// void ping();

		std::string getTitle();

		bool getBlockDesired();
		void setBlockDesired(bool b);

		bool _getBlockedState();
		void _setBlockedState(bool b);

		// Auto-block interface
		bool isAutoBlocked() const { return _is_auto_blocked; }
		const std::string& getAutoBlockReason() const { return _auto_block_reason; }

		// Call when user manually unblocks to reset auto-block state.
		void resetAutoBlock();

		// Called each render frame to check auto-block thresholds.
		void _checkAutoBlock(bool globally_enabled);

	private:
		/* this is the title displayed */
		std::string title;

		/* this is the description displayed */
		std::string description;

		/* optionally, specify an ip for the ping display feature */
		std::optional<std::string> ip_ping;

		/* is the endpoint blocked? and what's it's desired state? */
		bool blocked;
		bool blocked_desired;

		//union {
			std::unique_ptr<util::ping::Pinger> pinger;
			std::future<void> _ping_future;
		//};

		std::unique_ptr<std::optional<int>> ping;
		int ping_ms_display;

		// Auto-block state
		std::unique_ptr<core::AutoBlock> _auto_blocker;
		std::string _auto_block_reason;
		bool _is_auto_blocked { false };
		double _auto_block_last_check { -999.0 };
};

