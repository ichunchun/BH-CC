#pragma once
#include "../../D2Structs.h"
#include "../Module.h"
#include "../../Config.h"
#include "../../Common.h"
#include <atomic>
#include "../../Task.h"

class ChatColor : public Module {
private:
	std::atomic<bool> inGame;
	std::map<string, unsigned int> whisperColors;
	void UpdateInGame();
public:
	static std::map<std::string, Toggle> Toggles;
	ChatColor() : Module("Chat Color") {};

	void Init();

	void OnLoad();
	void OnUnload();
	void LoadConfig();
	void OnGameJoin();
	void OnGameExit();
	void OnChatPacketRecv(BYTE* packet, bool *block);
};

static double mercLastHP = 100;  //佣兵上1次的血量百分比

LPWSTR __fastcall D2Lang_Win2UnicodePatch(LPWSTR lpUnicodeStr, LPCSTR lpWinStr, DWORD dwBufSize);

void ShowManaPatch_ASM();

void DrawPetHeadPath_ASM();
void DrawPartyHeadPath_ASM();