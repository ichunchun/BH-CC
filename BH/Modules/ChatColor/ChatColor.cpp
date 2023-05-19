#include "ChatColor.h"
#include "../../BH.h"
#include "../../D2Ptrs.h"
#include "../../D2Stubs.h"

std::map<std::string, Toggle> ChatColor::Toggles;

Patch* UnicodeSupport2 = new Patch(Jump, D2LANG, { 0x8320, 0x82E0 }, (DWORD)D2Lang_Win2UnicodePatch, 5);
//{PatchJMP,    DLLOFFSET2(D2LANG, 0x6FC08320, 0x6FC082E0),      (DWORD)D2Lang_Win2UnicodePatch,          5 ,   &fLocalizationSupport},
//佣兵/好友头像等级显示，招唤物个数显示
Patch* petHead = new Patch(Call, D2CLIENT, { 0x5B582, 0xB254C }, (DWORD)DrawPetHeadPath_ASM, 7);
//{PatchCALL, DLLOFFSET2(D2CLIENT, 0x6FB0B582, 0x6FB39662), (DWORD)DrawPetHeadPath_ASM, 7, & fDefault},
Patch* partyHead = new Patch(Call, D2CLIENT, { 0x5BBE0, 0xB254C }, (DWORD)DrawPartyHeadPath_ASM, 6);
//{ PatchCALL,   DLLOFFSET2(D2CLIENT, 0x6FB0BBE0, 0x6FB39F90),    (DWORD)DrawPartyHeadPath_ASM,          6 ,   &fDefault },

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
	petHead->Install();
	partyHead->Install();
	LoadConfig();

}void ChatColor::OnUnload()
{
	UnicodeSupport2->Remove();
	petHead->Remove();
	partyHead->Remove();
}

