#include "ItemMover.h"
#include "../Item/Item.h"
#include "../../BH.h"
#include "../../D2Ptrs.h"
#include "../../D2Stubs.h"
#include "../../D2Helpers.h"
#include "../ScreenInfo/ScreenInfo.h"
#include "../ChatColor/ChatColor.h"
using namespace Drawing;

// This module was inspired by the RedVex plugin "Item Mover", written by kaiks.
// Thanks to kaiks for sharing his code.

#define INVENTORY_WIDTH  inventoryLayout->SlotWidth
#define INVENTORY_HEIGHT inventoryLayout->SlotHeight
#define INVENTORY_LEFT   inventoryLayout->Left
#define INVENTORY_RIGHT  inventoryLayout->Right
#define INVENTORY_TOP    inventoryLayout->Top
#define INVENTORY_BOTTOM inventoryLayout->Bottom

#define STASH_WIDTH  stashLayout->SlotWidth
#define STASH_HEIGHT stashLayout->SlotHeight
#define STASH_LEFT   stashLayout->Left
#define STASH_RIGHT  stashLayout->Right
#define STASH_TOP    stashLayout->Top
#define STASH_BOTTOM stashLayout->Bottom

#define CUBE_WIDTH  cubeLayout->SlotWidth
#define CUBE_HEIGHT cubeLayout->SlotHeight
#define CUBE_LEFT   cubeLayout->Left
#define CUBE_RIGHT  cubeLayout->Right
#define CUBE_TOP    cubeLayout->Top
#define CUBE_BOTTOM cubeLayout->Bottom

#define CELL_SIZE inventoryLayout->SlotPixelHeight

std::string POTIONS[] = { "hp", "mp", "rv" };

DWORD idBookId;
DWORD unidItemId;

bool ItemMover::Init() {
	BnetData* pData = (*p_D2LAUNCH_BnData);
	if (!pData) { return false; }
	int xpac = pData->nCharFlags & PLAYER_TYPE_EXPANSION;

	if (xpac) {
		stashLayout = p_D2CLIENT_StashLayout;
		StashItemIds = LODStashItemIds;
	}
	else {
		stashLayout = p_D2CLIENT_ClassicStashLayout;
		StashItemIds = ClassicStashItemIds;
	}
	inventoryLayout = p_D2CLIENT_InventoryLayout;
	cubeLayout = p_D2CLIENT_CubeLayout;

	if (!InventoryItemIds) {
		InventoryItemIds = new int[INVENTORY_WIDTH * INVENTORY_HEIGHT];
	}
	if (!StashItemIds) {
		StashItemIds = new int[STASH_WIDTH * STASH_HEIGHT];
	}
	if (!CubeItemIds) {
		CubeItemIds = new int[CUBE_WIDTH * CUBE_HEIGHT];
	}

	//PrintText(1, "Got positions: %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d",
	//	INVENTORY_WIDTH,
	//	INVENTORY_HEIGHT,
	//	STASH_WIDTH,
	//	STASH_HEIGHT,
	//	CUBE_WIDTH,
	//	CUBE_HEIGHT,
	//	INVENTORY_LEFT,
	//	INVENTORY_TOP,
	//	STASH_LEFT,
	//	STASH_TOP,
	//	CUBE_LEFT,
	//	CUBE_TOP,
	//	CELL_SIZE
	//);

	return true;
}

bool ItemMover::LoadInventory(UnitAny *unit, int source, int sourceX, int sourceY, bool shiftState, bool ctrlState, int stashUI, int invUI) {
	bool returnValue = false;

	memset(InventoryItemIds, 0, INVENTORY_WIDTH * INVENTORY_HEIGHT * sizeof(int));
	memset(StashItemIds, 0, STASH_WIDTH * STASH_HEIGHT * sizeof(int));
	memset(CubeItemIds, 0, CUBE_WIDTH * CUBE_HEIGHT * sizeof(int));

	unsigned int itemId = 0;
	BYTE itemXSize, itemYSize;
	bool cubeInInventory = false, cubeAnywhere = false;
	for (UnitAny *pItem = unit->pInventory->pFirstItem; pItem; pItem = pItem->pItemData->pNextInvItem) {
		int *p, width;
		if (pItem->pItemData->ItemLocation == STORAGE_INVENTORY) {
			p = InventoryItemIds;
			width = INVENTORY_WIDTH;
		} else if (pItem->pItemData->ItemLocation == STORAGE_STASH) {
			p = StashItemIds;
			width = STASH_WIDTH;
		} else if (pItem->pItemData->ItemLocation == STORAGE_CUBE) {
			p = CubeItemIds;
			width = CUBE_WIDTH;
		} else {
			continue;
		}

		bool box = false;
		char *code = D2COMMON_GetItemText(pItem->dwTxtFileNo)->szCode;
		if (code[0] == 'b' && code[1] == 'o' && code[2] == 'x') {
			if (pItem->pItemData->ItemLocation == STORAGE_INVENTORY) {
				cubeInInventory = true;
				cubeAnywhere = true;
			}
			if (pItem->pItemData->ItemLocation == STORAGE_STASH) {
				cubeAnywhere = true;
			}
			box = true;
		}

		int xStart = pItem->pObjectPath->dwPosX;
		int yStart = pItem->pObjectPath->dwPosY;
		BYTE xSize = D2COMMON_GetItemText(pItem->dwTxtFileNo)->xSize;
		BYTE ySize = D2COMMON_GetItemText(pItem->dwTxtFileNo)->ySize;
		for (int x = xStart; x < xStart + xSize; x++) {
			for (int y = yStart; y < yStart + ySize; y++) {
				p[y*width + x] = pItem->dwUnitId;

				// If you click to move the cube into itself, your character ends up in
				// the amusing (and apparently permanent) state where he has no visible
				// cube and yet is unable to pick one up. Logging out does not fix it.
				// So we disable all cube movements to be on the safe side.
				if (x == sourceX && y == sourceY && pItem->pItemData->ItemLocation == source && !box) {
					// This is the item we want to move
					itemId = pItem->dwUnitId;
					itemXSize = xSize;
					itemYSize = ySize;
				}
			}
		}
	}

	int destination;
	if (ctrlState && shiftState && ((stashUI && cubeAnywhere) || (invUI && cubeInInventory)) && source != STORAGE_CUBE) {
		destination = STORAGE_CUBE;
	} else if (ctrlState) {
		destination = STORAGE_NULL;  // i.e. the ground
	} else if (source == STORAGE_STASH || source == STORAGE_CUBE) {
		destination = STORAGE_INVENTORY;
	} else if (source == STORAGE_INVENTORY && D2CLIENT_GetUIState(UI_STASH)) {
		destination = STORAGE_STASH;
	} else if (source == STORAGE_INVENTORY && D2CLIENT_GetUIState(UI_CUBE)) {
		destination = STORAGE_CUBE;
	} else {
		return false;
	}

	// Find a spot for the item in the destination container
	if (itemId > 0) {
		returnValue = FindDestination(destination, itemId, itemXSize, itemYSize);
	}

	FirstInit = true;
	return returnValue;
}

