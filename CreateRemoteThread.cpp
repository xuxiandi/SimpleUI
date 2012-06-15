#pragma once
#include <windows.h>
#include <TlHelp32.h>
#include "stdio.h"
#pragma comment(lib,"kernel32")
#pragma comment(lib,"user32")
#pragma comment(lib,"Advapi32.lib")

//�̲߳����ṹ�嶨��
typedef struct _RemoteParam {
	char szMsg[12];    //MessageBox��������ʾ���ַ���ʾ
	DWORD dwMessageBox;//MessageBox��������ڵ�ַ
} RemoteParam, * PRemoteParam;
//����MessageBox���͵ĺ���ָ��
typedef int (__stdcall * PFN_MESSAGEBOX)(HWND, LPCTSTR, LPCTSTR, DWORD);

//�̺߳�������
DWORD __stdcall threadProc(LPVOID lParam)
{
	//ֻҪʹ��api�������� !!!!!!!!
	RemoteParam* pRP = (RemoteParam*)lParam;
	PFN_MESSAGEBOX pfnMessageBox;
	pfnMessageBox = (PFN_MESSAGEBOX)pRP->dwMessageBox;
	//��������д�������������������
	pfnMessageBox(NULL, pRP->szMsg, pRP->szMsg, 0);
	return 0;
}
//�������̷���Ȩ��
bool enableDebugPriv()
{
	HANDLE hToken;
	LUID sedebugnameValue;
	TOKEN_PRIVILEGES tkp;

	if (!OpenProcessToken(GetCurrentProcess(), 
		TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hToken)) {
			return false;
	}
	if (!LookupPrivilegeValue(NULL, SE_DEBUG_NAME, &sedebugnameValue)) {
		CloseHandle(hToken);
		return false;
	}
	tkp.PrivilegeCount = 1;
	tkp.Privileges[0].Luid = sedebugnameValue;
	tkp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
	if (!AdjustTokenPrivileges(hToken, FALSE, &tkp, sizeof(tkp), NULL, NULL)) {
		CloseHandle(hToken);
		return false;
	}
	return true;
}

//���ݽ������Ƶõ�����ID,����ж������ʵ���Ļ������ص�һ��ö�ٵ��Ľ��̵�ID
DWORD processNameToId(LPCTSTR lpszProcessName)
{
	HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
	PROCESSENTRY32 pe;
	pe.dwSize = sizeof(PROCESSENTRY32);
	if (!Process32First(hSnapshot, &pe)) {
		MessageBox(NULL, 
			"The frist entry of the process list has not been copyied to the buffer", 
			"Notice", MB_ICONINFORMATION | MB_OK);
		return 0;
	}
	while (Process32Next(hSnapshot, &pe)) {
		if (!strcmp(lpszProcessName, pe.szExeFile)) {
			return pe.th32ProcessID;
		}
	}

	return 0;
}
int main(int argc, char* argv[])
{
	//�����߳���Ĵ�С
	const DWORD dwThreadSize = 4096;
	DWORD dwWriteBytes;
	//�������̷���Ȩ��
	enableDebugPriv();
	//�ȴ�����������ƣ�ע���Сдƥ��
	char szExeName[MAX_PATH] = { 0 };
	//    cout<< "Please input the name of target process !" <<endl;
	//    
	//    cin >> szExeName;
	// cout<<szExeName<<endl;
	//strcpy(szExeName,"notepad.exe");
	//scanf("%s",szExeName);
	strcpy(szExeName,"chrome.exe");  

	DWORD dwProcessId = processNameToId(szExeName);
	if (dwProcessId == 0) {
		MessageBox(NULL, "The target process have not been found !",
			"Notice", MB_ICONINFORMATION | MB_OK);
		return -1;
	}
	//���ݽ���ID�õ����̾��
	HANDLE hTargetProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, dwProcessId);

	if (!hTargetProcess) {
		MessageBox(NULL, "Open target process failed !", 
			"Notice", MB_ICONINFORMATION | MB_OK);
		return 0;
	}

	//������������Ϊ�߳��忪��һ��洢����
	//��������Ҫע��MEM_COMMIT | MEM_RESERVE�ڴ���������Լ�PAGE_EXECUTE_READWRITE�ڴ汣������
	//����庬����ο�MSDN�й���VirtualAllocEx������˵����
	void* pRemoteThread = VirtualAllocEx(hTargetProcess, 0, 
		dwThreadSize, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
	if (!pRemoteThread) {
		MessageBox(NULL, "Alloc memory in target process failed !", 
			"notice", MB_ICONINFORMATION | MB_OK);
		return 0;
	}

	//���߳��忽��������������
	if (!WriteProcessMemory(hTargetProcess, 
		pRemoteThread, &threadProc, dwThreadSize, 0)) {
			MessageBox(NULL, "Write data to target process failed !", 
				"Notice", MB_ICONINFORMATION | MB_OK);
			return 0;
	}
	//�����̲߳����ṹ�����
	RemoteParam remoteData;
	ZeroMemory(&remoteData, sizeof(RemoteParam));

	//���ṹ������еĳ�Ա
	HINSTANCE hUser32 = LoadLibrary("User32.dll");
	remoteData.dwMessageBox = (DWORD)GetProcAddress(hUser32, "MessageBoxA");
	strcat(remoteData.szMsg, "Hello\0");

	//Ϊ�̲߳��������������п��ٴ洢����
	RemoteParam* pRemoteParam = (RemoteParam*)VirtualAllocEx(
		hTargetProcess , 0, sizeof(RemoteParam), MEM_COMMIT, PAGE_READWRITE);

	if (!pRemoteParam) {
		MessageBox(NULL, "Alloc memory failed !", 
			"Notice", MB_ICONINFORMATION | MB_OK);
		return 0;
	}
	//���̲߳����������������̵�ַ�ռ���
	DWORD dwWritten;   
	if (!WriteProcessMemory(hTargetProcess ,
		pRemoteParam, &remoteData, sizeof(remoteData), &dwWritten)) {
			MessageBox(NULL, "Write data to target process failed !", 
				"Notice", MB_ICONINFORMATION | MB_OK);
			return 0;
	}
	else
	{
		// Ҫд���ֽ�����ʵ��д���ֽ�������ȣ�����ʧ��   
		if ( dwWritten != sizeof(remoteData) )   
		{   
			VirtualFreeEx( hTargetProcess, pRemoteThread, dwThreadSize, MEM_DECOMMIT );   
			CloseHandle( hTargetProcess );   
			return FALSE;   
		}   
	}

	//�����������д����߳�
	HANDLE hRemoteThread = CreateRemoteThread(
		hTargetProcess, NULL, 0, (LPTHREAD_START_ROUTINE)pRemoteThread, 
		pRemoteParam, 0, &dwWriteBytes);

	if (!hRemoteThread) {
		VirtualFreeEx( hTargetProcess, pRemoteThread, dwThreadSize, MEM_DECOMMIT );   
		CloseHandle( hTargetProcess );   
		MessageBox(NULL, "Create remote thread failed !", "Notice",  MB_ICONINFORMATION | MB_OK);
		return 0;
	}
	// �ȴ��������   
	WaitForSingleObject( hRemoteThread, INFINITE );   
	// �ͷ�Ŀ�����������Ŀռ�   
	VirtualFreeEx( hTargetProcess, pRemoteThread, dwThreadSize, MEM_DECOMMIT );   
	CloseHandle(hRemoteThread);
	CloseHandle( hTargetProcess );   
	FreeLibrary(hUser32);
	return 0;
}