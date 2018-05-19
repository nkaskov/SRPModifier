#include <initguid.h>
#include <gpedit.h>
#include <objbase.h>
#include <stdio.h>
#include <string>
#include <tchar.h>
#include <time.h>
#include <random>

#define MAX_KEY_LENGTH 255
#define MAX_VALUE_LENGTH 255

#define MACHINE_POLICY 0
#define USER_POLICY 1

#define UNRESTRICTED_POLICY 0
#define BASIC_USER_POLICY 1
#define DISALLOWED_POLICY 2

#define UNRESTRICTED_RULE 0
#define BASIC_USER_RULE 1
#define DISALLOWED_RULE 2


using namespace std;

void CreatePolicy(DWORD policy_type = MACHINE_POLICY, DWORD policy_access = UNRESTRICTED_POLICY) {
	HRESULT hr = S_OK;
	IGroupPolicyObject* pLGPO;
	HKEY gpo_key, dsrkey;
	// MSVC is finicky about these ones => redefine them
	GUID RegistryId = REGISTRY_EXTENSION_GUID;
	// This next one can be any GUID you want
	GUID ThisAdminToolGuid = { 0x3d271cfc, 0x2bc6, 0x4ac2,{ 0xb6, 0x33, 0x3b, 0xdf, 0xf5, 0xbd, 0xab, 0x2a } };

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
			dwSection = GPO_SECTION_MACHINE;
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
		int N = 32; // number of strings

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

void DeletePolicy(DWORD policy_type = MACHINE_POLICY) {
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

	wprintf(L"Generated name %s\n", new_path_name);

	return new_path_name;
	//return L"{748a4186-9365-431d-a3db-1faecd80cca5}";
}

void DeleteFilePathRule(LPCTSTR file_path, DWORD policy_type = MACHINE_POLICY) {
	HRESULT hr = S_OK;
	IGroupPolicyObject* pLGPO;
	HKEY gpo_key;

	GUID RegistryId = REGISTRY_EXTENSION_GUID;

	GUID ThisAdminToolGuid = { 0x3d271cfc, 0x2bc6, 0x4ac2,{ 0xb6, 0x33, 0x3b, 0xdf, 0xf5, 0xbd, 0xab, 0x2a } };

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
			dwSection = GPO_SECTION_MACHINE;
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

void DeleteAllFilePathRules(DWORD policy_type = MACHINE_POLICY) {
	HRESULT hr = S_OK;
	IGroupPolicyObject* pLGPO;
	HKEY gpo_key;

	GUID RegistryId = REGISTRY_EXTENSION_GUID;

	GUID ThisAdminToolGuid = { 0x3d271cfc, 0x2bc6, 0x4ac2,{ 0xb6, 0x33, 0x3b, 0xdf, 0xf5, 0xbd, 0xab, 0x2a } };

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
			dwSection = GPO_SECTION_MACHINE;
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

void AddFilePathRule(LPCWSTR new_path, DWORD policy_type = MACHINE_POLICY, DWORD rule_access = UNRESTRICTED_RULE) {
	HRESULT hr = S_OK;
	IGroupPolicyObject* pLGPO;
	HKEY gpo_key, path_section_key, path_key;

	GUID RegistryId = REGISTRY_EXTENSION_GUID;
	// This next one can be any GUID you want
	GUID ThisAdminToolGuid = { 0x3d271cfc, 0x2bc6, 0x4ac2,{ 0xb6, 0x33, 0x3b, 0xdf, 0xf5, 0xbd, 0xab, 0x2a } };

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
			dwSection = GPO_SECTION_MACHINE;
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

void my_func() {
	HRESULT hr = S_OK;
	IGroupPolicyObject* pLGPO;
	HKEY machine_key, dsrkey;

	GUID RegistryId = REGISTRY_EXTENSION_GUID;
	
	GUID ThisAdminToolGuid = { 0x3d271cfc, 0x2bc6, 0x4ac2,{ 0xb6, 0x33, 0x3b, 0xdf, 0xf5, 0xbd, 0xab, 0x2a } };

	// Create an instance of the IGroupPolicyObject class
	CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
	hr = CoCreateInstance(CLSID_GroupPolicyObject, NULL, CLSCTX_INPROC_SERVER,
		IID_IGroupPolicyObject, (LPVOID*)&pLGPO);

	if (SUCCEEDED(hr)) {
		DWORD dwSection = GPO_SECTION_USER;//GPO_SECTION_MACHINE;

											   // We need the machine LGPO (if C++, no need to go through the lpVtbl table)
		hr = pLGPO->OpenLocalMachineGPO(GPO_OPEN_LOAD_REGISTRY);

		hr = pLGPO->GetRegistryKey(dwSection, &machine_key);

		LPTSTR *file_path_names_list = GetListOfExistFilePathRules(machine_key, UNRESTRICTED_RULE);

		int path_number = 0;
		LPTSTR file_path_name;

		while (file_path_name = file_path_names_list[path_number++]) {
			wprintf(L"%s\n", file_path_name);
		}

		file_path_names_list = GetListOfExistFilePathRules(machine_key, DISALLOWED_RULE);

		path_number = 0;

		while (file_path_name = file_path_names_list[path_number++]) {
			wprintf(L"%s\n", file_path_name);
		}

		return;

		// The disable System Restore is a DWORD value of Policies\Microsoft\Windows\DeviceInstall\Settings
		hr = RegCreateKeyEx(machine_key, L"Software\\Policies\\Microsoft\\Windows\\Safer\\CodeIdentifiers\\0\\Paths\\{fc94f449-788f-4e3d-815e-fad5235ccbfa}",
			0, NULL, 0, KEY_SET_VALUE | KEY_QUERY_VALUE, NULL, &dsrkey, NULL);

		printf("Created Registry Key %d GLE=%d\n", SUCCEEDED(hr), GetLastError());

		//begin writing 
		const wchar_t * val_sz = L"C:\\M";
		hr = RegSetKeyValue(dsrkey, NULL, L"ItemData", REG_SZ, val_sz, 2 * wcslen(val_sz));

		printf("Set Registry Key Value %d GLE=%d\n", SUCCEEDED(hr), GetLastError());

		RegCloseKey(dsrkey);

		// Apply policy and free resources
		hr = pLGPO->Save(TRUE, TRUE, &RegistryId, &ThisAdminToolGuid);

		printf("Saved changes %d GLE=%d\n", SUCCEEDED(hr), GetLastError());

		RegCloseKey(machine_key);

		pLGPO->Release();

	}

}


int _tmain(int argc, _TCHAR *argv[]) {
	Sleep(1000);

	DeleteAllFilePathRules();

	if (argc < 2) {
		//AddMachineFilePathUnrestrictedPolicy(L"C:\\O");
		AddFilePathRule(L"C:\\A");
	}
	else {
		//AddMachineFilePathUnrestrictedPolicy(argv[1]);
		for (int i = 1; i < argc; i++) {
			wprintf(L"Add rule %s", argv[i]);
			AddFilePathRule(argv[i], USER_POLICY);
		}
	}

	my_func();
	
	//DeleteAllFilePathRules();
	
	//my_func();

	return 0;
}