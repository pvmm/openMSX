#include "ImGuiUtils.hh"

#include "ImGuiCpp.hh"

#include "BooleanSetting.hh"
#include "EnumSetting.hh"
#include "HotKey.hh"
#include "IntegerSetting.hh"
#include "FloatSetting.hh"
#include "VideoSourceSetting.hh"
#include "imgui_impl_sdl2.h"

#include "ranges.hh"

#include <imgui.h>
#include <imgui_stdlib.h>
#include <SDL.h>

#include <variant>

namespace openmsx {

void HelpMarker(std::string_view desc)
{
	ImGui::SameLine();
	ImGui::TextDisabled("(?)");
	simpleToolTip(desc);
}

void drawURL(std::string_view text, zstring_view url)
{
	auto pos = ImGui::GetCursorScreenPos();
	auto color = ImGui::GetColorU32(ImGuiCol_ButtonHovered);
	im::StyleColor(ImGuiCol_Text, color, [&]{
		ImGui::TextUnformatted(text);
	});

	simpleToolTip(url);

	if (ImGui::IsItemHovered()) { // underline
		auto size = ImGui::CalcTextSize(text);
		auto* drawList = ImGui::GetWindowDrawList();
		ImVec2 p1{pos.x, pos.y + size.y};
		ImVec2 p2{pos.x + size.x, pos.y + size.y};
		drawList->AddLine(p1, p2, color);
	}

	if (ImGui::IsItemClicked(ImGuiMouseButton_Left)) {
		SDL_OpenURL(url.c_str());
	}
}

std::string GetSettingDescription::operator()(const Setting& setting) const
{
	return std::string(setting.getDescription());
}

template<std::invocable<const Setting&> GetTooltip = GetSettingDescription>
static void settingStuff(Setting& setting, GetTooltip getTooltip = {})
{
	simpleToolTip([&] { return getTooltip(setting); });
	im::PopupContextItem([&]{
		auto defaultValue = setting.getDefaultValue();
		auto defaultString = defaultValue.getString();
		ImGui::StrCat("Default value: ", defaultString);
		if (defaultString.empty()) {
			ImGui::SameLine();
			ImGui::TextDisabled("<empty>");
		}
		if (ImGui::Button("Restore default")) {
			try {
				setting.setValue(defaultValue);
			} catch (MSXException&) {
				// ignore
			}
			ImGui::CloseCurrentPopup();
		}
	});
}

bool Checkbox(const HotKey& hotKey, BooleanSetting& setting)
{
	std::string name(setting.getBaseName());
	return Checkbox(hotKey, name.c_str(), setting);
}
bool Checkbox(const HotKey& hotKey, const char* label, BooleanSetting& setting, std::function<std::string(const Setting&)> getTooltip)
{
	bool value = setting.getBoolean();
	bool changed = ImGui::Checkbox(label, &value);
	try {
		if (changed) setting.setBoolean(value);
	} catch (MSXException&) {
		// ignore
	}
	settingStuff(setting, getTooltip);

	ImGui::SameLine();
	auto shortCut = getShortCutForCommand(hotKey, strCat("toggle ", setting.getBaseName()));
	auto spacing = std::max(0.0f, ImGui::GetContentRegionAvail().x - ImGui::CalcTextSize(shortCut).x);
	ImGui::SameLine(0.0f, spacing);
	ImGui::TextDisabled("%s", shortCut.c_str());

	return changed;
}

bool SliderInt(IntegerSetting& setting, ImGuiSliderFlags flags)
{
	std::string name(setting.getBaseName());
	return SliderInt(name.c_str(), setting, flags);
}
bool SliderInt(const char* label, IntegerSetting& setting, ImGuiSliderFlags flags)
{
	int value = setting.getInt();
	int min = setting.getMinValue();
	int max = setting.getMaxValue();
	bool changed = ImGui::SliderInt(label, &value, min, max, "%d", flags);
	try {
		if (changed) setting.setInt(value);
	} catch (MSXException&) {
		// ignore
	}
	settingStuff(setting);
	return changed;
}

bool SliderFloat(FloatSetting& setting, const char* format, ImGuiSliderFlags flags)
{
	std::string name(setting.getBaseName());
	return SliderFloat(name.c_str(), setting, format, flags);
}
bool SliderFloat(const char* label, FloatSetting& setting, const char* format, ImGuiSliderFlags flags)
{
	float value = setting.getFloat();
	float min = narrow_cast<float>(setting.getMinValue());
	float max = narrow_cast<float>(setting.getMaxValue());
	bool changed = ImGui::SliderFloat(label, &value, min, max, format, flags);
	try {
		if (changed) setting.setFloat(value);
	} catch (MSXException&) {
		// ignore
	}
	settingStuff(setting);
	return changed;
}

bool InputText(Setting& setting)
{
	std::string name(setting.getBaseName());
	return InputText(name.c_str(), setting);
}
bool InputText(const char* label, Setting& setting)
{
	auto value = std::string(setting.getValue().getString());
	bool changed = ImGui::InputText(label, &value, ImGuiInputTextFlags_EnterReturnsTrue) || ImGui::IsItemDeactivatedAfterEdit();
	try {
		if (changed) setting.setValue(TclObject(value));
	} catch (MSXException&) {
		// ignore
	}
	settingStuff(setting);
	return changed;
}

void ComboBox(const char* label, Setting& setting, std::function<std::string(const std::string&)> displayValue, EnumToolTips toolTips)
{
	auto* enumSetting = dynamic_cast<EnumSettingBase*>(&setting);
	assert(enumSetting);
	auto current = setting.getValue().getString();
	im::Combo(label, current.c_str(), [&]{
		for (const auto& entry : enumSetting->getMap()) {
			bool selected = entry.name == current;
			const auto& display = displayValue(entry.name);
			if (ImGui::Selectable(display.c_str(), selected)) {
				try {
					setting.setValue(TclObject(entry.name));
				} catch (MSXException&) {
					// ignore
				}
			}
			if (auto it = ranges::find(toolTips, entry.name, &EnumToolTip::value);
			    it != toolTips.end()) {
				simpleToolTip(it->tip);
			}
		}
	});
	settingStuff(setting);
}
void ComboBox(const char* label, Setting& setting, EnumToolTips toolTips)
{
	ComboBox(label, setting, std::identity{}, toolTips);
}
void ComboBox(Setting& setting, EnumToolTips toolTips)
{
	std::string name(setting.getBaseName());
	ComboBox(name.c_str(), setting, toolTips);
}

void ComboBox(VideoSourceSetting& setting) // TODO share code with EnumSetting?
{
	std::string name(setting.getBaseName());
	ComboBox(name.c_str(), setting);
}
void ComboBox(const char* label, VideoSourceSetting& setting) // TODO share code with EnumSetting?
{
	std::string name(setting.getBaseName());
	auto current = setting.getValue().getString();
	im::Combo(label, current.c_str(), [&]{
		for (const auto& value : setting.getPossibleValues()) {
			bool selected = value == current;
			if (ImGui::Selectable(std::string(value).c_str(), selected)) {
				try {
					setting.setValue(TclObject(value));
				} catch (MSXException&) {
					// ignore
				}
			}
		}
	});
	settingStuff(setting);
}

const char* getComboString(int item, const char* itemsSeparatedByZeros)
{
	const char* p = itemsSeparatedByZeros;
	while (true) {
		assert(*p);
		if (item == 0) return p;
		--item;
		while (*p) ++p;
		++p;
	}
}

std::string formatTime(double time)
{
	assert(time >= 0.0);
	auto hours = int(time * (1.0 / 3600.0));
	time -= double(hours * 3600);
	auto minutes = int(time * (1.0 / 60.0));
	time -= double(minutes * 60);
	auto seconds = int(time);
	time -= double(seconds);
	auto hundreds = int(100.0 * time);

	std::string result = "00:00:00.00";
	auto insert = [&](size_t pos, unsigned value) {
		assert(value < 100);
		result[pos + 0] = char('0' + (value / 10));
		result[pos + 1] = char('0' + (value % 10));
	};
	insert(0, hours % 100);
	insert(3, minutes);
	insert(6, seconds);
	insert(9, hundreds);
	return result;
}

float calculateFade(float current, float target, float period)
{
	const auto& io = ImGui::GetIO();
	auto step = io.DeltaTime / period;
	if (target > current) {
		return std::min(target, current + step);
	} else {
		return std::max(target, current - step);
	}
}

std::string getShortCutForCommand(const HotKey& hotkey, std::string_view command)
{
	for (const auto& info : hotkey.getGlobalBindings()) {
		if (info.command != command) continue;
		if (const auto* keyDown = std::get_if<KeyDownEvent>(&info.event)) {
			std::string result;
			auto modifiers = keyDown->getModifiers();
			if (modifiers & KMOD_CTRL)  strAppend(result, "CTRL+");
			if (modifiers & KMOD_SHIFT) strAppend(result, "SHIFT+");
			if (modifiers & KMOD_ALT)   strAppend(result, "ALT+");
			if (modifiers & KMOD_GUI)   strAppend(result, "GUI+");
			strAppend(result, SDL_GetKeyName(keyDown->getKeyCode()));
			return result;
		}
	}
	return "";
}

// Adapted from ImGui_ImplSDL2_KeycodeToImGuiKey
static int ImGuiKeyToSDL2_KeyCode(ImGuiKey key)
{
    switch (key)
    {
        case ImGuiKey_Tab: return SDLK_TAB;
        case ImGuiKey_LeftArrow: return SDLK_LEFT;
        case ImGuiKey_RightArrow: return SDLK_RIGHT;
        case ImGuiKey_UpArrow: return SDLK_UP;
        case ImGuiKey_DownArrow: return SDLK_DOWN;
        case ImGuiKey_PageUp: return SDLK_PAGEUP;
        case ImGuiKey_PageDown: return SDLK_PAGEDOWN;
        case ImGuiKey_Home: return SDLK_HOME;
        case ImGuiKey_End: return SDLK_END;
        case ImGuiKey_Insert: return SDLK_INSERT;
        case ImGuiKey_Delete: return SDLK_DELETE;
        case ImGuiKey_Backspace: return SDLK_BACKSPACE;
        case ImGuiKey_Space: return SDLK_SPACE;
        case ImGuiKey_Enter: return SDLK_RETURN;
        case ImGuiKey_Escape: return SDLK_ESCAPE;
        case ImGuiKey_Apostrophe: return SDLK_QUOTE;
        case ImGuiKey_Comma: return SDLK_COMMA;
        case ImGuiKey_Minus: return SDLK_MINUS;
        case ImGuiKey_Period: return SDLK_PERIOD;
        case ImGuiKey_Slash: return SDLK_SLASH;
        case ImGuiKey_Semicolon: return SDLK_SEMICOLON;
        case ImGuiKey_Equal: return SDLK_EQUALS;
        case ImGuiKey_LeftBracket: return SDLK_LEFTBRACKET;
        case ImGuiKey_Backslash: return SDLK_BACKSLASH;
        case ImGuiKey_RightBracket: return SDLK_RIGHTBRACKET;
        case ImGuiKey_GraveAccent: return SDLK_BACKQUOTE;
        case ImGuiKey_CapsLock: return SDLK_CAPSLOCK;
        case ImGuiKey_ScrollLock: return SDLK_SCROLLLOCK;
        case ImGuiKey_NumLock: return SDLK_NUMLOCKCLEAR;
        case ImGuiKey_PrintScreen: return SDLK_PRINTSCREEN;
        case ImGuiKey_Pause: return SDLK_PAUSE;
        case ImGuiKey_Keypad0: return SDLK_KP_0;
        case ImGuiKey_Keypad1: return SDLK_KP_1;
        case ImGuiKey_Keypad2: return SDLK_KP_2;
        case ImGuiKey_Keypad3: return SDLK_KP_3;
        case ImGuiKey_Keypad4: return SDLK_KP_4;
        case ImGuiKey_Keypad5: return SDLK_KP_5;
        case ImGuiKey_Keypad6: return SDLK_KP_6;
        case ImGuiKey_Keypad7: return SDLK_KP_7;
        case ImGuiKey_Keypad8: return SDLK_KP_8;
        case ImGuiKey_Keypad9: return SDLK_KP_9;
        case ImGuiKey_KeypadDecimal: return SDLK_KP_PERIOD;
        case ImGuiKey_KeypadDivide: return SDLK_KP_DIVIDE;
        case ImGuiKey_KeypadMultiply: return SDLK_KP_MULTIPLY;
        case ImGuiKey_KeypadSubtract: return SDLK_KP_MINUS;
        case ImGuiKey_KeypadAdd: return SDLK_KP_PLUS;
        case ImGuiKey_KeypadEnter: return SDLK_KP_ENTER;
        case ImGuiKey_KeypadEqual: return SDLK_KP_EQUALS;
        case ImGuiKey_LeftCtrl: return SDLK_LCTRL;
        case ImGuiKey_LeftShift: return SDLK_LSHIFT;
        case ImGuiKey_LeftAlt: return SDLK_LALT;
        case ImGuiKey_LeftSuper: return SDLK_LGUI;
        case ImGuiKey_RightCtrl: return SDLK_RCTRL;
        case ImGuiKey_RightShift: return SDLK_RSHIFT;
        case ImGuiKey_RightAlt: return SDLK_RALT;
        case ImGuiKey_RightSuper: return SDLK_RGUI;
        case ImGuiKey_Menu: return SDLK_APPLICATION;
        case ImGuiKey_0: return SDLK_0;
        case ImGuiKey_1: return SDLK_1;
        case ImGuiKey_2: return SDLK_2;
        case ImGuiKey_3: return SDLK_3;
        case ImGuiKey_4: return SDLK_4;
        case ImGuiKey_5: return SDLK_5;
        case ImGuiKey_6: return SDLK_6;
        case ImGuiKey_7: return SDLK_7;
        case ImGuiKey_8: return SDLK_8;
        case ImGuiKey_9: return SDLK_9;
        case ImGuiKey_A: return SDLK_a;
        case ImGuiKey_B: return SDLK_b;
        case ImGuiKey_C: return SDLK_c;
        case ImGuiKey_D: return SDLK_d;
        case ImGuiKey_E: return SDLK_e;
        case ImGuiKey_F: return SDLK_f;
        case ImGuiKey_G: return SDLK_g;
        case ImGuiKey_H: return SDLK_h;
        case ImGuiKey_I: return SDLK_i;
        case ImGuiKey_J: return SDLK_j;
        case ImGuiKey_K: return SDLK_k;
        case ImGuiKey_L: return SDLK_l;
        case ImGuiKey_M: return SDLK_m;
        case ImGuiKey_N: return SDLK_n;
        case ImGuiKey_O: return SDLK_o;
        case ImGuiKey_P: return SDLK_p;
        case ImGuiKey_Q: return SDLK_q;
        case ImGuiKey_R: return SDLK_r;
        case ImGuiKey_S: return SDLK_s;
        case ImGuiKey_T: return SDLK_t;
        case ImGuiKey_U: return SDLK_u;
        case ImGuiKey_V: return SDLK_v;
        case ImGuiKey_W: return SDLK_w;
        case ImGuiKey_X: return SDLK_x;
        case ImGuiKey_Y: return SDLK_y;
        case ImGuiKey_Z: return SDLK_z;
        case ImGuiKey_F1: return SDLK_F1;
        case ImGuiKey_F2: return SDLK_F2;
        case ImGuiKey_F3: return SDLK_F3;
        case ImGuiKey_F4: return SDLK_F4;
        case ImGuiKey_F5: return SDLK_F5;
        case ImGuiKey_F6: return SDLK_F6;
        case ImGuiKey_F7: return SDLK_F7;
        case ImGuiKey_F8: return SDLK_F8;
        case ImGuiKey_F9: return SDLK_F9;
        case ImGuiKey_F10: return SDLK_F10;
        case ImGuiKey_F11: return SDLK_F11;
        case ImGuiKey_F12: return SDLK_F12;
        case ImGuiKey_F13: return SDLK_F13;
        case ImGuiKey_F14: return SDLK_F14;
        case ImGuiKey_F15: return SDLK_F15;
        case ImGuiKey_F16: return SDLK_F16;
        case ImGuiKey_F17: return SDLK_F17;
        case ImGuiKey_F18: return SDLK_F18;
        case ImGuiKey_F19: return SDLK_F19;
        case ImGuiKey_F20: return SDLK_F20;
        case ImGuiKey_F21: return SDLK_F21;
        case ImGuiKey_F22: return SDLK_F22;
        case ImGuiKey_F23: return SDLK_F23;
        case ImGuiKey_F24: return SDLK_F24;
        case ImGuiKey_AppBack: return SDLK_AC_BACK;
        case ImGuiKey_AppForward: return SDLK_AC_FORWARD;
	default: return SDLK_UNKNOWN;
    }
    return ImGuiKey_None;
}

std::optional<std::string> getKeyChordName(ImGuiKeyChord keyChord)
{
	int keyCode = ImGuiKeyToSDL2_KeyCode(ImGuiKey(keyChord & ~ImGuiMod_Mask_));
	if (keyCode != SDLK_UNKNOWN) {
		auto scanCode = SDL_GetScancodeFromKey(keyCode);
		return strCat((keyChord & ImGuiMod_Ctrl ? "Ctrl+" : ""),
			(keyChord & ImGuiMod_Shift ? "Shift+" : ""),
			(keyChord & ImGuiMod_Alt ? "Alt+" : ""),
			(keyChord & ImGuiMod_Super ? (ImGui::GetIO().ConfigMacOSXBehaviors ? "Cmd+" : "Super+") : ""),
			SDL_GetScancodeName(scanCode));
	}
	return {};
}

std::optional<ImGuiKeyChord> getKeyChordValue(const std::string_view name)
{
	ImGuiKeyChord keyMods = {};
	keyMods |= StringOp::containsCaseInsensitive(name, "Ctrl+")  ? ImGuiMod_Ctrl  : 0;
	keyMods |= StringOp::containsCaseInsensitive(name, "Shift+") ? ImGuiMod_Shift : 0;
	keyMods |= StringOp::containsCaseInsensitive(name, "Alt+")   ? ImGuiMod_Alt   : 0;
	SDL_Scancode scanCode = SDL_GetScancodeFromName(std::string(name).c_str());
	SDL_Keycode keyCode = SDL_GetKeyFromScancode(scanCode);
	return keyCode != SDLK_UNKNOWN ? std::optional<ImGuiKeyChord>(keyMods | ImGui_ImplSDL2_KeycodeToImGuiKey(keyCode)) : std::nullopt;
}

void setColors(int style)
{
	// style: 0->dark, 1->light, 2->classic
	bool light = style == 1;

	//                                            AA'BB'GG'RR
	imColors[size_t(imColor::TRANSPARENT   )] = 0x00'00'00'00;
	imColors[size_t(imColor::BLACK         )] = 0xff'00'00'00;
	imColors[size_t(imColor::WHITE         )] = 0xff'ff'ff'ff;
	imColors[size_t(imColor::GRAY          )] = 0xff'80'80'80;
	imColors[size_t(imColor::YELLOW        )] = 0xff'00'ff'ff;
	imColors[size_t(imColor::RED_BG        )] = 0x40'00'00'ff;
	imColors[size_t(imColor::YELLOW_BG     )] = 0x80'00'ff'ff;

	imColors[size_t(imColor::TEXT          )] = ImGui::GetColorU32(ImGuiCol_Text);
	imColors[size_t(imColor::TEXT_DISABLED )] = ImGui::GetColorU32(ImGuiCol_TextDisabled);

	imColors[size_t(imColor::ERROR         )] = 0xff'00'00'ff;
	imColors[size_t(imColor::WARNING       )] = 0xff'33'b3'ff;

	imColors[size_t(imColor::COMMENT       )] = 0xff'5c'ff'5c;
	imColors[size_t(imColor::VARIABLE      )] = 0xff'ff'ff'00;
	imColors[size_t(imColor::LITERAL       )] = light ? 0xff'9c'5d'27 : 0xff'00'ff'ff;
	imColors[size_t(imColor::PROC          )] = 0xff'cd'00'cd;
	imColors[size_t(imColor::OPERATOR      )] = 0xff'cd'cd'00;

	imColors[size_t(imColor::KEY_ACTIVE    )] = 0xff'10'40'ff;
	imColors[size_t(imColor::KEY_NOT_ACTIVE)] = 0x80'00'00'00;
}

} // namespace openmsx
