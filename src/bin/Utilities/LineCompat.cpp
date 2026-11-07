#include "bin/Utilities/LineCompat.h"

#if RUNTIME_LINE == 17
// CommonLibSSE-NG 7.2.0 declares RE::BookMenu::OpenBookMenu (RE/B/BookMenu.h) but ships no definition, so a call
// links to nothing. This is the definition line 1's CommonLib carries (RELOCATION_ID(50122, 51053)). The AE id is the
// one 1.7 uses: on 1.7.104 id 51053 resolves to rva 0x907290, a function entry whose prologue takes the four register
// arguments in this order (rcx description, rdx extra list, r8 reference, r9 book) - checked with
// .MD\scripts\gamecode.py on 2026-11-07.
namespace RE
{
	void BookMenu::OpenBookMenu(const BSString& a_description, const ExtraDataList* a_extraList, TESObjectREFR* a_ref, TESObjectBOOK* a_book, const NiPoint3& a_pos, const NiMatrix3& a_rot, float a_scale, bool a_useDefaultPos)
	{
		using func_t = decltype(&BookMenu::OpenBookMenu);
		static REL::Relocation<func_t> func{ RELOCATION_ID(50122, 51053) };
		return func(a_description, a_extraList, a_ref, a_book, a_pos, a_rot, a_scale, a_useDefaultPos);
	}
}
#endif
