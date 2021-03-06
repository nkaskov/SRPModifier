
#include <filesystem>
#include <ws2tcpip.h>
#include <stdlib.h>
#include <initguid.h>
#include <gpedit.h>
#include <objbase.h>
#include <stdio.h>
#include <vector>
#include <iostream>
#include <string>
#include <tchar.h>
#include <time.h>
#include <random>
#include "Shlwapi.h"

// Need to link with Ws2_32.lib, Mswsock.lib, and Advapi32.lib
#pragma comment (lib, "Ws2_32.lib")
#pragma comment (lib, "Mswsock.lib")
#pragma comment (lib, "AdvApi32.lib")

#define WIN32_LEAN_AND_MEAN

#define DEFAULT_BUFLEN 512
#define SERVER_PORT "4321"
#define SERVER_NAME "192.168.77.61"

#define MAX_KEY_LENGTH 255
#define MAX_VALUE_LENGTH 255

#define MACHINE_POLICY 0
#define USER_POLICY 1

#define DEFAULT_POLICY USER_POLICY

#define UNRESTRICTED_POLICY 0
#define BASIC_USER_POLICY 1
#define DISALLOWED_POLICY 2

#define UNRESTRICTED_RULE 0
#define BASIC_USER_RULE 1
#define DISALLOWED_RULE 2

#define uchar unsigned char

GUID AdminToolGuids[] = {
						{ 0x3d271cfc, 0x2bc6, 0x4ac2,{ 0xb6, 0x33, 0x3b, 0xdf, 0xf5, 0xbd, 0xab, 0x2a } },
						{ 0x5d671bdb, 0x2ee6, 0x7a52,{ 0xdc, 0x37, 0x7a, 0x7f, 0xe5, 0xb8, 0xbb, 0x11 } },
						{ 0xed3714da, 0x11e6, 0xe452,{ 0xde, 0x47, 0x75, 0x71, 0xa5, 0xc8, 0x4c, 0x45 } }
						};

DWORD AdminToolGuidNumber;
GUID ThisAdminToolGuid;

using namespace std;

void CreatePolicy(DWORD policy_type = USER_POLICY, DWORD policy_access = UNRESTRICTED_POLICY) {
	HRESULT hr = S_OK;
	IGroupPolicyObject* pLGPO;
	HKEY gpo_key, dsrkey;
	// MSVC is finicky about these ones => redefine them
	GUID RegistryId = REGISTRY_EXTENSION_GUID;
	// This next one can be any GUID you want
	

	// Create an instance of the IGroupPolicyObject class
	CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
	hr = CoCreateInstance(CLSID_GroupPolicyObject, NULL, CLSCTX_INPROC_SERVER,
		IID_IGroupPolicyObject, (LPVOID*)&pLGPO);

	if (SUCCEEDED(hr)) {
		DWORD dwSection;

		switch (policy_type) {
		case MACHINE_POLICY:
			dwSection = GPO_SECTION_MACHINE;
			break;
		case USER_POLICY:
			dwSection = GPO_SECTION_USER;
			break;
		default:
			dwSection = GPO_SECTION_USER;
		}

		hr = pLGPO->OpenLocalMachineGPO(GPO_OPEN_LOAD_REGISTRY);

		hr = pLGPO->GetRegistryKey(dwSection, &gpo_key);

		hr = RegCreateKeyEx(gpo_key, L"Software\\Policies\\Microsoft\\Windows\\Safer\\CodeIdentifiers",
			0, NULL, 0, KEY_SET_VALUE | KEY_QUERY_VALUE, NULL, &dsrkey, NULL);
		
		// Create the values

		//begin writing DefaultLevel
		DWORD val = 0;
		switch (policy_access) {
		case UNRESTRICTED_POLICY:
			val = 262144;
			break;
		/*case BASIC_USER_POLICY:
			val = ? ? ? ;
			break;*/
		case DISALLOWED_POLICY:
			val = 0;
			break;
		}
		
		hr = RegSetValueEx(dsrkey, L"DefaultLevel", NULL, REG_DWORD, (const BYTE*)(&val), sizeof(val));
		//end writing DefaultLevel
		
		//begin writing ExecutableTypes
		const wchar_t *strings[] = { L"ADE", L"ADP", L"BAS", L"BAT", L"CHM", L"CMD", L"COM", L"CPL", L"CRT", L"EXE", L"HLP", L"HTA", L"INF", L"INS", L"ISP", L"LNK", L"MDB", L"MDE", L"MSC", L"MSI", L"MSP", L"MST", L"OCX", L"PCD", L"PIF", L"REG", L"SCR", L"SHS", L"UR", L"VB", L"WSC" }; // array of strings
		int N = 31; // number of strings

		int len = 1;
		for (int i = 0; i<N; i++)
			len += wcslen(strings[i]) + 1;

		wchar_t* multi_sz = (wchar_t *)malloc(2*len), * ptr = multi_sz;
		memset(multi_sz, 0, len);
		for (int i = 0; i<N; i++) {
			wcscpy(ptr, strings[i]);
			ptr += wcslen(strings[i]) + 1;
		}

		hr = RegSetValueEx(dsrkey, L"ExecutableTypes", NULL, REG_MULTI_SZ, (const BYTE*)(&multi_sz[0]), 2*len);
		free(multi_sz);
		//end writing ExecutableTypes

		//begin writing PolicyScope
		val = 0;
		hr = RegSetValueEx(dsrkey, L"PolicyScope", NULL, REG_DWORD, (const BYTE*)(&val), sizeof(val));
		//end writing PolicyScope
		
		//begin writing TransparentEnabled
		val = 1;
		hr = RegSetValueEx(dsrkey, L"TransparentEnabled", NULL, REG_DWORD, (const BYTE*)(&val), sizeof(val));
		//end writing TransparentEnabled

		RegCloseKey(dsrkey);
		
		// Apply policy and free resources

		switch (policy_type) {
		case MACHINE_POLICY:
			hr = pLGPO->Save(TRUE, TRUE, &RegistryId, &ThisAdminToolGuid);
			break;
		case USER_POLICY:
			hr = pLGPO->Save(FALSE, TRUE, &RegistryId, &ThisAdminToolGuid);
			break;
		}

		RegCloseKey(gpo_key);

		pLGPO->Release();

	}

}

void DeletePolicy(DWORD policy_type = USER_POLICY) {
	CreatePolicy(policy_type); //Just create policy with required policy_type and UNRESTRICTED policy_access
}

