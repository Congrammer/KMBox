#include <iostream>
#include <string>

#include <Windows.h>
#include <setupapi.h>
#include <devguid.h>
#pragma comment(lib, "setupapi.lib")

using namespace std;

bool isHoldingAimKey = false;
int MouseTargetX = 5;
int MouseTargetY = 5;

string GetCOMPortByDescription(const string& targetDescription) {
	HDEVINFO hDevInfo = SetupDiGetClassDevsA(&GUID_DEVCLASS_PORTS, 0, 0, DIGCF_PRESENT);
	if (hDevInfo == INVALID_HANDLE_VALUE) return "";

	SP_DEVINFO_DATA deviceInfoData;
	deviceInfoData.cbSize = sizeof(SP_DEVINFO_DATA);

	for (DWORD i = 0; SetupDiEnumDeviceInfo(hDevInfo, i, &deviceInfoData); ++i) {
		char buf[512];
		DWORD nSize = 0;

		if (SetupDiGetDeviceRegistryPropertyA(hDevInfo, &deviceInfoData, SPDRP_FRIENDLYNAME, NULL, (PBYTE)buf, sizeof(buf), &nSize) && nSize > 0) {
			buf[nSize] = '\0';
			std::string deviceDescription = buf;

			size_t comPos = deviceDescription.find("COM");
			size_t endPos = deviceDescription.find(")", comPos);

			if (comPos != std::string::npos && endPos != std::string::npos && deviceDescription.find(targetDescription) != std::string::npos) {
				SetupDiDestroyDeviceInfoList(hDevInfo);
				return deviceDescription.substr(comPos, endPos - comPos);
			}
		}
	}

	SetupDiDestroyDeviceInfoList(hDevInfo);
	return "";
}


bool OpenSerialPort(HANDLE& hSerial, const char* portName, DWORD baudRate) {
	hSerial = CreateFileA(portName, GENERIC_READ | GENERIC_WRITE, 0, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);

	if (hSerial == INVALID_HANDLE_VALUE) return false;

	DCB dcbSerialParams = { 0 };
	dcbSerialParams.DCBlength = sizeof(dcbSerialParams);

	if (!GetCommState(hSerial, &dcbSerialParams)) {
		CloseHandle(hSerial);
		return false;
	}

	dcbSerialParams.BaudRate = baudRate;
	dcbSerialParams.ByteSize = 8;
	dcbSerialParams.StopBits = ONESTOPBIT;
	dcbSerialParams.Parity = NOPARITY;

	if (!SetCommState(hSerial, &dcbSerialParams)) {
		CloseHandle(hSerial);
		return false;
	}

	COMMTIMEOUTS timeouts = { 0 };
	timeouts.ReadIntervalTimeout = 5;
	timeouts.ReadTotalTimeoutConstant = 5;
	timeouts.ReadTotalTimeoutMultiplier = 5;
	timeouts.WriteTotalTimeoutConstant = 5;
	timeouts.WriteTotalTimeoutMultiplier = 5;

	if (!SetCommTimeouts(hSerial, &timeouts)) {
		std::cerr << "Error setting timeouts!" << std::endl;
		CloseHandle(hSerial);
		return false;
	}

	return true;
}

void SendCommand(HANDLE hSerial, const std::string& command) {
	DWORD bytesWritten;
	if (!WriteFile(hSerial, command.c_str(), command.length(), &bytesWritten, NULL)) {
		std::cerr << "Failed to write to serial port!" << std::endl;
	}
}


void ConnectKMBOXThread()
{
	string port = GetCOMPortByDescription("USB-SERIAL CH340");
	if (port.empty()) {
		std::cerr << "Failed to find COM port with description USB-SERIAL CH340!" << std::endl;
		return;
	}
	//printf("\nFound port: %s", port.c_str());

	HANDLE hSerial;
	if (!OpenSerialPort(hSerial, port.c_str(), CBR_115200)) {
		std::cerr << "Failed to open serial port!" << std::endl;
		return;
	}

	printf("\n	[+] KMBOX Connected");
	SendCommand(hSerial, "km.help()\r\n");
	SendCommand(hSerial, "km.move(100,100,10)\r\n");
	SendCommand(hSerial, "km.lock_ms1(1)\r\n");

	/* 
	* Optional device PID / VID change
		SendCommand(hSerial, "device.PID('1007')\r\n");
		SendCommand(hSerial, "device.VID('258A')\r\n");
		SendCommand(hSerial, "device.enable(1)\r\n");
	*/

	char readBuffer[256];
	DWORD bytesRead;

	while (true) {

		SendCommand(hSerial, "km.side1()\r\n");

		if (!ReadFile(hSerial, readBuffer, sizeof(readBuffer) - 1, &bytesRead, NULL)) {
			std::cerr << "Failed to read from serial port!" << std::endl;
			break;
		}

		if (bytesRead > 0) {
			readBuffer[bytesRead] = '\0';
			if (strstr(readBuffer, "km.side1()\r\n1"))
			{
				isHoldingAimKey = true;
				if (MouseTargetX || MouseTargetY)
				{
					std::string command = "km.move(" + std::to_string(MouseTargetX) + "," + std::to_string(MouseTargetY) + ")\r\n";
					SendCommand(hSerial, command.c_str());
				}
			}
			else
				isHoldingAimKey = false;

			//std::cout << "Received: " << readBuffer << std::endl;
		}
	}
}

void KMBox() {
	thread kmboxThread(ConnectKMBOXThread);
	kmboxThread.detach();  // Run thread in the background
}


int main()
{
    std::cout << "Hello World!\n";
	KMBox();
	while (true) Sleep(10000);
}
