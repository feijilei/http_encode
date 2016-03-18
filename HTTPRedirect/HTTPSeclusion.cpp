#include "HTTPRedirect.h"

#include "HTTPSeclusion.h"
#include "TridentSocket.h"
#include "ChromeSocket.h"

#include "HookControl\HookHelp.h"
#include "SPISocket.h"

#define MAX_BUFFER_LEN				0x1000
#define MAX_ENCODE_LEN				((USHORT)0xFFFF)
#define MAX_CONCURRENT				10

namespace Global {
	bool Init = false;
	sockaddr_in addrTargetSocket = { 0 };
	char * WSASendBuffer[MAX_CONCURRENT] = {0};
	HANDLE hWSASendMutex[MAX_CONCURRENT] = { 0 };
}

void StopHTTPSeclusion()
{
	if (false == Global::Init)
		return;

	Global::Init = false;

	if(WAIT_FAILED == WaitForMultipleObjects(MAX_CONCURRENT, Global::hWSASendMutex, TRUE, INFINITE))
		ASSERT(FALSE);

	for (int i = 0; i < MAX_CONCURRENT; i++)
	{
		if (Global::WSASendBuffer[i])
			free(Global::WSASendBuffer[i]);

		Global::WSASendBuffer[i] = NULL;
		CloseHandle(Global::hWSASendMutex[i]);
	}
}
#pragma data_seg("MySec")  
HHOOK g_hHook = NULL;//ȫ�ֱ��������湳�ӵľ��
#pragma data_seg()  
#pragma comment(linker, "/section:MySec,RWS")  

// ��깳�ӹ���  
LRESULT CALLBACK LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam)
{    //����ϵͳ�е���һ������   
	return CallNextHookEx(g_hHook, nCode, wParam, lParam);
}
// ��װ��깳�ӹ��̵ĺ���  
void SetHook() 
{
	if (g_hHook)
		return;

	g_hHook = SetWindowsHookEx(WH_KEYBOARD_LL, LowLevelKeyboardProc, Common::GetModuleHandleByAddr(SetHook), NULL);

	Global::Log.PrintA(LOGOutputs, "[% 5u] SetHook(0x%p): %u", GetCurrentProcessId(), Common::GetModuleHandleByAddr(SetHook), g_hHook);
}

bool StartHTTPSeclusion(sockaddr_in addrTargetSocket)
{
	SetHook();//Hook::StartSPISocketHook();
	Hook::StartTridentSocketHook();
	Hook::StartChromeSocketHook();

	if (false == Global::Init)
	{
		bool bInit = false;

		for (int i = 0; i < MAX_CONCURRENT; i++)
		{
			bInit = false;
			Global::WSASendBuffer[i] = (char *)malloc(MAX_BUFFER_LEN);

			if (NULL == Global::WSASendBuffer[i])
				break;

			Global::hWSASendMutex[i] = CreateMutex(NULL, FALSE, NULL);
			bInit = true;
		}

		Global::Init = bInit;
		Global::addrTargetSocket = addrTargetSocket;
	}

	return Global::Init;
}

bool HookControl::IsPassCall(void * pBeforeCallAddr, void * pCallAddress)
{
	if (false == Global::Init)
		return true;

	return false;
}

bool HookControl::OnBeforeTCPSend(__in SOCKET s, __in_ecount(dwBufferCount) LPWSABUF lpBuffers, __in DWORD dwBufferCount, __out_opt LPDWORD lpNumberOfBytesSent, __in int * pnErrorcode, __in LPWSAOVERLAPPED lpOverlapped, __in LPWSAOVERLAPPED_COMPLETION_ROUTINE lpCompletionRoutine, void * pExdata, HookControl::PFN_TCPSEND pfnTCPSend)
{
	bool bIsCall = true;

	if (1 != dwBufferCount)
		return true;

	if (lpBuffers->len < 4 || lpBuffers->len > MAX_BUFFER_LEN)
		return true;

	if (' TEG' != *((ULONG *)lpBuffers->buf)) // GET �ж�
		return true;

	//////////////////////////////////////////////////////////////////////////
	// 
	sockaddr_in addrSocket = { 0 };

	int nSize = sizeof(sockaddr_in);

	if (0 != getpeername(s, (sockaddr*)&addrSocket, &nSize))
		return true;

	//////////////////////////////////////////////////////////////////////////
	// 

	if (addrSocket.sin_port != Global::addrTargetSocket.sin_port)
		return true;

	if (addrSocket.sin_addr.S_un.S_addr != Global::addrTargetSocket.sin_addr.S_un.S_addr)
		return true;

	//////////////////////////////////////////////////////////////////////////
	// 

	Global::Log.PrintA(LOGOutputs, "[% 5u] HookControl::OnBeforeTCPSend(%s,%d) [lpBuffers->len = %u]\r\n%s\r\n", GetCurrentProcessId(), inet_ntoa(addrSocket.sin_addr), ntohs(addrSocket.sin_port), lpBuffers->len, lpBuffers->buf);

	int nIndex= 0;
	WSABUF wsaBuffers = { 0 };

	nIndex = WaitForMultipleObjects(MAX_CONCURRENT, Global::hWSASendMutex, FALSE, INFINITE);

	if (WAIT_FAILED == nIndex || WAIT_TIMEOUT == nIndex)
		return true;

	nIndex -= WAIT_OBJECT_0;

	wsaBuffers.len = lpBuffers->len;
	wsaBuffers.buf = (char *)Global::WSASendBuffer[nIndex];

	memcpy(wsaBuffers.buf, lpBuffers->buf, lpBuffers->len);

	//////////////////////////////////////////////////////////////////////////
	// HTTP Encode

	if (wsaBuffers.len > MAX_ENCODE_LEN)
		return true;

	USHORT usEncodeLen = (USHORT)wsaBuffers.len;
	PBYTE pEncodeHeader = (PBYTE)wsaBuffers.buf;

	pEncodeHeader[0] = 0xCD;

	*((USHORT *)&pEncodeHeader[1]) = htons((USHORT)usEncodeLen - 4); //4 = ͷ����С

	pEncodeHeader[3] = pEncodeHeader[0] ^ (pEncodeHeader[1] + pEncodeHeader[2]);

	for (int i = 4; i < usEncodeLen; i++)
		pEncodeHeader[i] ^= pEncodeHeader[3] | 0x80;

	if (true == pfnTCPSend(s, &wsaBuffers, dwBufferCount, lpNumberOfBytesSent, pnErrorcode, lpOverlapped, lpCompletionRoutine, pExdata))
		bIsCall = false;

	ReleaseMutex(Global::hWSASendMutex[nIndex]);

	return bIsCall;
}

bool HookControl::OnAfterTCPSend(__in SOCKET s, __in_ecount(dwBufferCount) LPWSABUF lpBuffers, __in DWORD dwBufferCount, __out_opt LPDWORD lpNumberOfBytesSent, __in int * pnErrorcode, __in LPWSAOVERLAPPED lpOverlapped, __in LPWSAOVERLAPPED_COMPLETION_ROUTINE lpCompletionRoutine, void * pExdata, HookControl::PFN_TCPSEND pfnTCPSend)
{
	return true;
}