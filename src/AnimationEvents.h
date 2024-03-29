#pragma once
#include <DKUtil/Logger.hpp>

using EventResult = RE::BSEventNotifyControl;
class animEventHandler
{
private:
	template <class Ty>
	static Ty SafeWrite64Function(uintptr_t addr, Ty data)
	{
		DWORD oldProtect;
		void* _d[2];
		memcpy(_d, &data, sizeof(data));
		size_t len = sizeof(_d[0]);

		VirtualProtect((void*)addr, len, PAGE_EXECUTE_READWRITE, &oldProtect);
		Ty olddata;
		memset(&olddata, 0, sizeof(Ty));
		memcpy(&olddata, (void*)addr, len);
		memcpy((void*)addr, &_d[0], len);
		VirtualProtect((void*)addr, len, oldProtect, &oldProtect);
		return olddata;
	}

	typedef RE::BSEventNotifyControl (animEventHandler::*FnProcessEvent)(RE::BSAnimationGraphEvent& a_event, RE::BSTEventSource<RE::BSAnimationGraphEvent>* dispatcher);

	RE::BSEventNotifyControl HookedProcessEvent(RE::BSAnimationGraphEvent& a_event, RE::BSTEventSource<RE::BSAnimationGraphEvent>* src);

	static void HookSink(uintptr_t ptr)
	{
		FnProcessEvent fn = SafeWrite64Function(ptr + 0x8, &animEventHandler::HookedProcessEvent);
		fnHash.insert(std::pair<uint64_t, FnProcessEvent>(ptr, fn));
	}

public:
	/*Hook anim event sink*/
	static void Register(bool player, bool NPC)
	{
		if (player) {
			INFO("Sinking animation event hook for player");
			REL::Relocation<uintptr_t> pcPtr{ RE::VTABLE_PlayerCharacter[2] };
			HookSink(pcPtr.address());
		}
		if (NPC) {
			INFO("Sinking animation event hook for NPC");
			REL::Relocation<uintptr_t> npcPtr{ RE::VTABLE_Character[2] };
			HookSink(npcPtr.address());
		}
		INFO("Sinking complete.");
	}
	
	static void RegisterForPlayer() {
		Register(true, false);
	}

protected:
	static std::unordered_map<uint64_t, FnProcessEvent> fnHash;
};
