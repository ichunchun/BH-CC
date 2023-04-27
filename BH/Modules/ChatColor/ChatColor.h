#pragma once
#include "../../D2Structs.h"
#include "../Module.h"
#include "../../Config.h"
#include "../../Common.h"
#include <atomic>

class ChatColor : public Module {
private:
	std::atomic<bool> inGame;
	std::map<string, unsigned int> whisperColors;
	void UpdateInGame();
public:
	ChatColor() : Module("Chat Color") {};

	void Init();

	void OnLoad();
	void OnUnload();
	void LoadConfig();
	void OnGameJoin();
	void OnGameExit();
	void OnChatPacketRecv(BYTE* packet, bool *block);
};

LPCSTR __fastcall D2Lang_Unicode2WinPatch(LPSTR lpWinStr, LPWSTR lpUnicodeStr, DWORD dwBufSize);
LPWSTR __fastcall D2Lang_Win2UnicodePatch(LPWSTR lpUnicodeStr, LPCSTR lpWinStr, DWORD dwBufSize);

//DWORD __fastcall ChannelEnterCharPatch(D2EditBox* hWnd, BYTE bKeyCode);
//void MultiByteFixPatch_ASM();