LPTSTR* GetListOfExistFilePathRules(HKEY gpo_key, DWORD rule_access = UNRESTRICTED_RULE) {
	HRESULT hr = S_OK;
	HKEY paths_key;


	switch (rule_access) {
	case UNRESTRICTED_RULE:
		hr = RegCreateKeyEx(gpo_key, L"Software\\Policies\\Microsoft\\Windows\\Safer\\CodeIdentifiers\\262144\\Paths",
			NULL, NULL, 0, KEY_SET_VALUE | KEY_QUERY_VALUE | KEY_ENUMERATE_SUB_KEYS, NULL, &paths_key, NULL);
		break;
		/*case BASIC_USER_RULE:
		val = ? ? ? ;
		break;*/
	case DISALLOWED_RULE:
		hr = RegCreateKeyEx(gpo_key, L"Software\\Policies\\Microsoft\\Windows\\Safer\\CodeIdentifiers\\0\\Paths",
			NULL, NULL, 0, KEY_SET_VALUE | KEY_QUERY_VALUE | KEY_ENUMERATE_SUB_KEYS, NULL, &paths_key, NULL);
		break;
	}
	
	if (SUCCEEDED(hr)) {
		DWORD number_of_subkeys = 0;
		hr = RegQueryInfoKey(paths_key, NULL, NULL, NULL, &number_of_subkeys, NULL, NULL, NULL, NULL, NULL, NULL, NULL);
		//printf("Total %d subkeys\n", number_of_subkeys);

		LPTSTR * result_list = (LPTSTR *)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, (number_of_subkeys + 1)* sizeof(LPTSTR));

		WCHAR new_subkey_name[MAX_KEY_LENGTH];
		DWORD size_of_subkey = MAX_KEY_LENGTH;

		for (DWORD subkey_number = 0; subkey_number < number_of_subkeys; subkey_number++){
			result_list[subkey_number] = (LPTSTR)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, size_of_subkey *sizeof(WCHAR));

			size_of_subkey = MAX_KEY_LENGTH;

			hr = RegEnumKeyEx(paths_key, subkey_number, result_list[subkey_number], &size_of_subkey, NULL, NULL, NULL, NULL);

			//wprintf(L"%s\n", result_list[subkey_number]);
		}

		result_list[number_of_subkeys] = NULL;

		return result_list;
		
	}
	else {
		return NULL;
	}
}

TCHAR GeneratePathSymbol() {

	// initialize the random number generator

	int random_value = rand() % 16;

	if (random_value < 10) {
		return '0' + random_value;
	}
	else {
		return 'a' + random_value - 10;
	}
}

LPCTSTR GenerateNewPathName() {
	
	static BOOL first_call = TRUE;

	if (first_call) {
		srand(time(NULL));
	}

	first_call = FALSE;

	LPTSTR new_path_name = (LPTSTR)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, 39 * sizeof(TCHAR));
	new_path_name[0] = '{';
	new_path_name[37] = '}';
	new_path_name[9] = '-';
	new_path_name[14] = '-';
	new_path_name[19] = '-';
	new_path_name[24] = '-';
	for (int index = 1; index < 9; index++) {
		new_path_name[index] = GeneratePathSymbol();
	}
	for (int index = 10; index < 14; index++) {
		new_path_name[index] = GeneratePathSymbol();
	}
	for (int index = 15; index < 19; index++) {
		new_path_name[index] = GeneratePathSymbol();
	}
	for (int index = 20; index < 24; index++) {
		new_path_name[index] = GeneratePathSymbol();
	}
	for (int index = 25; index < 37; index++) {
		new_path_name[index] = GeneratePathSymbol();
	}

	//wprintf(L"Generated name %s\n", new_path_name);

	return new_path_name;
	//return L"{748a4186-9365-431d-a3db-1faecd80cca5}";
}

void DeleteFilePathRule(LPCTSTR file_path, DWORD policy_type = USER_POLICY) {
	HRESULT hr = S_OK;
	IGroupPolicyObject* pLGPO;
	HKEY gpo_key;

	GUID RegistryId = REGISTRY_EXTENSION_GUID;

	

	// Create an instance of the IGroupPolicyObject class
	CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
	hr = CoCreateInstance(CLSID_GroupPolicyObject, NULL, CLSCTX_INPROC_SERVER,
		IID_IGroupPolicyObject, (LPVOID*)&pLGPO);

	if (SUCCEEDED(hr)) {
		DWORD dwSection;

		switch (policy_type) {
		case MACHINE_POLICY:
			dwSection = GPO_SECTION_MACHINE;
			break;
		case USER_POLICY:
			dwSection = GPO_SECTION_USER;
			break;
		default:
			dwSection = GPO_SECTION_USER;
		}

		hr = pLGPO->OpenLocalMachineGPO(GPO_OPEN_LOAD_REGISTRY);

		hr = pLGPO->GetRegistryKey(dwSection, &gpo_key);

		LPTSTR* unrestricted_list = GetListOfExistFilePathRules(gpo_key, UNRESTRICTED_RULE);
		LPTSTR* disallowed_list = GetListOfExistFilePathRules(gpo_key, DISALLOWED_RULE);
		
		int unrestricted_path_number = 0;
		LPTSTR unrestricted_path_name;
		HKEY unrestricted_paths_key;

		int disallowed_path_number = 0;
		LPTSTR disallowed_path_name;
		HKEY disallowed_paths_key;

		hr = RegCreateKeyEx(gpo_key, L"Software\\Policies\\Microsoft\\Windows\\Safer\\CodeIdentifiers\\262144\\Paths",
			NULL, NULL, 0, KEY_SET_VALUE | KEY_QUERY_VALUE | KEY_ENUMERATE_SUB_KEYS, NULL, &unrestricted_paths_key, NULL);

		while (unrestricted_path_name = unrestricted_list[unrestricted_path_number++]) {
			HKEY unrestricted_path_key;

			LPTSTR lpData[MAX_VALUE_LENGTH];
			DWORD lpcbData = MAX_VALUE_LENGTH;
			hr = RegCreateKeyEx(unrestricted_paths_key, unrestricted_path_name,
				NULL, NULL, 0, KEY_SET_VALUE | KEY_QUERY_VALUE | KEY_ENUMERATE_SUB_KEYS | DELETE, NULL, &unrestricted_path_key, NULL);

			hr = RegQueryValueEx(
				unrestricted_path_key,
				L"ItemData",
				NULL,
				NULL,
				(LPBYTE) lpData,
				&lpcbData
			);
			//wprintf(L"%s\n", lpData);

			RegCloseKey(unrestricted_path_key);

			if (!wcscmp((LPCTSTR)lpData, file_path)) {
				//printf("Will delete\n");
				hr = RegDeleteKeyEx(unrestricted_paths_key, unrestricted_path_name, KEY_ALL_ACCESS, NULL);
				//hr = RegDeleteTree(unrestricted_paths_key, unrestricted_path_name); //Similar behavior
				//printf("Delete %d\n", hr == ERROR_SUCCESS);
			}

		}

		RegCloseKey(unrestricted_paths_key);

		hr = RegCreateKeyEx(gpo_key, L"Software\\Policies\\Microsoft\\Windows\\Safer\\CodeIdentifiers\\0\\Paths",
			NULL, NULL, 0, KEY_SET_VALUE | KEY_QUERY_VALUE | KEY_ENUMERATE_SUB_KEYS, NULL, &disallowed_paths_key, NULL);

		while (disallowed_path_name = disallowed_list[disallowed_path_number++]) {
			HKEY disallowed_path_key;

			LPTSTR lpData[MAX_VALUE_LENGTH];
			DWORD lpcbData = MAX_VALUE_LENGTH;
			hr = RegCreateKeyEx(disallowed_paths_key, disallowed_path_name,
				NULL, NULL, 0, KEY_SET_VALUE | KEY_QUERY_VALUE | KEY_ENUMERATE_SUB_KEYS | DELETE, NULL, &disallowed_path_key, NULL);

			hr = RegQueryValueEx(
				disallowed_path_key,
				L"ItemData",
				NULL,
				NULL,
				(LPBYTE)lpData,
				&lpcbData
			);
			//wprintf(L"%s\n", lpData);

			RegCloseKey(disallowed_path_key);

			if (!wcscmp((LPCTSTR)lpData, file_path)) {
				//printf("Will delete\n");
				hr = RegDeleteKeyEx(disallowed_paths_key, disallowed_path_name, KEY_ALL_ACCESS, NULL);
				//hr = RegDeleteTree(unrestricted_paths_key, unrestricted_path_name); //Similar behavior
				//printf("Delete %d\n", hr == ERROR_SUCCESS);
			}

		}

		RegCloseKey(disallowed_paths_key);
	}
	
	switch (policy_type) {
	case MACHINE_POLICY:
		hr = pLGPO->Save(TRUE, TRUE, &RegistryId, &ThisAdminToolGuid);
		break;
	case USER_POLICY:
		hr = pLGPO->Save(FALSE, TRUE, &RegistryId, &ThisAdminToolGuid);
		break;
	}

	RegCloseKey(gpo_key);

	pLGPO->Release();
	
}

