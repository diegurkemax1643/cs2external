#include "Includes.h"

bool CInterfaces::Update()
{
	m_GlobalVars = g_Memory.ReadMemory<CGlobalVars>(g_Memory.ReadMemory<std::uintptr_t>(g_Globals.m_Offsets.m_uGlobalVars));
	m_CSGOInput = g_Memory.ReadMemory<CCSGOInput>(g_Memory.ReadMemory<std::uintptr_t>(g_Globals.m_Offsets.m_uCSGOInput));
	m_NetworkGameClient = g_Memory.ReadMemory<CNetWorkGameClient>(g_Memory.ReadMemory<std::uintptr_t>(g_Globals.m_Offsets.m_uNetworkGameClient));
	m_GameEntitySystem = g_Memory.ReadMemory<CGameEntitySystem>(g_Memory.ReadMemory<std::uintptr_t>(g_Globals.m_Offsets.m_uEntitySystem));
	return true;
}

bool CGlobals::Update()
{
	static std::once_flag flag;
	std::call_once(flag, []()
	{
		// Get module base addresses
		ModuleInformation_t clientModule = g_Memory.GetModule(CLIENT_DLL);
		ModuleInformation_t engine2Module = g_Memory.GetModule(ENGINE2_DLL);
		
		// Updated offsets from https://github.com/sezzyaep/CS2-OFFSETS (2026-01-30)
		// These are RVA offsets that need to be added to module base address
		g_Globals.m_Offsets.m_uEntityList = clientModule.m_uBaseAddress + 0x24A7B28;        // dwEntityList
		g_Globals.m_Offsets.m_uViewMatrix = clientModule.m_uBaseAddress + 0x2308860;        // dwViewMatrix
		g_Globals.m_Offsets.m_uLocalPlayerController = clientModule.m_uBaseAddress + 0x22ECA28; // dwLocalPlayerController
		g_Globals.m_Offsets.m_uPlantedC4 = clientModule.m_uBaseAddress + 0x230FCF0;        // dwPlantedC4
		g_Globals.m_Offsets.m_uAutoAcceptArray = 0U; // Not in offset dump, keeping as 0 for now
		
		g_Globals.m_Offsets.m_uGlobalVars = clientModule.m_uBaseAddress + 0x20572A8;        // dwGlobalVars
		g_Globals.m_Offsets.m_uCSGOInput = clientModule.m_uBaseAddress + 0x2312550;         // dwCSGOInput
		g_Globals.m_Offsets.m_uNetworkGameClient = engine2Module.m_uBaseAddress + 0x900FF0; // dwNetworkGameClient
		g_Globals.m_Offsets.m_uEntitySystem = clientModule.m_uBaseAddress + 0x24A7B28;      // dwGameEntitySystem (same as EntityList)
		g_Globals.m_Offsets.m_uSensitivity = clientModule.m_uBaseAddress + 0x2304128;     // dwSensitivity
	});

	// Read pointers from offset addresses (offsets point to where the actual pointers are stored)
	g_Globals.m_uEntityList = g_Memory.ReadMemory<std::uintptr_t>(g_Globals.m_Offsets.m_uEntityList);
	g_Globals.m_LocalPlayer.m_pController = g_Memory.ReadMemory<CCSPlayerController*>(g_Globals.m_Offsets.m_uLocalPlayerController);
	
	// Get player pawn from controller
	if (g_Globals.m_LocalPlayer.m_pController)
		g_Globals.m_LocalPlayer.m_pPlayerPawn = reinterpret_cast<C_CSPlayerPawn*>(g_Globals.m_LocalPlayer.m_pController->m_hPawn().Get());
	
	// View matrix is read directly from the offset address
	g_Globals.m_matViewMatrix = g_Memory.ReadMemory<ViewMatrix_t>(g_Globals.m_Offsets.m_uViewMatrix);

	return true;
}