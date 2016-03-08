#include "SPIInstaller.h"

#define UNICODE
#define _UNICODE
#include <Ws2spi.h>
#include <Sporder.h> // ������WSCWriteProviderOrder����
#include <windows.h>
#include <stdio.h>
#pragma comment(lib, "Ws2_32.lib")
#pragma comment(lib, "Rpcrt4.lib") // ʵ����UuidCreate����

LPWSAPROTOCOL_INFOW GetAllProvider(int * pnProtocalsNumber)
{
	int nError = 0;
	DWORD dwNeedSize = 0;
	LPWSAPROTOCOL_INFOW pProtoInfo = NULL;

	// ȡ����Ҫ�ĳ���
	if (::WSCEnumProtocols(NULL, pProtoInfo, &dwNeedSize, &nError) == SOCKET_ERROR && WSAENOBUFS != nError)
		return NULL;

	pProtoInfo = (LPWSAPROTOCOL_INFOW)::GlobalAlloc(GPTR, dwNeedSize);

	*pnProtocalsNumber = ::WSCEnumProtocols(NULL, pProtoInfo, &dwNeedSize, &nError);

	return pProtoInfo;
}

void FreeProvider(LPWSAPROTOCOL_INFOW pProtoInfo)
{
	::GlobalFree(pProtoInfo);
}

