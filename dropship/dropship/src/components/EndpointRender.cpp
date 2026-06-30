#include "pch.h"

#include "Endpoint.h"

#include "images.h"
#include "core/Settings.h"

extern ImFont* font_title;
extern ImFont* font_subtitle;
extern ImFont* font_text;

extern std::unique_ptr<Settings> g_settings;

void Endpoint2::render(int i) {

	// Ping display smoothing (existing logic).
	if (*(this->ping.get())) {
		const auto ping_ms = (*(this->ping.get())).value();
		if (this->ping_ms_display != ping_ms)
		{
			static const float min_delay = 9.0f;
			static const float max_delay = 90.0f;

			if (ping_ms > 0 && this->ping_ms_display <= 0)
			{
				this->ping_ms_display = ping_ms;
			}
			else {
				float param = fmin((float)std::abs(ping_ms - this->ping_ms_display) / 24.0f, 1.0f);

				if (!(ImGui::GetFrameCount() % (int)fmax(min_delay, max_delay - (max_delay * param * half_pi)) != 0)) {

					if (this->ping_ms_display < ping_ms)
						this->ping_ms_display++;
					else
						this->ping_ms_display--;

					this->ping_ms_display = std::max(0, this->ping_ms_display);
				}
			}
		}
	}

	// Auto-block check (polls at most once every 2s, handles timeout as loss).
	{
		bool auto_block_enabled = g_settings
			? g_settings->getAppSettings().options.auto_block
			: false;
		this->_checkAutoBlock(auto_block_enabled);
	}

	//
	//

	static const ImU32 color_text_secondary = ImGui::ColorConvertFloat4ToU32({ 1, 1, 1, .8f });
	static const ImU32 white = ImGui::ColorConvertFloat4ToU32({ 1, 1, 1, 1.0f });
	static const ImU32 transparent = ImGui::ColorConvertFloat4ToU32({ 1, 1, 1, 0.0f });

	auto const w_list = ImGui::GetWindowDrawList();
	static auto& style = ImGui::GetStyle();

	static const auto grayed_out = false;
	const auto unsynced = (this->blocked != this->blocked_desired);

	static const ImU32 color_disabled = ImGui::ColorConvertFloat4ToU32({ .8f, .8f, .8f, 1.0f });
	static const ImU32 color_disabled_secondary = ImGui::ColorConvertFloat4ToU32({ .88f, .88f, .88f, 1.0f });
	static const ImU32 color_disabled_secondary_faded = ImGui::ColorConvertFloat4ToU32({ .95f, .95f, .95f, 1.0f });

	// Base colors from hue index.
	ImU32 color = grayed_out ? color_disabled : (ImU32) ImColor::HSV(1.0f - ((i + 1) / 32.0f), 0.4f, 1.0f, 1.0f);
	ImU32 color_secondary = grayed_out ? color_disabled_secondary : (ImU32) ImColor::HSV(1.0f - ((i + 1) / 32.0f), 0.3f, 1.0f, 1.0f);
	ImU32 color_secondary_faded = grayed_out ? color_disabled_secondary_faded : (ImU32) ImColor::HSV(1.0f - ((i + 1) / 32.0f), 0.2f, 1.0f, 0.4f * 1.0f);

	// Override with amber/orange when auto-blocked (distinct from manual block).
	if (this->_is_auto_blocked) {
		color               = ImGui::ColorConvertFloat4ToU32({ 1.0f, 0.55f, 0.0f, 1.0f });
		color_secondary     = ImGui::ColorConvertFloat4ToU32({ 1.0f, 0.65f, 0.1f, 1.0f });
		color_secondary_faded = ImGui::ColorConvertFloat4ToU32({ 1.0f, 0.70f, 0.2f, 0.4f });
	}

	ImGui::Dummy({ 0 ,0 });
	ImGui::SameLine(NULL, 16);

	ImGui::PushID(i);
	ImGui::PushStyleColor(ImGuiCol_HeaderHovered, this->blocked ? color_secondary_faded : color_secondary);
	ImGui::PushStyleColor(ImGuiCol_Header, this->blocked ? transparent : color);
	ImGui::PushStyleColor(ImGuiCol_HeaderActive, this->blocked ? color_secondary_faded : color_secondary);
	ImGui::PushStyleColor(ImGuiCol_NavHighlight, NULL);
	bool action = (ImGui::Selectable("##end", &(this->blocked_desired), ImGuiSelectableFlags_SelectOnClick, { ImGui::GetContentRegionAvail().x - 16, 74 - 9 }));
	ImGui::PopStyleColor(4);
	ImGui::PopID();

	auto hovered = ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenBlockedByPopup);

	// Background.
	if (!this->blocked) {
		if (hovered) {
			w_list->AddRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax(), color_secondary, 5, 0, 8);
		}
		else {
			w_list->AddRectFilled(ImGui::GetItemRectMin(), ImGui::GetItemRectMax(), color, 5, NULL);
		}
	}

	// Unsynced background animation.
	if (unsynced)
	{
		const auto& c = color_secondary_faded;
		const auto offset = (ImGui::GetFrameCount() / 4) % 40;
		const auto offset_vec = ImVec2((float)offset, (float)offset);
		const auto pos = ImGui::GetItemRectMin() - ImVec2(40, 40) + offset_vec;

		static const auto image = _get_image("background_diagonal");

		w_list->PushClipRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax(), true);
		w_list->AddImage(image.texture, pos, pos + ImVec2((float) image.width, (float) image.height), ImVec2(0, 0), ImVec2(1, 1), this->blocked ? color_secondary_faded : color);
		w_list->PopClipRect();
	}

	// Icon.
	const auto icon_frame = ImVec2({ ImGui::GetItemRectSize().y, ImGui::GetItemRectSize().y });
	static auto padding = ImVec2(21, 21);
	const auto icon = unsynced ? _get_texture("icon_wall_fire") : (this->blocked ? _get_texture("icon_block") : _get_texture("icon_allow"));
	w_list->AddImage(icon, ImGui::GetItemRectMin() + padding, ImGui::GetItemRectMin() + icon_frame - padding, ImVec2(0, 0), ImVec2(1, 1), this->blocked ? color : color_text_secondary);

	// Title text.
	auto pos = ImGui::GetItemRectMin() + ImVec2(icon_frame.x, 4) + ImVec2(-2, 0);
	w_list->AddText(font_title, 35, pos, this->blocked ? color : white, this->title.c_str());

	// Status line: show auto-block reason when applicable.
	pos += ImVec2(1, ImGui::GetItemRectSize().y - 24 - 16);
	std::string status_text = this->description;
	if (this->blocked) {
		if (this->_is_auto_blocked) {
			status_text += " [auto] " + this->_auto_block_reason;
		}
		else {
			status_text += " (blocked)";
		}
	}
	w_list->AddText(font_subtitle, 24, pos, this->blocked ? color_secondary : color_text_secondary, status_text.c_str());

	// Ping indicator.
	if ((*(this->ping))) {

		auto icon_wifi = _get_texture("icon_wifi");
		static ImVec2 frame = ImVec2(26, 26);

		const auto ping = (*(this->ping.get())).value();

		if (ping > 90)
			icon_wifi = _get_texture("icon_wifi_poor");
		else if (ping > 40)
			icon_wifi = _get_texture("icon_wifi_fair");
		else if (ping < 0)
			icon_wifi = _get_texture("icon_wifi_slash");

		auto wpos = ImGui::GetItemRectMax() - ImVec2(frame.x, ImGui::GetItemRectSize().y) + (style.FramePadding * ImVec2(-1, 1)) + ImVec2(-8, 1);
		w_list->AddImage(icon_wifi, wpos, wpos + frame, ImVec2(0, 0), ImVec2(1, 1), this->blocked ? color : white);

		if (!(ping < 0)) {
			auto const text = std::to_string(this->ping_ms_display);
			auto text_size = font_subtitle->CalcTextSizeA(24, FLT_MAX, 0.0f, text.c_str());
			auto tpos = ImGui::GetItemRectMax() - style.FramePadding - text_size + ImVec2(-8, -4);

			w_list->AddText(font_subtitle, 24, tpos, this->blocked ? color_secondary : color_text_secondary, text.c_str());
		}
	}

	// Tooltip showing auto-block details on hover.
	if (this->_is_auto_blocked && hovered) {
		std::string tip = std::format(
			"[auto-block]\nReason: {}\nIP: {}\n\nClick to manually unblock and reset.",
			this->_auto_block_reason,
			this->ip_ping.value_or("?")
		);
		ImGui::SetTooltip("%s", tip.c_str());
	}

	ImGui::Spacing();
}
