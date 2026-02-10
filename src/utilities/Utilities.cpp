#define STB_IMAGE_IMPLEMENTATION
#include "../Includes.h"

void CUtilities::Sleep(float flMilliseconds)
{
	static tZwSetTimerResolution ZwSetTimerResolution = reinterpret_cast<tZwSetTimerResolution>(g_Memory.GetImport(NTDLL_DLL, X("ZwSetTimerResolution")));
	static tNtDelayExecution NtDelayExecution = reinterpret_cast<tNtDelayExecution>(g_Memory.GetImport(NTDLL_DLL,  X("NtDelayExecution")));

	static std::once_flag flag;
	std::call_once(flag, []()
	{
		ULONG uCurrent;
		ZwSetTimerResolution(static_cast<ULONG>(0.5f * 10000.f), true, &uCurrent);
	});

	if (flMilliseconds < 0.5f)
		flMilliseconds = 0.5f;

	LARGE_INTEGER time{};
	time.QuadPart = -1 * static_cast<LONGLONG>(flMilliseconds * 10000.f);
	NtDelayExecution(false, &time);
}

bool CUtilities::IsVisible(C_CSPlayerPawn* pPlayer, Vector vecStart, Vector vecPosition)
{
	if (!pPlayer || vecPosition.IsZero())
	{
		Logger::Log("[IsVisible] Invalid parameters: pPlayer=" + std::to_string(reinterpret_cast<uintptr_t>(pPlayer)) + ", vecPosition.IsZero()=" + std::to_string(vecPosition.IsZero()));
		return false;
	}

	// Fix: Use EntryIndex for bitmask, not pointer
	int localIndex = g_Globals.m_LocalPlayer.m_pPlayerPawn ? g_Globals.m_LocalPlayer.m_pPlayerPawn->GetRefEHandle().GetEntryIndex() : 0;
	const bool bSpotted = pPlayer->m_entitySpottedState().m_bSpottedByMask[0] & (1 << localIndex);
	
	if (!g_MapParser.m_bSetup)
	{
		Logger::Log("[IsVisible] MapParser not setup, using spotted check: bSpotted=" + std::to_string(bSpotted) + ", localIndex=" + std::to_string(localIndex));
		return bSpotted;
	}

	bool bMapVisible = g_MapParser.IsVisible(vecStart, vecPosition);
	Logger::Log("[IsVisible] MapParser setup, using map check: bMapVisible=" + std::to_string(bMapVisible));
	return bMapVisible;
}

bool CUtilities::IsChangingLevel()
{
	return g_Interfaces.m_NetworkGameClient.m_nSignonState == SignonState_t::SIGNONSTATE_CHANGELEVEL;
}

bool CUtilities::IsInGame()
{
	return g_Interfaces.m_NetworkGameClient.m_nSignonState == SignonState_t::SIGNONSTATE_FULL;
}

bool CUtilities::IsConnected()
{
	return g_Interfaces.m_NetworkGameClient.m_nSignonState >= SignonState_t::SIGNONSTATE_CONNECTED;
}

void CUtilities::MouseMove(int iDeltaX, int iDeltaY)
{
	if (iDeltaX == 0 && iDeltaY == 0)
		return;

	INPUT input = { 0 };
	input.type = INPUT_MOUSE;
	input.mi.dwFlags = MOUSEEVENTF_MOVE;
	input.mi.dx = iDeltaX;
	input.mi.dy = iDeltaY;
	SendInput(1, &input, sizeof(INPUT));
}

ImTextureID CUtilities::LoadImageTexture(const char* szFilePath)
{
	if (!Window::m_pDevice)
		return nullptr;

	// Try to load the image
	int iWidth = 0, iHeight = 0, iChannels = 0;
	unsigned char* pImageData = stbi_load(szFilePath, &iWidth, &iHeight, &iChannels, 4);
	
	if (!pImageData)
	{
		// Image file not found or couldn't be loaded
		return nullptr;
	}

	// Validate dimensions
	if (iWidth <= 0 || iHeight <= 0)
	{
		stbi_image_free(pImageData);
		return nullptr;
	}

	ID3D11Texture2D* pTexture = nullptr;
	ID3D11ShaderResourceView* pSRV = nullptr;

	D3D11_TEXTURE2D_DESC desc = {};
	desc.Width = static_cast<UINT>(iWidth);
	desc.Height = static_cast<UINT>(iHeight);
	desc.MipLevels = 1;
	desc.ArraySize = 1;
	desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	desc.SampleDesc.Count = 1;
	desc.SampleDesc.Quality = 0;
	desc.Usage = D3D11_USAGE_DEFAULT;
	desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	desc.CPUAccessFlags = 0;
	desc.MiscFlags = 0;

	D3D11_SUBRESOURCE_DATA subResource = {};
	subResource.pSysMem = pImageData;
	subResource.SysMemPitch = desc.Width * 4; // 4 bytes per pixel (RGBA)
	subResource.SysMemSlicePitch = 0;

	HRESULT hr = Window::m_pDevice->CreateTexture2D(&desc, &subResource, &pTexture);
	if (FAILED(hr))
	{
		stbi_image_free(pImageData);
		return nullptr;
	}

	D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MipLevels = desc.MipLevels;
	srvDesc.Texture2D.MostDetailedMip = 0;

	hr = Window::m_pDevice->CreateShaderResourceView(pTexture, &srvDesc, &pSRV);
	if (FAILED(hr))
	{
		pTexture->Release();
		stbi_image_free(pImageData);
		return nullptr;
	}

	// Release texture reference (SRV holds its own reference)
	pTexture->Release();
	stbi_image_free(pImageData);

	return reinterpret_cast<ImTextureID>(pSRV);
}