void DeleteAllFilePathRules(DWORD policy_type = USER_POLICY) {
	HRESULT hr = S_OK;
	IGroupPolicyObject* pLGPO;
	HKEY gpo_key;

	GUID RegistryId = REGISTRY_EXTENSION_GUID;

	

	// Create an instance of the IGroupPolicyObject class
	CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
	hr = CoCreateInstance(CLSID_GroupPolicyObject, NULL, CLSCTX_INPROC_SERVER,
		IID_IGroupPolicyObject, (LPVOID*)&pLGPO);

	if (SUCCEEDED(hr)) {
		DWORD dwSection;

		switch (policy_type) {
		case MACHINE_POLICY:
			dwSection = GPO_SECTION_MACHINE;
			break;
		case USER_POLICY:
			dwSection = GPO_SECTION_USER;
			break;
		default:
			dwSection = GPO_SECTION_USER;
		}

		hr = pLGPO->OpenLocalMachineGPO(GPO_OPEN_LOAD_REGISTRY);

		hr = pLGPO->GetRegistryKey(dwSection, &gpo_key);

		HKEY all_paths_key;

		hr = RegCreateKeyEx(gpo_key, L"Software\\Policies\\Microsoft\\Windows\\Safer\\CodeIdentifiers\\",
			NULL, NULL, 0, KEY_SET_VALUE | KEY_QUERY_VALUE | KEY_ENUMERATE_SUB_KEYS | DELETE, NULL, &all_paths_key, NULL);

		RegDeleteTree(all_paths_key, L"262144"); //Delete rules for unrestricted access
		RegDeleteTree(all_paths_key, L"0"); //Delete rules for disallowed access
		//RegDeleteTree(all_paths_key, "MAGIC"); //Delete rules for basic user
		RegCloseKey(all_paths_key);

		switch (policy_type) {
		case MACHINE_POLICY:
			hr = pLGPO->Save(TRUE, TRUE, &RegistryId, &ThisAdminToolGuid);
			break;
		case USER_POLICY:
			hr = pLGPO->Save(FALSE, TRUE, &RegistryId, &ThisAdminToolGuid);
			break;
		}

		RegCloseKey(gpo_key);

		pLGPO->Release();
	}
}

void AddFilePathRule(LPCWSTR new_path, DWORD policy_type = USER_POLICY, DWORD rule_access = UNRESTRICTED_RULE) {
	HRESULT hr = S_OK;
	IGroupPolicyObject* pLGPO;
	HKEY gpo_key, path_section_key, path_key;

	GUID RegistryId = REGISTRY_EXTENSION_GUID;
	// This next one can be any GUID you want
	

	// Create an instance of the IGroupPolicyObject class
	CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
	hr = CoCreateInstance(CLSID_GroupPolicyObject, NULL, CLSCTX_INPROC_SERVER,
		IID_IGroupPolicyObject, (LPVOID*)&pLGPO);

	if (SUCCEEDED(hr)) {
		DWORD dwSection;

		switch (policy_type) {
		case MACHINE_POLICY:
			dwSection = GPO_SECTION_MACHINE;
			break;
		case USER_POLICY:
			dwSection = GPO_SECTION_USER;
			break;
		default:
			dwSection = GPO_SECTION_USER;
		}



		hr = pLGPO->OpenLocalMachineGPO(GPO_OPEN_LOAD_REGISTRY);

		hr = pLGPO->GetRegistryKey(dwSection, &gpo_key);
		
		if (rule_access == UNRESTRICTED_RULE) { //Unrestricted access
			hr = RegCreateKeyEx(gpo_key, L"Software\\Policies\\Microsoft\\Windows\\Safer\\CodeIdentifiers\\262144\\Paths\\",
				NULL, NULL, 0, KEY_SET_VALUE | KEY_QUERY_VALUE | KEY_ENUMERATE_SUB_KEYS, NULL, &path_section_key, NULL);
		}
		else {
			if (rule_access == DISALLOWED_RULE) { //Disallowed access
				hr = RegCreateKeyEx(gpo_key, L"Software\\Policies\\Microsoft\\Windows\\Safer\\CodeIdentifiers\\0\\Paths\\",
					NULL, NULL, 0, KEY_SET_VALUE | KEY_QUERY_VALUE | KEY_ENUMERATE_SUB_KEYS, NULL, &path_section_key, NULL);
			}
		}


		hr = RegCreateKeyEx(path_section_key, GenerateNewPathName(),
			NULL, NULL, 0, KEY_SET_VALUE | KEY_QUERY_VALUE | KEY_ENUMERATE_SUB_KEYS, NULL, &path_key, NULL);

		//begin writing Description
		const wchar_t * val_sz = L"";
		hr = RegSetKeyValue(path_key, NULL, L"Description", REG_SZ, val_sz, 2 * wcslen(val_sz));
		//end writing Description

		//begin writing ItemData
		hr = RegSetKeyValue(path_key, NULL, L"ItemData", REG_SZ, new_path, 2 * wcslen(new_path));
		//end writing ItemData


		//begin writing LastModified
		DWORD val = 131706312035314738;
		hr = RegSetValueEx(path_key, L"LastModified", NULL, REG_DWORD, (const BYTE*)(&val), sizeof(val));
		//end writing LastModified

		//begin writing SaferFlags
		val = 0;
		hr = RegSetValueEx(path_key, L"SaferFlags", NULL, REG_DWORD, (const BYTE*)(&val), sizeof(val));
		//end writing SaferFlags


		RegCloseKey(path_key);

		RegCloseKey(path_section_key);

		// Apply policy and free resources
		

		switch (policy_type) {
		case MACHINE_POLICY:
			hr = pLGPO->Save(TRUE, TRUE, &RegistryId, &ThisAdminToolGuid);
			break;
		case USER_POLICY:
			hr = pLGPO->Save(FALSE, TRUE, &RegistryId, &ThisAdminToolGuid);
			break;
		}

		RegCloseKey(gpo_key);

		pLGPO->Release();

	}

}