bool ItemMover::FindDestination(int destination, unsigned int itemId, BYTE xSize, BYTE ySize) {
	int *p, width = 0, height = 0;
	if (destination == STORAGE_INVENTORY) {
		p = InventoryItemIds;
		width = INVENTORY_WIDTH;
		height = INVENTORY_HEIGHT;
	} else if (destination == STORAGE_STASH) {
		p = StashItemIds;
		width = STASH_WIDTH;
		height = STASH_HEIGHT;
	} else if (destination == STORAGE_CUBE) {
		p = CubeItemIds;
		width = CUBE_WIDTH;
		height = CUBE_HEIGHT;
	}

	bool found = false;
	int destX = 0, destY = 0;
	if (width) {
		bool first_y = true;
		for (int x = 0; x < width; x++) {
			for (int y = 0; y < height; y++) {
				bool abort = false;
				int vacancies = 0;
				for (int testx = x; testx < x + xSize && testx < width; testx++) {
					for (int testy = y; testy < y + ySize && testy < height; testy++) {
						if (p[testy*width + testx]) {
							abort = true;
							break;
						} else {
							vacancies++;
						}
					}
					if (abort) {
						break;
					}
				}
				if (vacancies == xSize * ySize) {
					// Found an empty spot that's big enough for the item
					found = true;
					destX = x;
					destY = y;
					break;
				}
				if (xSize == 1) {
					if (first_y) {
						if (x + 1 < width) {
							x++;
							y--;
							first_y = false;
						}
					} else {
						first_y = true;
						x--;
					}
				}
			} // end y loop
			if (found) {
				break;
			}
			if (xSize == 2 && x % 2 == 0 && x + 2 >= width) {
				x = 0;
			} else {
				x++;
			}
		} // end x loop
	} else {
		found = true;
	}

	if (found) {
		Lock();
		if (ActivePacket.startTicks == 0) {
			ActivePacket.itemId = itemId;
			ActivePacket.x = destX;
			ActivePacket.y = destY;
			ActivePacket.startTicks = BHGetTickCount();
			ActivePacket.destination = destination;
		}
		Unlock();
	}

	return found;
}

void ItemMover::PickUpItem() {
	BYTE PacketData[5] = {0x19,0,0,0,0};
	*reinterpret_cast<int*>(PacketData + 1) = ActivePacket.itemId;
	D2NET_SendPacket(5, 1, PacketData);
}

void ItemMover::PutItemInContainer() {
	BYTE PacketData[17] = {0x18,0,0,0,0};
	*reinterpret_cast<int*>(PacketData + 1) = ActivePacket.itemId;
	*reinterpret_cast<int*>(PacketData + 5) = ActivePacket.x;
	*reinterpret_cast<int*>(PacketData + 9) = ActivePacket.y;
	*reinterpret_cast<int*>(PacketData + 13)= ActivePacket.destination;
	D2NET_SendPacket(17, 1, PacketData);
}

void ItemMover::PutItemOnGround() {
	BYTE PacketData[5] = {0x17,0,0,0,0};
	*reinterpret_cast<int*>(PacketData + 1) = ActivePacket.itemId;
	D2NET_SendPacket(5, 1, PacketData);
}

void ItemMover::AutoToBelt()
{
	UnitAny* unit = D2CLIENT_GetPlayerUnit();
	if (!unit)
		return;

	/*UnitAny* test1 = D2COMMON_GetBeltItem(unit->pInventory, 0);
	DWORD test2 = D2COMMON_GetFreeBeltSlot(unit->pInventory,test1, 0);*/

	//"hp", "mp", "rv"
		//ѭ�����ұ��������ҩ
	for (UnitAny* pItem = unit->pInventory->pFirstItem; pItem; pItem = pItem->pItemData->pNextInvItem) {
		if (pItem->pItemData->ItemLocation == STORAGE_INVENTORY || pItem->pItemData->ItemLocation == STORAGE_CUBE) {   //ֻȡ�����ͺ��������
			char* code = D2COMMON_GetItemText(pItem->dwTxtFileNo)->szCode;
			if (code[0] == 'h' && code[1] == 'p') {
				DWORD itemId = pItem->dwUnitId;  //��ҩ
				//��һ�������֪���ǲ����������ݰ�
				BYTE PacketData[5] = { 0x63, 0, 0, 0, 0 };
				*reinterpret_cast<int*>(PacketData + 1) = itemId;
				D2NET_SendPacket(5, 0, PacketData);
			}
			if (code[0] == 'm' && code[1] == 'p') {
				DWORD itemId = pItem->dwUnitId;  //��ҩ
				//��һ�������֪���ǲ����������ݰ�
				BYTE PacketData[5] = { 0x63, 0, 0, 0, 0 };
				*reinterpret_cast<int*>(PacketData + 1) = itemId;
				D2NET_SendPacket(5, 0, PacketData);
			}
			if (code[0] == 'r' && code[1] == 'v') {
				DWORD itemId = pItem->dwUnitId;  //��ҩ
				//��һ�������֪���ǲ����������ݰ�
				BYTE PacketData[5] = { 0x63, 0, 0, 0, 0 };
				*reinterpret_cast<int*>(PacketData + 1) = itemId;
				D2NET_SendPacket(5, 0, PacketData);
			}
			if (code[0] == 'y' && code[1] == 'p' && code[2] == 's') {
				DWORD itemId = pItem->dwUnitId;  //�ⶾҩ
				//��һ�������֪���ǲ����������ݰ�
				BYTE PacketData[5] = { 0x63, 0, 0, 0, 0 };
				*reinterpret_cast<int*>(PacketData + 1) = itemId;
				D2NET_SendPacket(5, 0, PacketData);
			}
		}
	}
}