void ChatColor::LoadConfig() {
	whisperColors.clear();

	BH::config->ReadAssoc("Whisper Color", whisperColors);
	BH::config->ReadToggle("Merc Protect", "None", true, Toggles["Merc Protect"]);  //佣兵保护
	BH::config->ReadToggle("Merc Boring", "None", true, Toggles["Merc Boring"]);  //佣兵吐槽
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
void AutoToBelt()
{
	UnitAny* unit = D2CLIENT_GetPlayerUnit();
	if (!unit)
		return;

	//"hp", "mp", "rv"
		//循环查找背包里面的药
	for (UnitAny* pItem = unit->pInventory->pFirstItem; pItem; pItem = pItem->pItemData->pNextInvItem) {
		if (pItem->pItemData->ItemLocation == STORAGE_INVENTORY || pItem->pItemData->ItemLocation == STORAGE_CUBE) {   //只取背包和盒子里面的
			char* code = D2COMMON_GetItemText(pItem->dwTxtFileNo)->szCode;
			if (code[0] == 'h' && code[1] == 'p') {
				DWORD itemId = pItem->dwUnitId;  //红药
				//试一下这个不知道是不是填充的数据包
				BYTE PacketData[5] = { 0x63, 0, 0, 0, 0 };
				*reinterpret_cast<int*>(PacketData + 1) = itemId;
				D2NET_SendPacket(5, 0, PacketData);
			}
			if (code[0] == 'm' && code[1] == 'p') {
				DWORD itemId = pItem->dwUnitId;  //蓝药
				//试一下这个不知道是不是填充的数据包
				BYTE PacketData[5] = { 0x63, 0, 0, 0, 0 };
				*reinterpret_cast<int*>(PacketData + 1) = itemId;
				D2NET_SendPacket(5, 0, PacketData);
			}
			if (code[0] == 'r' && code[1] == 'v') {
				DWORD itemId = pItem->dwUnitId;  //紫药
				//试一下这个不知道是不是填充的数据包
				BYTE PacketData[5] = { 0x63, 0, 0, 0, 0 };
				*reinterpret_cast<int*>(PacketData + 1) = itemId;
				D2NET_SendPacket(5, 0, PacketData);
			}
		}
	}
}

//佣兵自动喝药,只支持腰带上的药
void AutoMercDrink(double perHP) {
	UnitAny* unit = D2CLIENT_GetPlayerUnit();
	if (!unit)
		return;
	if (perHP > 65) return;  //大于65的阈值也直接跳过算了
	//"hp", "mp", "rv"
		//循环查找背包里面的药
	DWORD itemId = 0;
	char* code = "NULL";
	for (UnitAny* pItem = unit->pInventory->pFirstItem; pItem; pItem = pItem->pItemData->pNextInvItem) {
		if (pItem->pItemData->ItemLocation == STORAGE_NULL && pItem->pItemData->NodePage == NODEPAGE_BELTSLOTS) {   //只能用腰带里的
			code = D2COMMON_GetItemText(pItem->dwTxtFileNo)->szCode;
			if (code[0] == 'h' && code[1] == 'p') {
				if (perHP <= 65) {  //吃红药
					itemId = pItem->dwUnitId;  //红药
				}
			}
			if (code[0] == 'r' && code[1] == 'v') {
				if (perHP <= 35) {  //吃紫药
					itemId = pItem->dwUnitId;  //紫药
				}
			}
			if (itemId != 0) break;  //找到了就直接中断循环
		}
	}
	if (itemId == 0 && perHP <= 35) {
		if (ChatColor::Toggles["Merc Boring"].state)
			PrintText(Yellow, "你都没药啦，还不带我回家？：%.0f%%", perHP);
	}
	else {
		if (code[0] == 'h' && code[1] == 'p') {
			if (ChatColor::Toggles["Merc Boring"].state)
				PrintText(Red, "佣兵少血啦,红喝起来,干杯！：%.0f%%", perHP);
		}
		else if (code[0] == 'r' && code[1] == 'v') {
			if (ChatColor::Toggles["Merc Boring"].state)
				PrintText(Purple, "佣兵少血啦,紫喝起来,干杯！：%.0f%%", perHP);
		}
	}
	if (itemId == 0) return;  //没药了或不用喝直接跳过
	BYTE PacketData[13] = { 0x26, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
	*reinterpret_cast<int*>(PacketData + 1) = itemId;
	*reinterpret_cast<int*>(PacketData + 5) = 1;  //是否给佣兵吃药,1是给，0是不给
	D2NET_SendPacket(13, 0, PacketData);
	//喝完还要填充回去
	Task::Enqueue([=]()->void {
		Sleep(1000);  //停1秒试试看
		AutoToBelt();
		});
}

//头像相关
void __fastcall DrawPetHeadPath(int xpos, UnitAny* pUnit) {

	wchar_t wszTemp[512];
	wsprintfW(wszTemp, L"%d", D2COMMON_GetUnitStat(pUnit, STAT_LEVEL, 0));
	//swprintf(wszTemp, L"%d，%f/%f", D2COMMON_GetUnitStat(pUnit, STAT_LEVEL, 0), hp,maxhp);

	//D2WIN_DrawText(1, wszTemp, xpos + 5, 57, 0);
	//DWORD dwOldFone = D2WIN_SetTextFont(1);   //设置字体
	D2WIN_DrawText(wszTemp, xpos + 5, 57, White, 0);
	//D2WIN_DrawText(wszTemp, xpos + 700, 570, White, 0);
	//D2WIN_SetTextFont(dwOldFone);
	bool test1 = ChatColor::Toggles["Merc Protect"].state;
	if (test1) {
		//下面是佣兵自动喝药
		DWORD mHP = D2COMMON_GetUnitStat(pUnit, STAT_HP, 0);
		if (mHP > 0x8000) {  //这个说明merc的血发生了变化
			double maxhp = (double)(D2COMMON_GetUnitStat(pUnit, STAT_MAXHP, 0) >> 8);
			double hp = (double)(mHP >> 8);
			double perHP = (hp / maxhp) * 100.0;

			if (perHP < mercLastHP)
			{
				//PrintText(White, "佣兵血变少：%.0f%%", perHP);
				//开始喝药
				AutoMercDrink(perHP);
			}
			mercLastHP = perHP;
		}
	}
}

void __declspec(naked) DrawPetHeadPath_ASM()
{
	//ecx  xpos
	//eax  ypos
	//ebx  pPet
	__asm {
		push esi

		mov edx, ebx
		call DrawPetHeadPath

		pop esi
		//org
		mov[esp + 0x56], 0
		ret
	}
}


void __fastcall DrawPartyHeadPath(int xpos, RosterUnit* pRosterUnit) {

	wchar_t wszTemp[512];

	//if (tShowPartyLevel.isOn) {
	wsprintfW(wszTemp, L"%d", pRosterUnit->wLevel);
	//DrawD2Text(1, wszTemp, xpos + 5, 57, 0);
	//DWORD dwOldFone = D2WIN_SetTextFont(1);   //设置字体
	D2WIN_DrawText(wszTemp, xpos + 5, 57, White, 0);
	//D2WIN_SetTextFont(dwOldFone);
	//}

	//if (tShowPartyPosition.isOn) {  //场景号看不懂，先不开
	//	wsprintfW(wszTemp, L"%d", pRosterUnit->dwLevelNo);
	//	DrawCenterText(1, wszTemp, xpos + 20, 15, 4, 1, 1);
	//}
}

void __declspec(naked) DrawPartyHeadPath_ASM()
{
	//[ebx]  xpos
	//eax  ypos
	//[esp+0C]  pRosterUnit
	__asm {
		mov ecx, dword ptr[ebx]
		mov edx, dword ptr[esp + 0xC]

		push ebx
		push edi
		push eax

		call DrawPartyHeadPath

		pop eax
		pop edi
		pop ebx

		mov ecx, dword ptr[ebx]
		mov edx, dword ptr[esp + 0xC]
		ret
	}
}