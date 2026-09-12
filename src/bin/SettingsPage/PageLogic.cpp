#include "PageLogic.h"

#include <algorithm>
#include <cctype>
#include <cstdarg>
#include <cstdint>
#include <cstdio>

namespace SettingsPage
{
	namespace
	{
		bool ContainsDot(const std::string& a_text)
		{
			return a_text.find('.') != std::string::npos;
		}

		bool LooksHex(const std::string& a_text)
		{
			std::size_t index = 0;
			while (index < a_text.size() && std::isspace(static_cast<unsigned char>(a_text[index]))) {
				++index;
			}
			return (index + 1 < a_text.size()) &&
			       a_text[index] == '0' &&
			       (a_text[index + 1] == 'x' || a_text[index + 1] == 'X');
		}

		std::string Printf(const char* a_format, ...)
		{
			char buffer[64]{};
			va_list args;
			va_start(args, a_format);
			const int written = std::vsnprintf(buffer, sizeof(buffer), a_format, args);
			va_end(args);
			return (written > 0) ? std::string(buffer, static_cast<std::size_t>(written)) : std::string{};
		}

		// Does one requirement hold, given the target entry's current value?
		bool RequirementHolds(const Requirement& a_requirement, const Entry& a_target, const ResolvedValue& a_value)
		{
			if (a_requirement.boolValue.has_value()) {
				const bool fallback = a_target.defaultBool.value_or(false);
				return a_value.AsBool(fallback) == *a_requirement.boolValue;
			}
			if (a_requirement.numberValue.has_value()) {
				const double fallback = a_target.defaultNumber.value_or(0.0);
				return a_value.AsNumber(fallback) == *a_requirement.numberValue;
			}
			if (a_requirement.stringValue.has_value()) {
				const std::string fallback = a_target.defaultString.value_or(std::string{});
				return a_value.AsString(fallback) == *a_requirement.stringValue;
			}
			// A requirement with no value at all states nothing; treat it as met rather than as a
			// reason to disable a working control.
			return true;
		}
	}

	const char* VisibilityName(Visibility a_visibility)
	{
		switch (a_visibility) {
		case Visibility::Disabled:
			return "disabled";
		case Visibility::Hidden:
			return "hidden";
		default:
			return "shown";
		}
	}

	Visibility EvaluateVisibility(
		const Catalog& a_catalog,
		const Panel& a_panel,
		const Entry& a_entry,
		const ValueStore& a_store)
	{
		for (const auto& requirement : a_entry.requirements) {
			const Entry* target = a_catalog.FindById(a_panel, requirement.id);
			if (!target) {
				continue;  // unresolvable: treated as satisfied, see the header
			}

			const ResolvedValue value = a_store.Get(a_panel, *target);
			if (!RequirementHolds(requirement, *target, value)) {
				return (a_entry.failAction == FailAction::Hide) ? Visibility::Hidden : Visibility::Disabled;
			}
		}
		return Visibility::Shown;
	}

	std::string PresetKeyFor(const Entry& a_entry)
	{
		if (a_entry.type != EntryType::Color || a_entry.iniKey.empty()) {
			return {};
		}
		return a_entry.iniKey + "Preset";
	}

	bool IsOverriddenByPreset(const Panel& a_panel, const Entry& a_entry, const ValueStore& a_store)
	{
		const std::string presetKey = PresetKeyFor(a_entry);
		if (presetKey.empty()) {
			return false;
		}

		const ResolvedValue preset = a_store.Get(a_panel, a_entry.iniSection, presetKey);
		if (!preset.Found()) {
			return false;  // no preset for this colour - it is used as stored
		}
		// 0 is "Custom", which is the only value that leaves the stored colour in force.
		return preset.AsUInt32(0u) != 0u;
	}

	std::string FormatNumberLike(double a_value, const ResolvedValue& a_current)
	{
		// Match the spelling already in the file. "0.200000" stays six-decimal; "278" stays an
		// integer. Without this the page rewrites notation it did not mean to change, and every
		// slider touched once churns its whole line.
		if (a_current.Found() && !ContainsDot(a_current.raw)) {
			const double rounded = (a_value < 0.0) ? (a_value - 0.5) : (a_value + 0.5);
			return Printf("%lld", static_cast<long long>(rounded));
		}
		return Printf("%.6f", a_value);
	}

	std::string FormatBoolLike(bool a_value, const ResolvedValue& a_current)
	{
		if (a_current.Found() && !a_current.raw.empty()) {
			switch (a_current.raw[0]) {
			case '1':
			case '0':
				return a_value ? "1" : "0";
			case 'y':
			case 'n':
				return a_value ? "yes" : "no";
			case 'Y':
			case 'N':
				return a_value ? "Yes" : "No";
			case 'o':
				return a_value ? "on" : "off";
			case 'O':
				return a_value ? "On" : "Off";
			case 'T':
			case 'F':
				return a_value ? "True" : "False";
			default:
				break;
			}
		}
		return a_value ? "true" : "false";
	}

	std::string FormatColorLike(const ColorValue& a_value, const ResolvedValue& a_current)
	{
		const std::uint32_t packed = ColorToPacked(a_value);

		// presets/N/Styles.ini writes 0xC8A08C6E; the main files write decimal. Both parse, because
		// the readers use base 0 - but write back whichever was there.
		if (a_current.Found() && LooksHex(a_current.raw)) {
			return Printf("0x%08X", packed);
		}
		// A ".000000" tail is legal here (dMenu's float-slider serialisation, which GetUInt32Value
		// tolerates deliberately) and is preserved so the line does not churn.
		if (a_current.Found() && ContainsDot(a_current.raw)) {
			return Printf("%u.000000", packed);
		}
		return Printf("%u", packed);
	}

	std::string FormatIndexLike(int a_value, const ResolvedValue& a_current)
	{
		if (a_current.Found() && ContainsDot(a_current.raw)) {
			return Printf("%d.000000", a_value);
		}
		return Printf("%d", a_value);
	}
}