void PrintExistingPolicy(DWORD policy_type = USER_POLICY) {
	HRESULT hr = S_OK;
	IGroupPolicyObject* pLGPO;
	HKEY gpo_key, dsrkey;

	GUID RegistryId = REGISTRY_EXTENSION_GUID;
	
	

	// Create an instance of the IGroupPolicyObject class
	CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
	hr = CoCreateInstance(CLSID_GroupPolicyObject, NULL, CLSCTX_INPROC_SERVER,
		IID_IGroupPolicyObject, (LPVOID*)&pLGPO);

	if (SUCCEEDED(hr)) {
		DWORD dwSection;

		switch (policy_type) {
		case MACHINE_POLICY:
			dwSection = GPO_SECTION_MACHINE;
			wprintf(L"Existing machine policy:\n");
			break;
		case USER_POLICY:
			dwSection = GPO_SECTION_USER;
			wprintf(L"Existing user policy:\n");
			break;
		default:
			dwSection = GPO_SECTION_USER;
			wprintf(L"Existing user policy:\n");
		}
		
		hr = pLGPO->OpenLocalMachineGPO(GPO_OPEN_LOAD_REGISTRY);

		hr = pLGPO->GetRegistryKey(dwSection, &gpo_key);

		LPTSTR *file_path_names_list = GetListOfExistFilePathRules(gpo_key, UNRESTRICTED_RULE);

		int path_number = 0;
		LPTSTR file_path_name;
		
		TCHAR base_path_name[] = L"Software\\Policies\\Microsoft\\Windows\\Safer\\CodeIdentifiers\\262144\\Paths\\";
		TCHAR tmp_path_name[255];
		TCHAR item_data[255];
		DWORD size;

		wprintf(L"\tExisting unrestricted rules:\n");
		while (file_path_name = file_path_names_list[path_number++]) {

			wcscpy(tmp_path_name, base_path_name);
			wcscat(tmp_path_name, file_path_name);

			size = 255*sizeof(TCHAR);
			printf("!");
			fflush(stdout);
			RegGetValue(gpo_key, tmp_path_name, L"ItemData", RRF_RT_REG_SZ, NULL, item_data, &size);

			wprintf(L"\t\t%s\n", item_data);
		}

		file_path_names_list = GetListOfExistFilePathRules(gpo_key, DISALLOWED_RULE);

		path_number = 0;

		wcscpy(base_path_name, L"Software\\Policies\\Microsoft\\Windows\\Safer\\CodeIdentifiers\\0\\Paths\\");

		wprintf(L"\tExisting disallowed rules:\n");

		while (file_path_name = file_path_names_list[path_number++]) {

			wcscpy(tmp_path_name, base_path_name);
			wcscat(tmp_path_name, file_path_name);

			size = 255 * sizeof(TCHAR);

			RegGetValue(gpo_key, tmp_path_name, L"ItemData", RRF_RT_REG_SZ, NULL, item_data, &size);

			wprintf(L"\t\t%s\n", item_data);
		}

		return;

		//hr = pLGPO->Save(TRUE, TRUE, &RegistryId, &ThisAdminToolGuid);

		//printf("Saved changes %d GLE=%d\n", SUCCEEDED(hr), GetLastError());

		RegCloseKey(gpo_key);

		pLGPO->Release();

	}

}

vector <string> GetUserNamesFromServerAnswer(char* server_answer_char) {
	string server_answer(server_answer_char);
	int user_index = 0, tmp_index = 0;

	while ((tmp_index = server_answer.find("[", tmp_index + 1)) != string::npos) {
		user_index = tmp_index;
	}

	vector <string> result_vector(0);

	tmp_index = user_index;

	while ((tmp_index = server_answer.find("'", tmp_index + 1)) != string::npos) {
		user_index = tmp_index;

		if ((tmp_index = server_answer.find("'", tmp_index + 1)) == string::npos) {
			break;
		}

		result_vector.push_back(server_answer.substr(user_index + 1, tmp_index - user_index - 1));

	}

	return result_vector;
}

vector <string> GetUserAppsFromServerAnswer(char* server_answer_char) {
	string server_answer(server_answer_char);

	int user_index = 0, tmp_index = 0;

	while ((tmp_index = server_answer.find("[", tmp_index + 1)) != string::npos) {
		user_index = tmp_index;
	}

	vector <string> result_vector(0);
	int user_number = 0;

	tmp_index = user_index;

	while ((tmp_index = server_answer.find("'", tmp_index + 1)) != string::npos) {
		user_index = tmp_index;

		if ((tmp_index = server_answer.find("'", tmp_index + 1)) == string::npos) {
			break;
		}

		result_vector.push_back(server_answer.substr(user_index + 1, tmp_index - user_index - 1));

	}

	return result_vector;
}

