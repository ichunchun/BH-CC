#include "ChatColor.h"
#include "../../BH.h"
#include "../../D2Ptrs.h"
#include "../../D2Stubs.h"

Patch* UnicodeSupport2 = new Patch(Jump, D2LANG, { 0x8320, 0x82E0 }, (DWORD)D2Lang_Win2UnicodePatch, 5);
//{PatchJMP,    DLLOFFSET2(D2LANG, 0x6FC08320, 0x6FC082E0),      (DWORD)D2Lang_Win2UnicodePatch,          5 ,   &fLocalizationSupport},

void ChatColor::Init() {
	
}

void Print(DWORD color, char* format, ...) {
	va_list vaArgs;
	va_start(vaArgs, format);
	int len = _vscprintf(format, vaArgs) + 1;
	char* str = new char[len];
	vsprintf_s(str, len, format, vaArgs);
	va_end(vaArgs);

	wchar_t* wstr = new wchar_t[len];
	MultiByteToWideChar(CODE_PAGE, 0, str, -1, wstr, len);
	D2CLIENT_PrintGameString(wstr, color);
	delete[] wstr;

	delete[] str;
}

void ChatColor::OnGameJoin() {
	inGame = true;
}

void ChatColor::OnGameExit() {
	inGame = false;
}

void ChatColor::OnLoad() {
	UnicodeSupport2->Install();
	LoadConfig();

}void ChatColor::OnUnload()
{
	UnicodeSupport2->Remove();
}

void ChatColor::LoadConfig() {
	whisperColors.clear();

	BH::config->ReadAssoc("Whisper Color", whisperColors);
}

LPCSTR __fastcall D2Lang_Unicode2WinPatch(LPSTR lpWinStr, LPWSTR lpUnicodeStr, DWORD dwBufSize)
{
	WideCharToMultiByte(CP_ACP, 0, lpUnicodeStr, -1, lpWinStr, dwBufSize, NULL, NULL);
	return lpWinStr;
}

void UnicodeFix2(LPWSTR lpText)
{
	if (lpText) {
		size_t LEN = wcslen(lpText);
		for (size_t i = 0; i < LEN; i++)
		{
			if (lpText[i] == 0xf8f5) // Unicode 'y'
				lpText[i] = 0xff; // Ansi 'y'
		}
	}
}

int MyMultiByteToWideChar(
	UINT CodePage,         // code page
	DWORD dwFlags,         // character-type options
	LPCSTR lpMultiByteStr, // string to map
	int cbMultiByte,       // number of bytes in string
	LPWSTR lpWideCharStr,  // wide-character buffer
	int cchWideChar        // size of buffer
)
{
	int r = MultiByteToWideChar(CodePage, dwFlags, lpMultiByteStr, cbMultiByte, lpWideCharStr, cchWideChar);
	UnicodeFix2(lpWideCharStr);
	return r;
}

LPWSTR __fastcall D2Lang_Win2UnicodePatch(LPWSTR lpUnicodeStr, LPCSTR lpWinStr, DWORD dwBufSize)
{
	MyMultiByteToWideChar(CP_ACP, 0, lpWinStr, -1, lpUnicodeStr, dwBufSize);
	return lpUnicodeStr;
}


void ChatColor::UpdateInGame() {
	if ((*p_D2WIN_FirstControl) && inGame) inGame = false;
	else if (D2CLIENT_GetPlayerUnit() && !inGame) inGame = true;
}

void ChatColor::OnChatPacketRecv(BYTE* packet, bool* block) {
	// the game thread only checks if we've left the game every 10ms, so we update inGame here.
	UpdateInGame();	
	if (packet[1] == 0x0F && inGame) {
		unsigned int event_id = *(unsigned short int*)&packet[4];

		if (event_id == 4) {
			const char* from = (const char*)&packet[28];
			unsigned int fromLen = strlen(from);

			const char* message = (const char*)&packet[28 + fromLen + 1];
			unsigned int messageLen = strlen(message);

			bool replace = false;
			int color = 0;
			if (whisperColors.find(from) != whisperColors.end()) {
				replace = true;
				color = whisperColors[from];
			}
			UpdateInGame();
			if (replace && inGame) {
				*block = true;

				Print(color, "%s | %s", from, message);
			}
		}
	}
}