bool SPIInstall(const wchar_t * pwszLSPProtocolName, const wchar_t *pwszPathName)
{
	int nError = 0;
	int nProtocols = 0;
	PDWORD pdwProtocalOrders = NULL;
	LPWSAPROTOCOL_INFOW pAllProtocolsInfo = NULL;

	int nArrayCount = 0;
	bool bIsSuccess = false;
	DWORD dwInstallCatalogID = 0;
	DWORD dwOrigCatalogId[3] = { 0 };
	WSAPROTOCOL_INFOW wpiLayeredProtocolInfo = { 0 };
	WSAPROTOCOL_INFOW wpiOriginalProtocolInfo[3] = { 0 };


	if (NULL == (pAllProtocolsInfo = GetAllProvider(&nProtocols)))//ö�����з�������ṩ��
		return FALSE;

	BOOL bFindTcp = FALSE;
	BOOL bFindUdp = FALSE;
	BOOL bFindRaw = FALSE;

	for (int i = 0; i < nProtocols; i++)	// �ҵ����ǵ��²�Э�飬����Ϣ����������
	{
		if (pAllProtocolsInfo[i].iAddressFamily != AF_INET)
			continue;

		if (3 == nArrayCount)
			break;

		switch (pAllProtocolsInfo[i].iProtocol)
		{
		case IPPROTO_IP:
		{
			if (bFindRaw)
				break;

			memcpy(&wpiOriginalProtocolInfo[nArrayCount], &pAllProtocolsInfo[i], sizeof(WSAPROTOCOL_INFOW));
			wpiOriginalProtocolInfo[nArrayCount].dwServiceFlags1 = wpiOriginalProtocolInfo[nArrayCount].dwServiceFlags1 & (~XP1_IFS_HANDLES);
			bFindRaw = TRUE;
			dwOrigCatalogId[nArrayCount++] = pAllProtocolsInfo[i].dwCatalogEntryId;

			break;
		}
		case IPPROTO_TCP:
		{
			if (bFindTcp)
				break;

			memcpy(&wpiOriginalProtocolInfo[nArrayCount], &pAllProtocolsInfo[i], sizeof(WSAPROTOCOL_INFOW));
			wpiOriginalProtocolInfo[nArrayCount].dwServiceFlags1 = wpiOriginalProtocolInfo[nArrayCount].dwServiceFlags1 & (~XP1_IFS_HANDLES);

			bFindTcp = TRUE;
			dwOrigCatalogId[nArrayCount++] = pAllProtocolsInfo[i].dwCatalogEntryId;

			break;
		}
		case IPPROTO_UDP:
		{
			if (bFindUdp)
				break;

			memcpy(&wpiOriginalProtocolInfo[nArrayCount], &pAllProtocolsInfo[i], sizeof(WSAPROTOCOL_INFOW));
			wpiOriginalProtocolInfo[nArrayCount].dwServiceFlags1 = wpiOriginalProtocolInfo[nArrayCount].dwServiceFlags1 & (~XP1_IFS_HANDLES);

			bFindUdp = TRUE;
			dwOrigCatalogId[nArrayCount++] = pAllProtocolsInfo[i].dwCatalogEntryId;
			break;
		}
		}
	}

	memcpy(&wpiLayeredProtocolInfo, &wpiOriginalProtocolInfo[0], sizeof(WSAPROTOCOL_INFOW)); // ��װ���ǵķֲ�Э�飬��ȡһ��dwLayeredCatalogId, �����һ���²�Э��Ľṹ���ƹ�������

	wcscpy_s(wpiLayeredProtocolInfo.szProtocol, pwszLSPProtocolName);

	wpiLayeredProtocolInfo.dwProviderFlags |= PFL_HIDDEN;
	wpiLayeredProtocolInfo.ProtocolChain.ChainLen = LAYERED_PROTOCOL; 	 // �޸�Э�����ƣ����ͣ�����PFL_HIDDEN��־

	do
	{
		if (::WSCInstallProvider(&GUID_SPIProvider, pwszPathName, &wpiLayeredProtocolInfo, 1, &nError) == SOCKET_ERROR) // ��װ
			break;

		FreeProvider(pAllProtocolsInfo);

		if (NULL == (pAllProtocolsInfo = GetAllProvider(&nProtocols)))// ����ö��Э�飬��ȡ�ֲ�Э���Ŀ¼ID��
			break;

		for (int i = 0; i < nProtocols; i++)
		{
			if (memcmp(&pAllProtocolsInfo[i].ProviderId, &GUID_SPIProvider, sizeof(GUID_SPIProvider)) == 0)
			{
				dwInstallCatalogID = pAllProtocolsInfo[i].dwCatalogEntryId;
				break;
			}
		}

		WCHAR wszChainName[WSAPROTOCOL_LEN + 1] = { 0 }; // ��װЭ���� �޸�Э�����ƣ�����

		for (int i = 0; i < nArrayCount; i++)
		{
			swprintf_s(wszChainName, L"%ws over %ws", pwszLSPProtocolName, wpiOriginalProtocolInfo[i].szProtocol);

			wcscpy_s(wpiOriginalProtocolInfo[i].szProtocol, wszChainName);

			if (wpiOriginalProtocolInfo[i].ProtocolChain.ChainLen == 1)
			{
				wpiOriginalProtocolInfo[i].ProtocolChain.ChainEntries[1] = dwOrigCatalogId[i];
			}
			else
			{
				for (int j = wpiOriginalProtocolInfo[i].ProtocolChain.ChainLen; j > 0; j--)
					wpiOriginalProtocolInfo[i].ProtocolChain.ChainEntries[j] = wpiOriginalProtocolInfo[i].ProtocolChain.ChainEntries[j - 1];
			}

			wpiOriginalProtocolInfo[i].ProtocolChain.ChainLen++;
			wpiOriginalProtocolInfo[i].ProtocolChain.ChainEntries[0] = dwInstallCatalogID;
		}

		GUID GUID_SPIProviderChain = { 0 };
		if (::UuidCreate(&GUID_SPIProviderChain) != RPC_S_OK) // ��ȡһ��Guid����װ֮
			break;

		if (::WSCInstallProvider(&GUID_SPIProviderChain, pwszPathName, wpiOriginalProtocolInfo, nArrayCount, &nError) == SOCKET_ERROR)
			break;

		bIsSuccess = true;

		MessageBoxA(NULL, "����˳��", ("����˳���, QQ �ɳ���װһ����Ч��LSP�ᵼ�¶���   ���غ�,�ᵼ�� LSP ��Ч"), MB_OK);//return bIsSuccess;	// ����˳���, QQ �ɳ���װһ����Ч��LSP�ᵼ�¶���   ���غ�,�ᵼ�� LSP ��Ч

		FreeProvider(pAllProtocolsInfo);

		if (NULL == (pAllProtocolsInfo = GetAllProvider(&nProtocols)))// ��������WinsockĿ¼�������ǵ�Э������ǰ
			break;

		int nIndex = 0;
		PDWORD pdwProtocalOrders = (PDWORD)malloc(sizeof(DWORD) * nProtocols);

		for (int i = 0; i < nProtocols; i++) // ������ǵ�Э����
		{
			if ((pAllProtocolsInfo[i].ProtocolChain.ChainLen > 1) && (pAllProtocolsInfo[i].ProtocolChain.ChainEntries[0] == dwInstallCatalogID))
				pdwProtocalOrders[nIndex++] = pAllProtocolsInfo[i].dwCatalogEntryId;
		}

		// �������Э��
		for (int i = 0; i < nProtocols; i++)
		{
			if ((pAllProtocolsInfo[i].ProtocolChain.ChainLen <= 1) || (pAllProtocolsInfo[i].ProtocolChain.ChainEntries[0] != dwInstallCatalogID))
				pdwProtocalOrders[nIndex++] = pAllProtocolsInfo[i].dwCatalogEntryId;
		}

		if ((nError = ::WSCWriteProviderOrder(pdwProtocalOrders, nIndex)) != ERROR_SUCCESS) // ��������WinsockĿ¼
			break;

	} while (false);

	if (pAllProtocolsInfo)
		FreeProvider(pAllProtocolsInfo);

	return bIsSuccess;
}

BOOL SPIRemove()
{
	int i = 0;
	int nError = 0;
	int nProtocols = 0;
	DWORD dwLayeredCatalogId = 0;
	LPWSAPROTOCOL_INFOW pProtoInfo = NULL;

	pProtoInfo = GetAllProvider(&nProtocols); // ����Guidȡ�÷ֲ�Э���Ŀ¼ID��

	for (i = 0; i < nProtocols; i++)
	{
		if (memcmp(&GUID_SPIProvider, &pProtoInfo[i].ProviderId, sizeof(GUID_SPIProvider)) == 0)
		{
			dwLayeredCatalogId = pProtoInfo[i].dwCatalogEntryId;
			break;
		}
	}

	if (i < nProtocols)
	{
		for (i = 0; i < nProtocols; i++) // �Ƴ�Э����
		{
			if ((pProtoInfo[i].ProtocolChain.ChainLen > 1) && (pProtoInfo[i].ProtocolChain.ChainEntries[0] == dwLayeredCatalogId))
				::WSCDeinstallProvider(&pProtoInfo[i].ProviderId, &nError);
		}

		::WSCDeinstallProvider(&GUID_SPIProvider, &nError); // �Ƴ��ֲ�Э��
	}
	else 
		return FALSE;

	return TRUE;
}