vector <string> GetUserNames() {
	//char server_name[] = "192.168.77.61";

	WSADATA wsaData;
	SOCKET ConnectSocket = INVALID_SOCKET;
	struct addrinfo *result = NULL,
		*ptr = NULL,
		hints;
	char sendbuf_get_users[] = "./srp_shell get_users 2> /dev/null ; cat answer.txt";
	char recvbuf[DEFAULT_BUFLEN];
	int iResult;
	int recvbuflen = DEFAULT_BUFLEN;

	// Validate the parameters
	
	// Initialize Winsock
	iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (iResult != 0) {
		printf("WSAStartup failed with error: %d\n", iResult);
		return {};
	}

	ZeroMemory(&hints, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;

	// Resolve the server address and port
	iResult = getaddrinfo(SERVER_NAME, SERVER_PORT, &hints, &result);
	if (iResult != 0) {
		printf("getaddrinfo failed with error: %d\n", iResult);
		WSACleanup();
		return {};
	}

	// Attempt to connect to an address until one succeeds
	for (ptr = result; ptr != NULL; ptr = ptr->ai_next) {

		// Create a SOCKET for connecting to server
		ConnectSocket = socket(ptr->ai_family, ptr->ai_socktype,
			ptr->ai_protocol);
		if (ConnectSocket == INVALID_SOCKET) {
			printf("socket failed with error: %ld\n", WSAGetLastError());
			WSACleanup();
			return {};
		}

		// Connect to server.
		iResult = connect(ConnectSocket, ptr->ai_addr, (int)ptr->ai_addrlen);
		if (iResult == SOCKET_ERROR) {
			printf("Unable to connect to server! GLE=%d\n", GetLastError());
			closesocket(ConnectSocket);
			ConnectSocket = INVALID_SOCKET;
			continue;
		}
		break;
	}


	freeaddrinfo(result);

	if (ConnectSocket == INVALID_SOCKET) {
		printf("Unable to connect to server!\n");
		WSACleanup();
		return {};
	}

	// Send an initial buffer
	iResult = send(ConnectSocket, sendbuf_get_users, (int)strlen(sendbuf_get_users), 0);
	if (iResult == SOCKET_ERROR) {
		printf("send failed with error: %d\n", WSAGetLastError());
		closesocket(ConnectSocket);
		WSACleanup();
		return {};
	}

	printf("Bytes Sent: %ld\n", iResult);

	// shutdown the connection since no more data will be sent
	iResult = shutdown(ConnectSocket, SD_SEND);
	if (iResult == SOCKET_ERROR) {
		printf("shutdown failed with error: %d\n", WSAGetLastError());
		closesocket(ConnectSocket);
		WSACleanup();
		return {};
	}

	// Receive until the peer closes the connection
	do {

		iResult = recv(ConnectSocket, recvbuf, recvbuflen, 0);
		if (iResult > 0) {
			printf("Bytes received: %d\n", iResult);

			return GetUserNamesFromServerAnswer(recvbuf);

		}
		else if (iResult == 0)
			printf("Connection closed\n");
		else
			printf("recv failed with error: %d\n", WSAGetLastError());

	} while (iResult > 0);

	// cleanup
	closesocket(ConnectSocket);
	WSACleanup();
}

vector <string> GetUserApps(string username) {
	WSADATA wsaData;
	SOCKET ConnectSocket = INVALID_SOCKET;
	struct addrinfo *result = NULL,
		*ptr = NULL,
		hints;
	
	char sendbuf_get_apps[255] = "./srp_shell get_apps ";
	strcat(sendbuf_get_apps, username.c_str());
	strcat(sendbuf_get_apps, " 2> /dev/null ; cat answer.txt");

	char recvbuf[DEFAULT_BUFLEN];
	int iResult;
	int recvbuflen = DEFAULT_BUFLEN;

	// Validate the parameters

	// Initialize Winsock
	iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (iResult != 0) {
		printf("WSAStartup failed with error: %d\n", iResult);
		return {};
	}

	ZeroMemory(&hints, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;

	// Resolve the server address and port
	iResult = getaddrinfo(SERVER_NAME, SERVER_PORT, &hints, &result);
	if (iResult != 0) {
		printf("getaddrinfo failed with error: %d\n", iResult);
		WSACleanup();
		return {};
	}

	// Attempt to connect to an address until one succeeds
	for (ptr = result; ptr != NULL; ptr = ptr->ai_next) {

		// Create a SOCKET for connecting to server
		ConnectSocket = socket(ptr->ai_family, ptr->ai_socktype,
			ptr->ai_protocol);
		if (ConnectSocket == INVALID_SOCKET) {
			printf("socket failed with error: %ld\n", WSAGetLastError());
			WSACleanup();
			return {};
		}

		// Connect to server.
		iResult = connect(ConnectSocket, ptr->ai_addr, (int)ptr->ai_addrlen);
		if (iResult == SOCKET_ERROR) {
			printf("Unable to connect to server! GLE=%d\n", GetLastError());
			closesocket(ConnectSocket);
			ConnectSocket = INVALID_SOCKET;
			continue;
		}
		break;
	}


	freeaddrinfo(result);

	if (ConnectSocket == INVALID_SOCKET) {
		printf("Unable to connect to server!\n");
		WSACleanup();
		return {};
	}

	// Send an initial buffer
	iResult = send(ConnectSocket, sendbuf_get_apps, (int)strlen(sendbuf_get_apps), 0);
	if (iResult == SOCKET_ERROR) {
		printf("send failed with error: %d\n", WSAGetLastError());
		closesocket(ConnectSocket);
		WSACleanup();
		return {};
	}

	printf("Bytes Sent: %ld\n", iResult);

	// shutdown the connection since no more data will be sent
	iResult = shutdown(ConnectSocket, SD_SEND);
	if (iResult == SOCKET_ERROR) {
		printf("shutdown failed with error: %d\n", WSAGetLastError());
		closesocket(ConnectSocket);
		WSACleanup();
		return {};
	}

	// Receive until the peer closes the connection
	do {

		iResult = recv(ConnectSocket, recvbuf, recvbuflen, 0);
		if (iResult > 0) {
			printf("Bytes received: %d\n", iResult);

			return GetUserAppsFromServerAnswer(recvbuf);

		}
		else if (iResult == 0)
			printf("Connection closed\n");
		else
			printf("recv failed with error: %d\n", WSAGetLastError());

	} while (iResult > 0);

	// cleanup
	closesocket(ConnectSocket);
	WSACleanup();
}

/*
int __cdecl main(int argc, char **argv){

	// Validate the parameters
	if (argc != 2) {
		printf("usage: %s server-name\n", argv[0]);
		return 1;
	}

	vector <string> vector_apps = GetUserApps(string(argv[1]));

	for (vector<string>::iterator it = vector_apps.begin(); it != vector_apps.end(); ++it) {
		cout << "App:" << *it << endl;
	}

	return 0;
}*/


namespace DefaultValue {
	enum TDefaultValue { disallowed = 0x0, unrestricted = 0x40000 };
}

namespace TransparentEnabled {
	enum TTransparentEnabled { off = 0x0, skip_dll = 0x1, all_files = 0x2 };
}
namespace PolicyScope {
	enum TPolicyScope { all_users = 0x0, skip_admins = 0x1 };
}

namespace HashAlg {
	enum THashAlg { MD5 = 32771, SHA1 = 32772, SHA256 = 32780 };
}

const int ALLOWED_VALUE = 262144;
const int DISALLAWED_VALUE = 0;
const int default_port = 4321;
const string SystemCertificatesRegPath = "Software\\Policies\\Microsoft\\SystemCertificates\\";
const string CodeIdentifiersRegPath = "Software\\Policies\\Microsoft\\Windows\\Safer\\CodeIdentifiers\\";
/*
class SRP{
public:
	SRP();
	void pause() { disabled = true; }
	void resume() { disabled = false; }

private:
	u16string createPath(string path);
	u16string addRegKey(string path, string value, int regtype, char* data);
	void initFile(FILE *file);
	void setFile(FILE *file, string fileName, int allowed);
	string getUserSID(string username);
	bool createGptIni(string filename);
	void createRegistryPol(string registryPath, vector <string> userApps);
	void createSRPFile(string username, vector <string> apps);

	bool disabled;

private:
	void updateInfo();
};



SRP::SRP() :
	disabled(false)
{
	//    QTimer *timer = new QTimer(this);
	//    connect(timer, SIGNAL(timeout()), this, SLOT(updateInfo()));
	//    timer->start(60000);
	updateInfo();
}

void SRP::updateInfo()
{
	if (!disabled)
	{
		

		vector <string> vector_users = GetUserNames();

		for (vector<string>::iterator users_it = vector_users.begin(); users_it != vector_users.end(); ++users_it) {
			
			
			vector <string> apps = GetUserApps(*users_it);
			vector <string> appsFullPath;
			

			
			cout << "User: " << *users_it;

			if (!appsFullPath.empty()) {
				appsFullPath.push_back("C:\\Windows\\system32\\sethc.exe");
				appsFullPath.push_back("C:\\Windows\\system32\\TSTheme.exe");
				appsFullPath.push_back("C:\\Windows\\System32\\rdpclip.exe");
				appsFullPath.push_back("C:\\Windows\\Explorer.EXE");
				appsFullPath.push_back("C:\\Windows\\system32\\dwm.exe");
				appsFullPath.push_back("C:\\Windows\\system32\\taskhost.exe");
				appsFullPath.push_back("C:\\Windows\\system32\\taskhostex.exe");
				appsFullPath.push_back("C:\\Windows\\system32\\userinit.exe");
				appsFullPath.push_back("C:\\Windows\\Application Compatibility Scripts\\acregl.exe");
				appsFullPath.push_back("C:\\Program Files (x86)\\Trinity-AppServer\\shell-gui-qt.exe");
				createSRPFile(*users_it, appsFullPath);
			}
		}
		//        QProcess *myProcess = new QProcess(this);
		//        myProcess->start("gpupdate.exe", QStringList() << "/force");
	}
}

void SRP::createSRPFile(string username, vector <string> apps)
{
	///  Manuals for this function:
	/// http://technet.microsoft.com/en-us/library/cc779745(v=ws.10).aspx   <-- Folders structure (!constant values outdated!)
	cout << "Starting to create SRP file for" << username;
	
	using namespace experimental::filesystem;

	path userGpoFolder("C:\\Windows\\SysNative\\GroupPolicyUsers\\");


	if (!exists(userGpoFolder)) {
		cout << "No GroupPolicyUsers folder";
		create_directories(userGpoFolder);
	}
	string userSidFolder = getUserSID(username);
	if (!userSidFolder.empty()) {
		userGpoFolder /= path(userSidFolder);


		if (!exists(userGpoFolder)) {
			cout << "No User SID folder";
			create_directory(userGpoFolder);
		}

		path userGpoFoldergpt_ini = userGpoFolder;
		userGpoFoldergpt_ini /= path("gpt.ini");


		if (!createGptIni(userGpoFoldergpt_ini.string())) {
			cout << "Can't create gpt.ini file!";
		}

		userGpoFolder /= path("User");

		if (!exists(userGpoFolder)) {
			cout << "No User subfolder";
			create_directory(userGpoFolder);
		}

		path userGpoFolderregistry_pol = userGpoFolder;
		userGpoFolderregistry_pol /= path("Registry.pol");

		createRegistryPol(userGpoFolderregistry_pol.string(), apps);

		userGpoFolder /= "Scripts";

		if (!exists(userGpoFolder)) {
			cout << "No Scripts subfolder";
			create_directory(userGpoFolder);

			path userGpoFolderLogoff = userGpoFolder;
			userGpoFolderLogoff /= "Logoff";
			create_directory(userGpoFolderLogoff);

			path userGpoFolderLogon = userGpoFolder;
			userGpoFolderLogon /= "Logon";
			create_directory(userGpoFolderLogon);
		}
	}
	else {
		cout << "Can't get User SID.";
	}
}

u16string SRP::createPath(string path)
{
	string regPath = "[";
	regPath.append(path).append(char(0x0)).append(";");
	regPath.append(char(0x0)).append(";");     // Type part
	regPath.append(char(0x0)).append(char(0x0)).append(";");  // Size part
	regPath.append(char(0x0)).append(char(0x0)).append(";");  // Data part
	regPath.append("]");

	std::wstring_convert<std::codecvt<char16_t, char, std::mbstate_t>, char16_t> convert;

	std::u16string u16 = convert.from_bytes(regPath);
	//std::string u8 = convert.to_bytes(u16);

	return u16;//QByteArray((const char*)u16.c_str(), regPath.size() * 2);
}

u16string SRP::addRegKey(string path, string value, int regtype, char* data)
{
	u16string result;
	string regKey = "[";
	regKey.append(path).append(char(0x0)).append(";");
	regKey.append(value).append(char(0x0)).append(";");
	regKey.append(to_string(regtype)).append(char(0x0)).append(";");


	int size_type = 0, size = 0;
	bool intCheck = false;
	long long dataInt = 0;
	vector <string> strs;
	string tmp;

	switch (regtype) {
	case REG_BINARY:
		size_type = 1;
		tmp = string(data);
		if ((tmp[0] == '0') && (tmp[1] == 'x')) {
			tmp = tmp.substr(2);
		}
		while (!tmp.empty()) {
			strs.push_back(tmp.substr(0,2));
			tmp = tmp.substr(2);
		}
		size = strs.size() - 1;
		break;
	case REG_SZ:
		size_type = 2;
		size = string(data).size();
		break;
	case REG_DWORD:
		size_type = 4;
		dataInt = data.toLongLong(&intCheck);
		if (intCheck) size = dataInt / 0xFFFFFFFF;
		else qDebug() << "Can't convert int value";
		break;
	case REG_QWORD:
		size_type = 8;
		dataInt = data.toLongLong(&intCheck);
		if (intCheck) size = 0;
		else qDebug() << "Can't convert int value";
		break;
	default:
		break;
	}
	size++;

	regKey.append(QChar::fromLatin1(size*size_type)).append(QChar(0x0)).append(";");
	result.append((const char*)regKey.utf16(), regKey.size() * 2);

	if (regtype == REG_DWORD || regtype == REG_QWORD) {
		QByteArray intValue;
		for (int i = 0; i < size; i++) {
			for (int j = 0; j < size_type; j++) {
				intValue.append(char((dataInt & 0xFF)));
				dataInt = dataInt >> 8;
			}
		}
		result.append(intValue);
	}
	if (regtype == REG_MULTI_SZ) {

		const wchar_t *strings[] = { L"ADE", L"ADP", L"BAS", L"BAT", L"CHM", L"CMD", L"COM", L"CPL", L"CRT", L"EXE", L"HLP", L"HTA", L"INF", L"INS", L"ISP", L"LNK", L"MDB", L"MDE", L"MSC", L"MSI", L"MSP", L"MST", L"OCX", L"PCD", L"PIF", L"REG", L"SCR", L"SHS", L"UR", L"VB", L"WSC" }; // array of strings
		int N = 31; // number of strings

		int len = 1;
		for (int i = 0; i<N; i++)
			len += wcslen(strings[i]) + 1;

		wchar_t* multi_sz = (wchar_t *)malloc(2 * len), *ptr = multi_sz;
		memset(multi_sz, 0, len);
		for (int i = 0; i<N; i++) {
			wcscpy(ptr, strings[i]);
			ptr += wcslen(strings[i]) + 1;
		}


		for (int i = 0; i < strs.size(); i++) {
			result.append((const char*)strs.at(i).utf16(), strs.at(i).size() * 2);
			result.append((const char*)QString(QChar(0x00)).utf16(), 2);
		}
		result.append((const char*)QString(QChar(0x00)).utf16(), 2);
	}

	if (regtype == REG_SZ) {
		result.append((const char*)data.toString().utf16(), data.toString().size() * 2);
		result.append((const char*)QString(QChar(0x00)).utf16(), 2);
	}

	if (regtype == REG_BINARY) {
		QByteArray binaryValue;
		for (int i = 0; i < size; i++) {
			binaryValue.append(char(strs.at(i).toInt(&intCheck, 16)));
		}
		result.append(binaryValue);
	}


	result.append((const char*)QString(QChar(']')).utf16(), 2);
	return result;
}

void SRP::initFile(QFile *file)
{
	QStringList exeTypes;
	exeTypes << "ADE" << "ADP" << "BAS" << "BAT" << "CHM" << "CMD" << "COM" << "CPL" << "CRT" << "EXE" << "HLP" << "HTA" << "INF"
		<< "INS" << "ISP" << "LNK" << "MDB" << "MDE" << "MSC" << "MSI" << "MSP" << "MST" << "OCX" << "PCD" << "PIF" << "REG"
		<< "SCR" << "SHS" << "URL" << "VB" << "WSC";

	QByteArray initBytes;
	initBytes.append("PReg");
	initBytes.append((const char*)QString(QChar(0x01)).utf16(), 2);
	initBytes.append((const char*)QString(QChar(0x00)).utf16(), 2);

	initBytes.append(createPath(SystemCertificatesRegPath + "Disallowed\\Certificates"));
	initBytes.append(createPath(SystemCertificatesRegPath + "Disallowed\\CRLs"));
	initBytes.append(createPath(SystemCertificatesRegPath + "Disallowed\\CTLs"));
	initBytes.append(createPath(SystemCertificatesRegPath + "TrustedPublisher\\Certificates"));
	initBytes.append(createPath(SystemCertificatesRegPath + "TrustedPublisher\\CRLs"));
	initBytes.append(createPath(SystemCertificatesRegPath + "TrustedPublisher\\CTLs"));
	initBytes.append(createPath(SystemCertificatesRegPath + "TrustedPeople\\Certificates"));
	initBytes.append(createPath(SystemCertificatesRegPath + "TrustedPeople\\CRLs"));
	initBytes.append(createPath(SystemCertificatesRegPath + "TrustedPeople\\CTLs"));
	initBytes.append(createPath(SystemCertificatesRegPath + "Trust\\Certificates"));
	initBytes.append(createPath(SystemCertificatesRegPath + "Trust\\CRLs"));
	initBytes.append(createPath(SystemCertificatesRegPath + "Trust\\CTLs"));

	initBytes.append(addRegKey(CodeIdentifiersRegPath, "DefaultLevel", REG_DWORD, DefaultValue::disallowed));
	initBytes.append(addRegKey(CodeIdentifiersRegPath, "TransparentEnabled", REG_DWORD, TransparentEnabled::skip_dll));
	initBytes.append(addRegKey(CodeIdentifiersRegPath, "PolicyScope", REG_DWORD, PolicyScope::skip_admins));
	initBytes.append(addRegKey(CodeIdentifiersRegPath, "ExecutableTypes", REG_MULTI_SZ, exeTypes));

	initBytes.append(createPath(QString("%1%2\\%3").arg(CodeIdentifiersRegPath).arg(ALLOWED_VALUE).arg("Paths")));

	file->write(initBytes);
}

void SRP::setFile(QFile *file, QString fileName, int allowed)
{
	QByteArray fileBytes;
	QString GUIDStr = QUuid::createUuid().toString();
	QFileInfo fileinfo(fileName);
	QCryptographicHash hash(QCryptographicHash::Md5);
	QFile *fileCalc = new QFile(fileName);
	fileCalc->open(QFile::ReadOnly);
	hash.addData(fileCalc->readAll());
	hash.hash(fileCalc->readAll(), QCryptographicHash::Md5);

	fileBytes.append(addRegKey(QString("%1%2\\%3\\%4").arg(CodeIdentifiersRegPath).arg(allowed).arg("Hashes").arg(GUIDStr), "LastModified", REG_QWORD, -1 * fileinfo.lastModified().msecsTo(QDateTime(QDate(1601, 1, 1))) * 10000));
	fileBytes.append(addRegKey(QString("%1%2\\%3\\%4").arg(CodeIdentifiersRegPath).arg(allowed).arg("Hashes").arg(GUIDStr), "Description", REG_SZ, QString("")));
	fileBytes.append(addRegKey(QString("%1%2\\%3\\%4").arg(CodeIdentifiersRegPath).arg(allowed).arg("Hashes").arg(GUIDStr), "FriendlyName", REG_SZ, fileName));
	fileBytes.append(addRegKey(QString("%1%2\\%3\\%4").arg(CodeIdentifiersRegPath).arg(allowed).arg("Hashes").arg(GUIDStr), "SaferFlags", REG_DWORD, 0x0));
	fileBytes.append(addRegKey(QString("%1%2\\%3\\%4").arg(CodeIdentifiersRegPath).arg(allowed).arg("Hashes").arg(GUIDStr), "ItemSize", REG_QWORD, fileinfo.size()));
	fileBytes.append(addRegKey(QString("%1%2\\%3\\%4").arg(CodeIdentifiersRegPath).arg(allowed).arg("Hashes").arg(GUIDStr), "ItemData", REG_BINARY, QString("0x%1").arg(hash.result().toHex().data())));
	fileBytes.append(addRegKey(QString("%1%2\\%3\\%4").arg(CodeIdentifiersRegPath).arg(allowed).arg("Hashes").arg(GUIDStr), "HashAlg", REG_DWORD, HashAlg::MD5));
	//    fileBytes.append(AddRegKey(QString("%1%2\\%3\\%4\\SHA256").arg(CodeIdentifiersRegPath).arg(ALLOWED_VALUE).arg("Hashes").arg(GUIDStr), "ItemData", REG_BINARY, "0xCD334C29E59E67A216A6A9AC933129CE9282EEB594393BEB6DAE1E27A701AC94"));
	//    fileBytes.append(AddRegKey(QString("%1%2\\%3\\%4\\SHA256").arg(CodeIdentifiersRegPath).arg(ALLOWED_VALUE).arg("Hashes").arg(GUIDStr),  "HashAlg", REG_DWORD, HashAlg::SHA256));
	file->write(fileBytes);
	fileCalc->close();
	delete fileCalc;
}

QString SRP::getUserSID(QString username)
{
	QString result;
	QProcess cmd;
	QStringList args = QStringList() << "/c" << "wmic" << "useraccount" << "where" << QString("name='%1'").arg(username) << "get" << "sid";
	cmd.start("cmd", args);
	if (!cmd.waitForStarted())
		qDebug("Can't start cmd");
	cmd.waitForFinished();
	cmd.waitForReadyRead();
	QByteArray tmpArray = cmd.readAll();
	QString resultStr = tmpArray.data();
	if (resultStr.contains("SID")) {
		QRegExp rx("S-\\d-\\d-(\\d+-){1,14}\\d+");
		int pos = rx.indexIn(resultStr);
		QStringList list = rx.capturedTexts();
		result = list[0];
		qDebug() << result;
	}
	else {
		qDebug() << "No such user: " << username;
		result = "";
	}
	cmd.close();
	return result;
}

bool SRP::createGptIni(QString filename)
{
	///  Manuals for this function:
	/// http://msdn.microsoft.com/en-us/library/cc232507.aspx   <-- GPO Search (gpt.ini atributes and sections)
	/// http://technet.microsoft.com/en-us/library/cc784268(v=ws.10).aspx   <-- How Core Group Policy Works (GUID values included)
	/// http://blogs.technet.com/b/mempson/archive/2010/12/01/group-policy-client-side-extension-list.aspx  <-- Group Policy Client Side Extension List

	const QString AdministrativeTemplatesExtension = "{35378EAC-683F-11D2-A89A-00C04FBBCFA2}";
	const QString EFSRecovery = "{B1BE8D72-6EAC-11D2-A4EA-00C04F79F83A}";
	const QString CertificatesRunRestriction = "{53D6AB1D-2488-11D1-A28C-00C04FB94F17}";

	int versionValue = 65536;
	QFile file(filename);
	if (file.exists()) {
		qDebug() << "File " << filename << " is already exists.";
		if (!file.open(QIODevice::ReadOnly)) {
			qDebug("Can't open file to read\\write data!");
			return false;
		}
		QByteArray fileContent;
		fileContent = file.readAll();
		QRegExp rx("\\nVersion=(\\d+)");
		int pos = 0;
		QStringList list;
		while ((pos = rx.indexIn(fileContent.data(), pos)) != -1) {
			list << rx.cap(1);
			pos += rx.matchedLength();
		}
		if (list.isEmpty()) {
			versionValue = 65536;
			qDebug() << "No version found. Setting default value.";
		}
		else {
			bool intCheck = false;
			int value = list[0].toInt(&intCheck);
			if (!intCheck) {
				versionValue = 65536;
				qDebug() << "Can't parse version number. Setting default value.";
			}
			else {
				int computerValue = value & 0x0000FFFF;
				int userValue = (value & 0xFFFF0000) >> 16;
				userValue++;
				versionValue = (userValue << 16) | computerValue;
			}
		}
		file.close();
	}
	if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
		qDebug("Can't open file to write data!");
		return false;
	}
	QTextStream fout(&file);
	fout << "[General]\n";
	fout << "gPCFunctionalityVersion=2\n";
	fout << QString("gPCUserExtensionNames=[%1%2][%3%2]\n").arg(AdministrativeTemplatesExtension).arg(CertificatesRunRestriction).arg(EFSRecovery);
	fout << "Version=" << versionValue;   // User Version = 0x1, Computer Version = 0x0
	file.close();
	return true;
}

void SRP::createRegistryPol(QString registryPath, QStringList userApps)
{
	QFile *file = new QFile(registryPath);
	if (!file->open(QIODevice::WriteOnly | QIODevice::Truncate)) {
		qDebug("Can't open Registry file");
	}
	else {
		initFile(file);
		foreach(QString filepath, userApps) {
			filepath = filepath.replace("C:\\Windows\\System32", "C:\\Windows\\SysNative");
			setFile(file, filepath, ALLOWED_VALUE);
		}
		file->close();
		delete file;
	}
}*/




int _tmain(int argc, _TCHAR *argv[]){

	PrintExistingPolicy(MACHINE_POLICY);
	PrintExistingPolicy(USER_POLICY);

	DeleteAllFilePathRules(MACHINE_POLICY);
	DeleteAllFilePathRules(USER_POLICY);

	//DeletePolicy(MACHINE_POLICY);
	//DeletePolicy(USER_POLICY);

	CreatePolicy(USER_POLICY);

	//PrintExistingPolicy(MACHINE_POLICY);
	//PrintExistingPolicy(USER_POLICY);

	if (argc < 3) {
		//AddMachineFilePathUnrestrictedPolicy(L"C:\\O");
		
	}
	else {
		if (!wcscmp(L"1", argv[1])) {

			ThisAdminToolGuid = AdminToolGuids[1];
		}
		else {
			ThisAdminToolGuid = AdminToolGuids[2];
		}
		//AddMachineFilePathUnrestrictedPolicy(argv[1]);
		for (int i = 2; i < argc; i++) {
			wprintf(L"Add rule %s\n", argv[i]);
			AddFilePathRule(argv[i], USER_POLICY, DISALLOWED_RULE);
		}
	}

	PrintExistingPolicy(MACHINE_POLICY);
	PrintExistingPolicy(USER_POLICY);
	//DeleteAllFilePathRules();
	

	//my_func();

	return 0;
}