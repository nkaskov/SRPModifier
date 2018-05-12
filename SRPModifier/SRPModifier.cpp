#include <initguid.h>
#include <gpedit.h>
#include <objbase.h>
#include <stdio.h>
void my_func_1() {
	DWORD val, val_size = sizeof(DWORD);
	HRESULT hr = S_OK;
	IGroupPolicyObject* pLGPO;
	HKEY machine_key, dsrkey;
	// MSVC is finicky about these ones => redefine them
	GUID RegistryId = REGISTRY_EXTENSION_GUID;
	// This next one can be any GUID you want
	GUID ThisAdminToolGuid = { 0x3d271cfc, 0x2bc6, 0x4ac2,{ 0xb6, 0x33, 0x3b, 0xdf, 0xf5, 0xbd, 0xab, 0x2a } };

	// Create an instance of the IGroupPolicyObject class
	CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
	hr = CoCreateInstance(CLSID_GroupPolicyObject, NULL, CLSCTX_INPROC_SERVER,
		IID_IGroupPolicyObject, (LPVOID*)&pLGPO);

	if (SUCCEEDED(hr)) {
		DWORD dwSection = GPO_SECTION_MACHINE; //GPO_SECTION_USER;

		// We need the machine LGPO (if C++, no need to go through the lpVtbl table)
		hr = pLGPO->OpenLocalMachineGPO(GPO_OPEN_LOAD_REGISTRY);
		
		printf("Open Local Machine GPO %d\n", SUCCEEDED(hr));

		hr = pLGPO->GetRegistryKey(dwSection, &machine_key);

		printf("Get Registry Key %d\n", SUCCEEDED(hr));

		// The disable System Restore is a DWORD value of Policies\Microsoft\Windows\DeviceInstall\Settings
		hr = RegCreateKeyEx(machine_key, L"Software\\Policies\\Microsoft\\Windows\\Safer\\CodeIdentifiers\\0\\Paths\\{fc94f449-788f-4e3d-815e-fad5235ccbfa}",
			0, NULL, 0, KEY_SET_VALUE | KEY_QUERY_VALUE, NULL, &dsrkey, NULL);

		printf("Created Registry Key %d GLE=%d\n", SUCCEEDED(hr), GetLastError());

		// Create the value
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

void my_func() {
	DWORD val, val_size = sizeof(DWORD);
	HRESULT hr = S_OK;
	IGroupPolicyObject* pLGPO;
	HKEY machine_key, dsrkey;
	// MSVC is finicky about these ones => redefine them
	GUID RegistryId = REGISTRY_EXTENSION_GUID;
	// This next one can be any GUID you want
	GUID ThisAdminToolGuid = { 0x3d271cfc, 0x2bc6, 0x4ac2,{ 0xb6, 0x33, 0x3b, 0xdf, 0xf5, 0xbd, 0xab, 0x2a } };

	// Create an instance of the IGroupPolicyObject class
	CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
	hr = CoCreateInstance(CLSID_GroupPolicyObject, NULL, CLSCTX_INPROC_SERVER,
		IID_IGroupPolicyObject, (LPVOID*)&pLGPO);

	if (SUCCEEDED(hr)) {
		DWORD dwSection = GPO_SECTION_MACHINE; //GPO_SECTION_USER;

											   // We need the machine LGPO (if C++, no need to go through the lpVtbl table)
		hr = pLGPO->OpenLocalMachineGPO(GPO_OPEN_LOAD_REGISTRY);

		printf("Open Local Machine GPO %d\n", SUCCEEDED(hr));

		hr = pLGPO->GetRegistryKey(dwSection, &machine_key);

		printf("Get Registry Key %d\n", SUCCEEDED(hr));

		// The disable System Restore is a DWORD value of Policies\Microsoft\Windows\DeviceInstall\Settings
		hr = RegCreateKeyEx(machine_key, L"Software\\Policies\\Microsoft\\Windows\\Safer\\CodeIdentifiers\\0\\Paths\\{fc94f449-788f-4e3d-815e-fad5235ccbfa}",
			0, NULL, 0, KEY_SET_VALUE | KEY_QUERY_VALUE, NULL, &dsrkey, NULL);

		printf("Created Registry Key %d GLE=%d\n", SUCCEEDED(hr), GetLastError());

		// Create the value
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


int main() {
	Sleep(1000);
	my_func();
	return 0;
}