void ItemMover::OnLeftClick(bool up, unsigned int x, unsigned int y, bool* block) {
	UnitAny *unit = D2CLIENT_GetPlayerUnit();
	bool shiftState = ((GetKeyState(VK_LSHIFT) & 0x80) || (GetKeyState(VK_RSHIFT) & 0x80));
	
	if (up || !unit || !shiftState || D2CLIENT_GetCursorItem()>0 ||
		(!D2CLIENT_GetUIState(UI_INVENTORY) && !D2CLIENT_GetUIState(UI_STASH)
			&& !D2CLIENT_GetUIState(UI_CUBE) && !D2CLIENT_GetUIState(UI_NPCSHOP)) ||
		!Init()) {
		return;
	}

	unidItemId = 0;
	idBookId = 0;
	
	int mouseX,mouseY;	

	for (UnitAny *pItem = unit->pInventory->pFirstItem; pItem; pItem = pItem->pItemData->pNextInvItem) {
		char *code = D2COMMON_GetItemText(pItem->dwTxtFileNo)->szCode;
		if ((pItem->pItemData->dwFlags & ITEM_IDENTIFIED) <= 0) {
			int xStart = pItem->pObjectPath->dwPosX;
			int yStart = pItem->pObjectPath->dwPosY;
			BYTE xSize = D2COMMON_GetItemText(pItem->dwTxtFileNo)->xSize;
			BYTE ySize = D2COMMON_GetItemText(pItem->dwTxtFileNo)->ySize;
			if (pItem->pItemData->ItemLocation == STORAGE_INVENTORY) {
				mouseX = (*p_D2CLIENT_MouseX - INVENTORY_LEFT) / CELL_SIZE;
				mouseY = (*p_D2CLIENT_MouseY - INVENTORY_TOP) / CELL_SIZE;
			} else if(pItem->pItemData->ItemLocation == STORAGE_STASH) {
				mouseX = (*p_D2CLIENT_MouseX - STASH_LEFT) / CELL_SIZE;
				mouseY = (*p_D2CLIENT_MouseY - STASH_TOP) / CELL_SIZE;
			} else if(pItem->pItemData->ItemLocation == STORAGE_CUBE) {
				mouseX = (*p_D2CLIENT_MouseX - CUBE_LEFT) / CELL_SIZE;
				mouseY = (*p_D2CLIENT_MouseY - CUBE_TOP) / CELL_SIZE;
			}
			for (int x = xStart; x < xStart + xSize; x++) {
				for (int y = yStart; y < yStart + ySize; y++) {
					if (x == mouseX && y == mouseY) {
						if ((pItem->pItemData->ItemLocation == STORAGE_STASH && !D2CLIENT_GetUIState(UI_STASH)) || (pItem->pItemData->ItemLocation == STORAGE_CUBE && !D2CLIENT_GetUIState(UI_CUBE))) {
							return;
						}
						unidItemId = pItem->dwUnitId;								
					}
				}
			}
		}
		if (code[0] == 'i' && code[1] == 'b' && code[2] == 'k' && (pItem->pItemData->ItemLocation == STORAGE_INVENTORY || pItem->pItemData->ItemLocation == STORAGE_CUBE || pItem->pItemData->ItemLocation == STORAGE_STASH) && D2COMMON_GetUnitStat(pItem, STAT_AMMOQUANTITY, 0)>0) {
			idBookId = pItem->dwUnitId;
		}
		if (unidItemId > 0 && idBookId > 0) {
			BYTE PacketData[13] = { 0x20, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
			*reinterpret_cast<int*>(PacketData + 1) = idBookId;
			*reinterpret_cast<WORD*>(PacketData + 5) = (WORD)unit->pPath->xPos;
			*reinterpret_cast<WORD*>(PacketData + 9) = (WORD)unit->pPath->yPos;
			D2NET_SendPacket(13, 0, PacketData);
			*block = true;
			return;
		}
	}
}

void ItemMover::OnRightClick(bool up, unsigned int x, unsigned int y, bool* block) {
	UnitAny *unit = D2CLIENT_GetPlayerUnit();
	bool shiftState = ((GetKeyState(VK_LSHIFT) & 0x80) || (GetKeyState(VK_RSHIFT) & 0x80));
	bool ctrlState = ((GetKeyState(VK_LCONTROL) & 0x80) || (GetKeyState(VK_RCONTROL) & 0x80));
	if (up || !unit || !(shiftState || ctrlState) || !Init()) {
		return;
	}

	int source, sourceX, sourceY;
	int invUI = D2CLIENT_GetUIState(UI_INVENTORY);
	int stashUI = D2CLIENT_GetUIState(UI_STASH);
	int cubeUI = D2CLIENT_GetUIState(UI_CUBE);
	if ((invUI || stashUI || cubeUI) && x >= INVENTORY_LEFT && x <= INVENTORY_RIGHT && y >= INVENTORY_TOP && y <= INVENTORY_BOTTOM) {
		source = STORAGE_INVENTORY;
		sourceX = (x - INVENTORY_LEFT) / CELL_SIZE;
		sourceY = (y - INVENTORY_TOP) / CELL_SIZE;
	} else if (stashUI && x >= STASH_LEFT && x <= STASH_RIGHT && y >= STASH_TOP && y <= STASH_BOTTOM) {
		source = STORAGE_STASH;
		sourceX = (x - STASH_LEFT) / CELL_SIZE;
		sourceY = (y - STASH_TOP) / CELL_SIZE;
	} else if (cubeUI && x >= CUBE_LEFT && x <= CUBE_RIGHT && y >= CUBE_TOP && y <= CUBE_BOTTOM) {
		source = STORAGE_CUBE;
		sourceX = (x - CUBE_LEFT) / CELL_SIZE;
		sourceY = (y - CUBE_TOP) / CELL_SIZE;
	} else {
		return;
	}

	bool moveItem = LoadInventory(unit, source, sourceX, sourceY, shiftState, ctrlState, stashUI, invUI);
	if (moveItem) {
		PickUpItem();
	}
	*block = true;
}

void ItemMover::LoadConfig() {
	BH::config->ReadKey("Use TP Tome", "VK_NUMPADADD", TpKey);
	BH::config->ReadKey("Use TP Tome Back", "VK_BACK", TpBackKey);
	BH::config->ReadKey("One Key Exit Game", "VK_NUMPADSUB", ExitGameKey);
	BH::config->ReadKey("Use Healing Potion", "VK_NUMPADMULTIPLY", HealKey);
	BH::config->ReadKey("Use Mana Potion", "VK_NUMPADSUBTRACT", ManaKey);
	BH::config->ReadKey("Use Rejuv Potion", "VK_NUMPADDIVIDE", JuvKey);
	BH::config->ReadKey("One Key Fill Belt", "VK_SHIFT", BeltKey);
	BH::config->ReadInt("Low TP Warning", tp_warn_quantity);
	BH::config->ReadKey("Use Auto Pickup", "VK_U", AutoPickupKey);
}

void ItemMover::OnLoad() {
	LoadConfig();
	Drawing::Texthook* colored_text;

	settingsTab = new Drawing::UITab("���˵��", BH::settingsUI);

	unsigned int x = 8;
	unsigned int y = 7;
	new Drawing::Texthook(settingsTab, x, y, "��������Զ����ݼ� (��esc�����ݼ�)");
	new Drawing::Keyhook(settingsTab, x, (y += 15), &TpKey ,  "���ٿ���:     ");
	new Drawing::Keyhook(settingsTab, x, (y += 15), &TpBackKey, "���ٻس�:       ");
	new Drawing::Keyhook(settingsTab, x, (y += 15), &ExitGameKey, "һ���˳�:     ");
	new Drawing::Keyhook(settingsTab, x, (y += 15), &HealKey, "�Ⱥ�ҩ:    ");
	new Drawing::Keyhook(settingsTab, x, (y += 15), &ManaKey, "����ҩ:       ");
	new Drawing::Keyhook(settingsTab, x, (y += 15), &JuvKey,  "����ҩ:      ");
	new Drawing::Keyhook(settingsTab, x, (y += 15), &BeltKey, "�������:       ");
	new Drawing::Keyhook(settingsTab, x, (y += 15), &AutoPickupKey, "�Զ�ʰȡ:       ");
	int keyhook_x = 150;
	new Checkhook(settingsTab, 4, (y += 15), &ChatColor::Toggles["Merc Protect"].state, "Ӷ������");
	new Keyhook(settingsTab, keyhook_x, (y + 2), &ChatColor::Toggles["Merc Protect"].toggle, "");
	new Checkhook(settingsTab, 4, (y += 15), &ChatColor::Toggles["Merc Boring"].state, "Ӷ���²�");
	new Keyhook(settingsTab, keyhook_x, (y + 2), &ChatColor::Toggles["Merc Boring"].toggle, "");
	y += 15;

	new Drawing::Texthook(settingsTab, x, (y += 15), "��Ʒ�����ƶ�˵��");
	colored_text = new Drawing::Texthook(settingsTab, x, (y += 15),
			"Shift+��� ����������ڱ������Ϳ��Կ��ټ�����Ʒ");
	colored_text = new Drawing::Texthook(settingsTab, x, (y += 15),
			"Shift+�Ҽ� �ڱ����ʹ򿪵����ӣ���򿪵ĺ��ӣ�֮���ƶ�");
	colored_text->SetColor(Gold);
	colored_text = new Drawing::Texthook(settingsTab, x, (y += 15),
			"Ctrl+�Ҽ� ����Ʒ�ӵ���");
	colored_text->SetColor(Gold);
	colored_text = new Drawing::Texthook(settingsTab, x, (y += 15),
			"Ctrl+shift+�Ҽ� �ƶ���Ʒ���ر��ŵĺ���");
	colored_text->SetColor(Gold);

	colored_text = new Drawing::Texthook(settingsTab, x, (y += 15),
			"");
	colored_text->SetColor(Gold);

}

void ItemMover::OnKey(bool up, BYTE key, LPARAM lParam, bool* block)  {
	UnitAny *unit = D2CLIENT_GetPlayerUnit();
	if (!unit)
		return;

	bool shiftState = ((GetKeyState(VK_LSHIFT) & 0x80) || (GetKeyState(VK_RSHIFT) & 0x80));
	if (shiftState) return;  //����shift�Ͳ�����,��������ȥ,��Ҫ����Ӷ����ҩ

	if (!up && (key == HealKey || key == ManaKey || key == JuvKey)) {
		int idx = key == JuvKey ? 2 : key == ManaKey ? 1 : 0;
		std::string startChars = POTIONS[idx];
		char minPotion = 127;
		DWORD minItemId = 0;
		bool isBelt = false;
		for (UnitAny *pItem = unit->pInventory->pFirstItem; pItem; pItem = pItem->pItemData->pNextInvItem) {
			if (pItem->pItemData->ItemLocation == STORAGE_INVENTORY ||
				pItem->pItemData->ItemLocation == STORAGE_NULL && pItem->pItemData->NodePage == NODEPAGE_BELTSLOTS) {
				char* code = D2COMMON_GetItemText(pItem->dwTxtFileNo)->szCode;
				if (code[0] == startChars[0] && code[1] == startChars[1] && code[2] < minPotion) {
					minPotion = code[2];
					minItemId = pItem->dwUnitId;
					isBelt = pItem->pItemData->NodePage == NODEPAGE_BELTSLOTS;
				}
			}
			//char *code = D2COMMON_GetItemText(pItem->dwTxtFileNo)->szCode;
			//if (code[0] == 'b' && code[1] == 'o' && code[2] == 'x') {
			//	// Hack to pick up cube to fix cube-in-cube problem
			//	BYTE PacketDataCube[5] = {0x19,0,0,0,0};
			//	*reinterpret_cast<int*>(PacketDataCube + 1) = pItem->dwUnitId;
			//	D2NET_SendPacket(5, 1, PacketDataCube);
			//	break;
			//}
		}
		if (minItemId > 0) {
			if (isBelt){
				BYTE PacketData[13] = { 0x26, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
				*reinterpret_cast<int*>(PacketData + 1) = minItemId;
				D2NET_SendPacket(13, 0, PacketData);
			}
			else{
				//PrintText(1, "Sending packet %d, %d, %d", minItemId, unit->pPath->xPos, unit->pPath->yPos);
				BYTE PacketData[13] = { 0x20, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
				*reinterpret_cast<int*>(PacketData + 1) = minItemId;
				*reinterpret_cast<WORD*>(PacketData + 5) = (WORD)unit->pPath->xPos;
				*reinterpret_cast<WORD*>(PacketData + 9) = (WORD)unit->pPath->yPos;
				D2NET_SendPacket(13, 0, PacketData);
			}
			*block = true;
		}
	}
	if (!up && (key == TpKey || key == TpBackKey)) {
		AutoBackTown = false;
		if (key == TpBackKey) {
			AutoBackTown = true;
		}
		DWORD tpId = 0;
		int tp_quantity = 0;
		for (UnitAny *pItem = unit->pInventory->pFirstItem; pItem; pItem = pItem->pItemData->pNextInvItem) {
			if (pItem->pItemData->ItemLocation == STORAGE_INVENTORY || pItem->pItemData->ItemLocation == STORAGE_CUBE || pItem->pItemData->ItemLocation == STORAGE_STASH) {
				char* code = D2COMMON_GetItemText(pItem->dwTxtFileNo)->szCode;
				if (code[0] == 't' && code[1] == 'b' && code[2] =='k') {
					tp_quantity = D2COMMON_GetUnitStat(pItem, STAT_AMMOQUANTITY, 0);
					if (tp_quantity > 0) {
						tpId = pItem->dwUnitId;
						break;
					}
				}
			}
		}
		if (tpId > 0) {
			BYTE PacketData[13] = { 0x20, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
			*reinterpret_cast<int*>(PacketData + 1) = tpId;
			*reinterpret_cast<WORD*>(PacketData + 5) = (WORD)unit->pPath->xPos;
			*reinterpret_cast<WORD*>(PacketData + 9) = (WORD)unit->pPath->yPos;
			if (tp_quantity < tp_warn_quantity) {
				PrintText(Red, "TP tome is running low!");
			}
			D2NET_SendPacket(13, 0, PacketData);
			*block = true;
		}
	}
	if (!up && key == ExitGameKey) {
		*p_D2CLIENT_ExitAppFlag = 0;
		SendMessage(D2GFX_GetHwnd(), WM_CLOSE, 0, 0);
		*block = true;
	}
	if (!up && key == BeltKey) {  //�Զ��������
		AutoToBelt();
	}
	if (!up && (key == AutoPickupKey))
	{
		AutoPickupOn = !AutoPickupOn;
		PrintText(Blue, "�Զ�ʰȡ���%s.", AutoPickupOn ? "����" : "�ر�");
	}
}

void ItemMover::OnGamePacketRecv(BYTE* packet, bool* block) {
	switch (packet[0])
	{
	case 0x3F:
		{
			// We get this packet after our cursor change. Will only ID if we found book and item previously. packet[1] = 0 guarantees the cursor is changing to "id ready" state.
			if (packet[1] == 0 && idBookId > 0 && unidItemId > 0) {
				BYTE PacketData[9] = {0x27,0,0,0,0,0,0,0,0};
				*reinterpret_cast<int*>(PacketData + 1) = unidItemId;
				*reinterpret_cast<int*>(PacketData + 5) = idBookId;
				D2NET_SendPacket(9, 0, PacketData);
				*block = true;
				// Reseting variables after we ID an item so the next ID works.
				unidItemId = 0;
				idBookId = 0;
			}
			break;
		}
	case 0x9c:
		{
			// We get this packet after placing an item in a container or on the ground
			if (FirstInit) {
				BYTE action = packet[1];
				unsigned int itemId = *(unsigned int*)&packet[4];
				Lock();
				if (itemId == ActivePacket.itemId) {
					//PrintText(1, "Placed item id %d", itemId);
					ActivePacket.itemId = 0;
					ActivePacket.x = 0;
					ActivePacket.y = 0;
					ActivePacket.startTicks = 0;
					ActivePacket.destination = 0;
				}
				Unlock();
			}

			if ((*BH::MiscToggles2)["Advanced Item Display"].state) {
				bool success = true;
				ItemInfo item = {};
				ParseItem((unsigned char*)packet, &item, &success);
				//PrintText(1, "Item packet: %s, %s, %X, %d, %d", item.name.c_str(), item.code, item.attrs->flags, item.sockets, GetDefense(&item));
				if ((item.action == ITEM_ACTION_NEW_GROUND || item.action == ITEM_ACTION_OLD_GROUND) && success) {
					bool showOnMap = false;
					bool nameWhitelisted = false;
					bool noTracking = false;
					auto pingLevel = -1;
					auto color = UNDEFINED_COLOR;

					for (vector<Rule*>::iterator it = MapRuleList.begin(); it != MapRuleList.end(); it++) {
						if ((*it)->Evaluate(NULL, &item)) {
							nameWhitelisted = true;
							// skip map and notification if ping level requirement is not met
							if ((*it)->action.pingLevel > Item::GetPingLevel()) continue;
							auto action_color = (*it)->action.notifyColor;
							// never overwrite color with an undefined color. never overwrite a defined color with dead color.
							if (action_color != UNDEFINED_COLOR && (action_color != DEAD_COLOR || color == UNDEFINED_COLOR))
								color = action_color;
							showOnMap = true;
							noTracking = (*it)->action.noTracking;
							pingLevel = (*it)->action.pingLevel;
							// break unless %CONTINUE% is used
							if ((*it)->action.stopProcessing) break;
						}
					}
					// Don't block items that have a white-listed name
					for (vector<Rule*>::iterator it = DoNotBlockRuleList.begin(); it != DoNotBlockRuleList.end(); it++) {
						if ((*it)->Evaluate(NULL, &item)) {
							nameWhitelisted = true;
							break;
						}
					}
					//PrintText(1, "Item on ground: %s, %s, %s, %X", item.name.c_str(), item.code, item.attrs->category.c_str(), item.attrs->flags);
					if(showOnMap && !(*BH::MiscToggles2)["Item Detailed Notifications"].state) {
						if (!noTracking && !IsTown(GetPlayerArea()) && pingLevel <= Item::GetTrackerPingLevel()) {
							ScreenInfo::AddDrop(item.name.c_str(), item.x, item.y);
						}
						if (color == UNDEFINED_COLOR) {
							color = ItemColorFromQuality(item.quality);
						}
						if ((*BH::MiscToggles2)["Item Drop Notifications"].state &&
								item.action == ITEM_ACTION_NEW_GROUND &&
								color != DEAD_COLOR
							 ) {
							PrintText(color, "%s%s",
									item.name.c_str(),
									(*BH::MiscToggles2)["Verbose Notifications"].state ? " \377c5drop" : ""
									);
						}
						if ((*BH::MiscToggles2)["Item Close Notifications"].state &&
								item.action == ITEM_ACTION_OLD_GROUND &&
								color != DEAD_COLOR
							 ) {
							PrintText(color, "%s%s",
									item.name.c_str(),
									(*BH::MiscToggles2)["Verbose Notifications"].state ? " \377c5close" : ""
									);
						}
					}
					else if (!showOnMap && !nameWhitelisted) {
						for (vector<Rule*>::iterator it = IgnoreRuleList.begin(); it != IgnoreRuleList.end(); it++) {
							if ((*it)->Evaluate(NULL, &item)) {
								*block = true;
								//PrintText(1, "Blocking item: %s, %s, %d", item.name.c_str(), item.code, item.amount);
								break;
							}
						}
					}
				}
			}
			break;
		}
	case 0x9d:
		{
			// We get this packet after picking up an item
			if (FirstInit) {
				BYTE action = packet[1];
				unsigned int itemId = *(unsigned int*)&packet[4];
				Lock();
				if (itemId == ActivePacket.itemId) {
					//PrintText(2, "Picked up item id %d", itemId);
					if (ActivePacket.destination == STORAGE_NULL) {
						PutItemOnGround();
					} else {
						PutItemInContainer();
					}
				}
				Unlock();
			}
			break;
		}
	case 0x60:  //by zyl �Զ��س�
	{
		if (!AutoBackTown) return;   //���Զ��سǾ�ֱ�ӷ���
		if (packet[1] == 0x00) { //00�޶�ֻ����ͨ�����Ĵ�����
			BYTE castMove[9] = { 0x13 };
			*(DWORD*)&castMove[1] = 2;
			*(DWORD*)&castMove[5] = *(DWORD*)&packet[3]; // portal ID
			D2NET_SendPacket(sizeof(castMove), 0, castMove);
		}
	}
	default:
		break;
	}
	return;
}

void ItemMover::OnGameExit() {
	ActivePacket.itemId = 0;
	ActivePacket.x = 0;
	ActivePacket.y = 0;
	ActivePacket.startTicks = 0;
	ActivePacket.destination = 0;
}

// Code for reading the 0x9c bitstream (borrowed from heroin_glands)
void ParseItem(const unsigned char *data, ItemInfo *item, bool *success) {
	*success = true;
	try {
		BitReader reader(data);
		unsigned long packet = reader.read(8);
		item->action = reader.read(8);
		unsigned long messageSize = reader.read(8);
		item->category = reader.read(8); // item type
		item->id = reader.read(32);

		if (packet == 0x9d) {
			reader.read(32);
			reader.read(8);
		}

		item->equipped = reader.readBool();
		reader.readBool();
		reader.readBool();
		item->inSocket = reader.readBool();
		item->identified = reader.readBool();
		reader.readBool();
		item->switchedIn = reader.readBool();
		item->switchedOut = reader.readBool();

		item->broken = reader.readBool();
		reader.readBool();
		item->potion = reader.readBool();
		item->hasSockets = reader.readBool();
		reader.readBool();
		item->inStore = reader.readBool();
		item->notInSocket = reader.readBool();
		reader.readBool();

		item->ear = reader.readBool();
		item->startItem = reader.readBool();
		reader.readBool();
		reader.readBool();
		reader.readBool();
		item->simpleItem = reader.readBool();
		item->ethereal = reader.readBool();
		reader.readBool();

		item->personalized = reader.readBool();
		item->gambling = reader.readBool();
		item->runeword = reader.readBool();
		reader.read(5);

		item->version = static_cast<unsigned int>(reader.read(8));

		reader.read(2);
		unsigned long destination = reader.read(3);

		item->ground = (destination == 0x03);

		if (item->ground) {
			item->x = reader.read(16);
			item->y = reader.read(16);
		} else {
			item->directory = reader.read(4);
			item->x = reader.read(4);
			item->y = reader.read(3);
			item->container = static_cast<unsigned int>(reader.read(4));
		}

		item->unspecifiedDirectory = false;

		if (item->action == ITEM_ACTION_TO_STORE || item->action == ITEM_ACTION_FROM_STORE) {
			long container = static_cast<long>(item->container);
			container |= 0x80;
			if (container & 1) {
				container--; //remove first bit
				item->y += 8;
			}
			item->container = static_cast<unsigned int>(container);
		} else if (item->container == CONTAINER_UNSPECIFIED) {
			if (item->directory == EQUIP_NONE) {
				if (item->inSocket) {
					//y is ignored for this container type, x tells you the index
					item->container = CONTAINER_ITEM;
				} else if (item->action == ITEM_ACTION_PLACE_BELT || item->action == ITEM_ACTION_REMOVE_BELT) {
					item->container = CONTAINER_BELT;
					item->y = item->x / 4;
					item->x %= 4;
				}
			} else {
				item->unspecifiedDirectory = true;
			}
		}

		if (item->ear) {
			item->earClass = static_cast<BYTE>(reader.read(3));
			item->earLevel = reader.read(7);
			item->code[0] = 'e';
			item->code[1] = 'a';
			item->code[2] = 'r';
			item->code[3] = 0;
			for (std::size_t i = 0; i < 16; i++) {
				char letter = static_cast<char>(reader.read(7));
				if (letter == 0) {
					break;
				}
				item->earName.push_back(letter);
			}
			item->attrs = ItemAttributeMap[item->code];
			item->name = item->attrs->name;
			item->width = item->attrs->width;
			item->height = item->attrs->height;
			//PrintText(1, "Ear packet: %s, %s, %d, %d", item->earName.c_str(), item->code, item->earClass, item->earLevel);
			return;
		}

		for (std::size_t i = 0; i < 4; i++) {
			item->code[i] = static_cast<char>(reader.read(8));
		}
		item->code[3] = 0;

		if (ItemAttributeMap.find(item->code) == ItemAttributeMap.end()) {
			HandleUnknownItemCode(item->code, "from packet");
			*success = false;
			return;
		}
		item->attrs = ItemAttributeMap[item->code];
		item->name = item->attrs->name;
		item->width = item->attrs->width;
		item->height = item->attrs->height;

		item->isGold = (item->code[0] == 'g' && item->code[1] == 'l' && item->code[2] == 'd');

		if (item->isGold) {
			bool big_pile = reader.readBool();
			if (big_pile) {
				item->amount = reader.read(32);
			} else {
				item->amount = reader.read(12);
			}
			return;
		}

		item->usedSockets = (BYTE)reader.read(3);

		if (item->simpleItem || item->gambling) {
			return;
		}

		item->level = (BYTE)reader.read(7);
		item->quality = static_cast<unsigned int>(reader.read(4));

		item->hasGraphic = reader.readBool();;
		if (item->hasGraphic) {
			item->graphic = reader.read(3);
		}

		item->hasColor = reader.readBool();
		if (item->hasColor) {
			item->color = reader.read(11);
		}

		if (item->identified) {
			switch(item->quality) {
			case ITEM_QUALITY_INFERIOR:
				item->prefix = reader.read(3);
				break;

			case ITEM_QUALITY_SUPERIOR:
				item->superiority = static_cast<unsigned int>(reader.read(3));
				break;

			case ITEM_QUALITY_MAGIC:
				item->prefix = reader.read(11);
				item->suffix = reader.read(11);
				break;

			case ITEM_QUALITY_CRAFT:
			case ITEM_QUALITY_RARE:
				item->prefix = reader.read(8) - 156;
				item->suffix = reader.read(8) - 1;
				break;

			case ITEM_QUALITY_SET:
				item->setCode = reader.read(12);
				break;

			case ITEM_QUALITY_UNIQUE:
				if (item->code[0] != 's' || item->code[1] != 't' || item->code[2] != 'd') { //standard of heroes exception?
					item->uniqueCode = reader.read(12);
				}
				break;
			}
		}

		if (item->quality == ITEM_QUALITY_RARE || item->quality == ITEM_QUALITY_CRAFT) {
			for (unsigned long i = 0; i < 3; i++) {
				if (reader.readBool()) {
					item->prefixes.push_back(reader.read(11));
				}
				if (reader.readBool()) {
					item->suffixes.push_back(reader.read(11));
				}
			}
		}

		if (item->runeword) {
			item->runewordId = reader.read(12);
			item->runewordParameter = reader.read(4);
		}

		if (item->personalized) {
			for (std::size_t i = 0; i < 16; i++) {
				char letter = static_cast<char>(reader.read(7));
				if (letter == 0) {
					break;
				}
				item->personalizedName.push_back(letter);
			}
			//PrintText(1, "Personalized packet: %s, %s", item->personalizedName.c_str(), item->code);
		}

		item->isArmor = (item->attrs->flags & ITEM_GROUP_ALLARMOR) > 0;
		item->isWeapon = (item->attrs->flags & ITEM_GROUP_ALLWEAPON) > 0;

		if (item->isArmor) {
			item->defense = reader.read(11) - 10;
		}

		/*if(entry.throwable)
		{
			reader.read(9);
			reader.read(17);
		} else */
		//special case: indestructible phase blade
		if (item->code[0] == '7' && item->code[1] == 'c' && item->code[2] == 'r') {
			reader.read(8);
		} else if (item->isArmor || item->isWeapon) {
			item->maxDurability = reader.read(8);
			item->indestructible = item->maxDurability == 0;
			/*if (!item->indestructible) {
				item->durability = reader.read(8);
				reader.readBool();
			}*/
			//D2Hackit always reads it, hmmm. Appears to work.
			item->durability = reader.read(8);
			reader.readBool();
		}

		if (item->hasSockets) {
			item->sockets = (BYTE)reader.read(4);
		}

		if (!item->identified) {
			return;
		}

		if (item->attrs->stackable) {
			if (item->attrs->useable) {
				reader.read(5);
			}
			item->amount = reader.read(9);
		}

		if (item->quality == ITEM_QUALITY_SET) {
			unsigned long set_mods = reader.read(5);
		}

		while (true) {
			unsigned long stat_id = reader.read(9);
			if (stat_id == 0x1ff) {
				break;
			}
			ItemProperty prop = {};
			if (!ProcessStat(stat_id, reader, prop) &&
					!(*BH::MiscToggles2)["Suppress Invalid Stats"].state) {
				PrintText(1, "Invalid stat: %d, %c%c%c", stat_id, item->code[0], item->code[1], item->code[2]);
				*success = false;
				break;
			}
			item->properties.push_back(prop);
		}
	} catch (int e) {
		PrintText(1, "Int exception parsing item: %c%c%c, %d", item->code[0], item->code[1], item->code[2], e);
	} catch (std::exception const & ex) {
		PrintText(1, "Exception parsing item: %c%c%c, %s", item->code[0], item->code[1], item->code[2], ex.what());
	} catch(...) {
		PrintText(1, "Miscellaneous exception parsing item: %c%c%c", item->code[0], item->code[1], item->code[2]);
		*success = false;
	}
	return;
}

bool ProcessStat(unsigned int stat, BitReader &reader, ItemProperty &itemProp) {
	if (stat > STAT_MAX) {
		return false;
	}

	StatProperties *bits = GetStatProperties(stat);
	unsigned int saveBits = bits->saveBits;
	unsigned int saveParamBits = bits->saveParamBits;
	unsigned int saveAdd = bits->saveAdd;
	itemProp.stat = stat;

	if (saveParamBits > 0) {
		switch (stat) {
			case STAT_CLASSSKILLS:
			{
				itemProp.characterClass = reader.read(saveParamBits);
				itemProp.value = reader.read(saveBits);
				return true;
			}
			case STAT_NONCLASSSKILL:
			case STAT_SINGLESKILL:
			{
				itemProp.skill = reader.read(saveParamBits);
				itemProp.value = reader.read(saveBits);
				return true;
			}
			case STAT_ELEMENTALSKILLS:
			{
				ulong element = reader.read(saveParamBits);
				itemProp.value = reader.read(saveBits);
				return true;
			}
			case STAT_AURA:
			{
				itemProp.skill = reader.read(saveParamBits);
				itemProp.value = reader.read(saveBits);
				return true;
			}
			case STAT_REANIMATE:
			{
				itemProp.monster = reader.read(saveParamBits);
				itemProp.value = reader.read(saveBits);
				return true;
			}
			case STAT_SKILLTAB:
			{
				itemProp.tab = reader.read(3);
				itemProp.characterClass = reader.read(3);
				ulong unknown = reader.read(10);
				itemProp.value = reader.read(saveBits);
				return true;
			}
			case STAT_SKILLONDEATH:
			case STAT_SKILLONHIT:
			case STAT_SKILLONKILL:
			case STAT_SKILLONLEVELUP:
			case STAT_SKILLONSTRIKING:
			case STAT_SKILLWHENSTRUCK:
			{
				itemProp.level = reader.read(6);
				itemProp.skill = reader.read(10);
				itemProp.skillChance = reader.read(saveBits);
				return true;
			}
			case STAT_CHARGED:
			{
				itemProp.level = reader.read(6);
				itemProp.skill = reader.read(10);
				itemProp.charges = reader.read(8);
				itemProp.maximumCharges = reader.read(8);
				return true;
			}
			case STAT_STATE:
			case STAT_ATTCKRTNGVSMONSTERTYPE:
			case STAT_DAMAGETOMONSTERTYPE:
			{
				// For some reason heroin_glands doesn't read these, even though
				// they have saveParamBits; maybe they don't occur in practice?
				itemProp.value = reader.read(saveBits) - saveAdd;
				return true;
			}
			default:
				reader.read(saveParamBits);
				reader.read(saveBits);
				return true;
		}
	}

	if (bits->op >= 2 && bits->op <= 5) {
		itemProp.perLevel = reader.read(saveBits);
		return true;
	}

	switch (stat) {
		case STAT_ENHANCEDMAXIMUMDAMAGE:
		case STAT_ENHANCEDMINIMUMDAMAGE:
		{
			itemProp.minimum = reader.read(saveBits);
			itemProp.maximum = reader.read(saveBits);
			return true;
		}
		case STAT_MINIMUMFIREDAMAGE:
		{
			itemProp.minimum = reader.read(saveBits);
			itemProp.maximum = reader.read(GetStatProperties(STAT_MAXIMUMFIREDAMAGE)->saveBits);
			return true;
		}
		case STAT_MINIMUMLIGHTNINGDAMAGE:
		{
			itemProp.minimum = reader.read(saveBits);
			itemProp.maximum = reader.read(GetStatProperties(STAT_MAXIMUMLIGHTNINGDAMAGE)->saveBits);
			return true;
		}
		case STAT_MINIMUMMAGICALDAMAGE:
		{
			itemProp.minimum = reader.read(saveBits);
			itemProp.maximum = reader.read(GetStatProperties(STAT_MAXIMUMMAGICALDAMAGE)->saveBits);
			return true;
		}
		case STAT_MINIMUMCOLDDAMAGE:
		{
			itemProp.minimum = reader.read(saveBits);
			itemProp.maximum = reader.read(GetStatProperties(STAT_MAXIMUMCOLDDAMAGE)->saveBits);
			itemProp.length = reader.read(GetStatProperties(STAT_COLDDAMAGELENGTH)->saveBits);
			return true;
		}
		case STAT_MINIMUMPOISONDAMAGE:
		{
			itemProp.minimum = reader.read(saveBits);
			itemProp.maximum = reader.read(GetStatProperties(STAT_MAXIMUMPOISONDAMAGE)->saveBits);
			itemProp.length = reader.read(GetStatProperties(STAT_POISONDAMAGELENGTH)->saveBits);
			return true;
		}
		case STAT_REPAIRSDURABILITY:
		case STAT_REPLENISHESQUANTITY:
		{
			itemProp.value = reader.read(saveBits);
			return true;
		}
		default:
		{
			itemProp.value = reader.read(saveBits) - saveAdd;
			return true;
		}
	}
}

//�Զ�ʰȡ����
// ��֡�����㣬����ʱ�䲻�Ǻ�׼ȷ
#define CHARGING_CD 100
#define AUTO_PICKUP_GOLD_RANGE 5
DWORD GetDistance(LONG x1, LONG y1, LONG x2, LONG y2)
{
	return (DWORD)::sqrt((double)(((int)x1 - (int)x2) * ((int)x1 - (int)x2) + ((int)y1 - (int)y2) * ((int)y1 - (int)y2)));
}

DWORD GetItemMapDistanceFrom(UnitAny* player, UnitAny* item) // Player's distance from a position
{
	return GetDistance(player->pPath->xPos, player->pPath->yPos, item->pItemPath->dwPosX, item->pItemPath->dwPosY);
}

void ItemMover::OnLoop()
{
	AutoPickupGold(AUTO_PICKUP_GOLD_RANGE);
}

void ItemMover::AutoPickupGold(DWORD range)
{
	if (!AutoPickupOn)
		return;
	UnitAny* unit = D2CLIENT_GetPlayerUnit();
	DWORD max_gold_amount = D2COMMON_GetUnitStat(unit, STAT_LEVEL, 0) * 10000;
	DWORD goldstash = D2COMMON_GetUnitStat(unit, STAT_GOLD, 0);
	if (goldstash >= max_gold_amount)
		return;

	Room1* room1 = unit->pPath->pRoom1;
	if (!room1 || !room1->pRoom2)
		return;
	for (DWORD i = 0; i < room1->dwRoomsNear; i++)
	{
		for (UnitAny* pUnit = room1->pRoomsNear[i]->pUnitFirst; pUnit; pUnit = pUnit->pListNext)
		{
			if (pUnit && pUnit->dwType == UNIT_ITEM)
			{
				char* code = D2COMMON_GetItemText(pUnit->dwTxtFileNo)->szCode;
				std::string szCode;
				szCode += code[0];
				szCode += code[1];
				szCode += code[2];
				DWORD dis = GetItemMapDistanceFrom(unit, pUnit);
				if (Auto_toPickupItems.find(szCode) != Auto_toPickupItems.end() && dis < range)
				{
					BYTE aPacket[13] = { 0 };
					aPacket[0] = 0x16;
					aPacket[1] = 0x04;
					aPacket[12] = 0;
					::memcpy(aPacket + 5, &pUnit->dwUnitId, 4);
					D2NET_SendPacket(13, 0, aPacket);
				}
			}
		}
	}
}

void ItemMover::ResetPacket()
{
	ActivePacket.itemId = 0;
	ActivePacket.x = 0;
	ActivePacket.y = 0;
	ActivePacket.startTicks = 0;
	ActivePacket.destination = 0;
}