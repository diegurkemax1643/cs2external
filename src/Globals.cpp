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
		
		// Try to load cached offsets first (fastest)
		COffsetUpdater::OffsetData cachedOffsets;
		bool bUseCached = g_OffsetUpdater.LoadCachedOffsets(cachedOffsets) && cachedOffsets.m_uEntityList != 0;
		
		// Try to fetch latest offsets in background (non-blocking)
		std::thread([&]() {
			COffsetUpdater::OffsetData fetchedOffsets;
			if (g_OffsetUpdater.FetchLatestOffsets(fetchedOffsets))
			{
				// Offsets fetched and cached, will be used on next run
			}
		}).detach();
		
		// If we have cached offsets, use them
		if (bUseCached)
		{
			g_Globals.m_Offsets.m_uEntityList = clientModule.m_uBaseAddress + cachedOffsets.m_uEntityList;
			g_Globals.m_Offsets.m_uViewMatrix = clientModule.m_uBaseAddress + cachedOffsets.m_uViewMatrix;
			g_Globals.m_Offsets.m_uLocalPlayerController = clientModule.m_uBaseAddress + cachedOffsets.m_uLocalPlayerController;
			g_Globals.m_Offsets.m_uPlantedC4 = clientModule.m_uBaseAddress + cachedOffsets.m_uPlantedC4;
			g_Globals.m_Offsets.m_uGlobalVars = clientModule.m_uBaseAddress + cachedOffsets.m_uGlobalVars;
			g_Globals.m_Offsets.m_uCSGOInput = clientModule.m_uBaseAddress + cachedOffsets.m_uCSGOInput;
			g_Globals.m_Offsets.m_uNetworkGameClient = engine2Module.m_uBaseAddress + cachedOffsets.m_uNetworkGameClient;
			g_Globals.m_Offsets.m_uEntitySystem = clientModule.m_uBaseAddress + cachedOffsets.m_uEntitySystem;
			g_Globals.m_Offsets.m_uSensitivity = clientModule.m_uBaseAddress + cachedOffsets.m_uSensitivity;
		}
		else
		{
			// Try to load from local cs2-dumper output (user-provided dump)
			COffsetUpdater::OffsetData localOffsets;
			if (g_OffsetUpdater.FetchFromLocalDump(localOffsets) && localOffsets.m_uEntityList != 0)
			{
				g_Globals.m_Offsets.m_uEntityList = clientModule.m_uBaseAddress + localOffsets.m_uEntityList;
				g_Globals.m_Offsets.m_uViewMatrix = clientModule.m_uBaseAddress + localOffsets.m_uViewMatrix;
				g_Globals.m_Offsets.m_uLocalPlayerController = clientModule.m_uBaseAddress + localOffsets.m_uLocalPlayerController;
				g_Globals.m_Offsets.m_uPlantedC4 = clientModule.m_uBaseAddress + localOffsets.m_uPlantedC4;
				g_Globals.m_Offsets.m_uGlobalVars = clientModule.m_uBaseAddress + localOffsets.m_uGlobalVars;
				g_Globals.m_Offsets.m_uCSGOInput = clientModule.m_uBaseAddress + localOffsets.m_uCSGOInput;
				g_Globals.m_Offsets.m_uNetworkGameClient = engine2Module.m_uBaseAddress + localOffsets.m_uNetworkGameClient;
				g_Globals.m_Offsets.m_uEntitySystem = clientModule.m_uBaseAddress + localOffsets.m_uEntitySystem;
				g_Globals.m_Offsets.m_uSensitivity = clientModule.m_uBaseAddress + localOffsets.m_uSensitivity;
			}
			else
			{
				// Fallback to hardcoded offsets (last known working offsets)
				// Updated offsets from https://github.com/a2x/cs2-dumper
				// Dump date: 2026-02-05 12:55:12 UTC (Fallback)
				// These are RVA offsets that need to be added to module base address
				g_Globals.m_Offsets.m_uEntityList = clientModule.m_uBaseAddress + 0x24ABF98;        // dwEntityList
				g_Globals.m_Offsets.m_uViewMatrix = clientModule.m_uBaseAddress + 0x230CC90;        // dwViewMatrix
				g_Globals.m_Offsets.m_uLocalPlayerController = clientModule.m_uBaseAddress + 0x22F0FB8; // dwLocalPlayerController
				g_Globals.m_Offsets.m_uPlantedC4 = clientModule.m_uBaseAddress + 0x2314820;        // dwPlantedC4
				g_Globals.m_Offsets.m_uGlobalVars = clientModule.m_uBaseAddress + 0x205B630;        // dwGlobalVars
				g_Globals.m_Offsets.m_uCSGOInput = clientModule.m_uBaseAddress + 0x2317080;         // dwCSGOInput
				g_Globals.m_Offsets.m_uNetworkGameClient = engine2Module.m_uBaseAddress + 0x905310; // dwNetworkGameClient
				g_Globals.m_Offsets.m_uEntitySystem = clientModule.m_uBaseAddress + 0x24ABF98;      // dwGameEntitySystem (same as EntityList)
				g_Globals.m_Offsets.m_uSensitivity = clientModule.m_uBaseAddress + 0x2308568;     // dwSensitivity
			}
		}
		
		g_Globals.m_Offsets.m_uAutoAcceptArray = 0U; // Not in offset dump, keeping as 0 for now
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