/* 
	Copyright (C) 2014 Suguru Kawamoto
	This software is released under the MIT License.
*/

#include <stdio.h>
#include <tchar.h>
#include <windows.h>
#include <setupapi.h>
#include <hidsdi.h>
#include <hidpi.h>
#include <mmsystem.h>

#pragma comment(lib, "setupapi.lib")
#pragma comment(lib, "hid.lib")
#pragma comment(lib, "winmm.lib")

typedef struct
{
	HANDLE HidDeviceObject;
	HIDP_CAPS Capabilities;
	DWORD Write;
	DWORD Read;
} USBIO20DATA;

BOOL OpenUSBIO20(void** Handle, DWORD Id)
{
	BOOL Result;
	DWORD Count;
	GUID HidGuid;
	HDEVINFO DeviceInfoSet;
	DWORD MemberIndex;
	SP_DEVICE_INTERFACE_DATA DeviceInterfaceData;
	DWORD RequiredSize;
	SP_DEVICE_INTERFACE_DETAIL_DATA* DeviceInterfaceDetailData;
	HANDLE HidDeviceObject;
	HIDD_ATTRIBUTES Attributes;
	PHIDP_PREPARSED_DATA PreparsedData;
	HIDP_CAPS Capabilities;
	USBIO20DATA* USBIO20;
	Result = FALSE;
	Count = 0;
	*Handle = NULL;
	HidD_GetHidGuid(&HidGuid);
	if(DeviceInfoSet = SetupDiGetClassDevs(&HidGuid, NULL, NULL, DIGCF_DEVICEINTERFACE | DIGCF_PRESENT))
	{
		MemberIndex = 0;
		DeviceInterfaceData.cbSize = sizeof(SP_DEVICE_INTERFACE_DATA);
		while(SetupDiEnumDeviceInterfaces(DeviceInfoSet, NULL, &HidGuid, MemberIndex, &DeviceInterfaceData))
		{
			RequiredSize = 0;
			SetupDiGetDeviceInterfaceDetail(DeviceInfoSet, &DeviceInterfaceData, NULL, 0, &RequiredSize, NULL);
			if(RequiredSize != 0)
			{
				if(DeviceInterfaceDetailData = (SP_DEVICE_INTERFACE_DETAIL_DATA*)malloc(RequiredSize))
				{
					DeviceInterfaceDetailData->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA);
					if(SetupDiGetDeviceInterfaceDetail(DeviceInfoSet, &DeviceInterfaceData, DeviceInterfaceDetailData, RequiredSize, NULL, NULL))
					{
						HidDeviceObject = CreateFile(DeviceInterfaceDetailData->DevicePath, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, 0);
						if(HidDeviceObject != INVALID_HANDLE_VALUE)
						{
							Attributes.Size = sizeof(HIDD_ATTRIBUTES);
							if(HidD_GetAttributes(HidDeviceObject, &Attributes))
							{
								if(Attributes.VendorID == 0x1352 && (Attributes.ProductID == 0x0120 || Attributes.ProductID == 0x0121))
								{
									if(HidD_GetPreparsedData(HidDeviceObject, &PreparsedData))
									{
										if(HidP_GetCaps(PreparsedData, &Capabilities) == HIDP_STATUS_SUCCESS)
										{
											if(Capabilities.InputReportByteLength >= 65 && Capabilities.OutputReportByteLength >= 65)
											{
												if(Count == Id)
												{
													if(USBIO20 = (USBIO20DATA*)malloc(sizeof(USBIO20DATA)))
													{
														USBIO20->HidDeviceObject = HidDeviceObject;
														USBIO20->Capabilities = Capabilities;
														USBIO20->Write = 0;
														USBIO20->Read = 0;
														*Handle = USBIO20;
														HidDeviceObject = INVALID_HANDLE_VALUE;
														Result = TRUE;
													}
												}
												Count++;
											}
										}
										HidD_FreePreparsedData(PreparsedData);
									}
								}
							}
							if(HidDeviceObject != INVALID_HANDLE_VALUE)
								CloseHandle(HidDeviceObject);
						}
					}
					free(DeviceInterfaceDetailData);
				}
			}
			MemberIndex++;
		}
		SetupDiDestroyDeviceInfoList(DeviceInfoSet);
	}
	return Result;
}

BOOL CloseUSBIO20(void* Handle)
{
	BOOL Result;
	USBIO20DATA* USBIO20;
	Result = FALSE;
	USBIO20 = (USBIO20DATA*)Handle;
	if(CloseHandle(USBIO20->HidDeviceObject))
		Result = TRUE;
	return Result;
}

BOOL LoadUSBIO20(void* Handle, BOOL* PullUp, DWORD* InputPin, DWORD Timeout)
{
	BOOL Result;
	USBIO20DATA* USBIO20;
	BYTE* Buffer;
	DWORD NumberOfBytesWritten;
	DWORD NumberOfBytesRead;
	Result = FALSE;
	USBIO20 = (USBIO20DATA*)Handle;
	Timeout = timeGetTime() + Timeout;
	if(Buffer = (BYTE*)malloc(USBIO20->Capabilities.OutputReportByteLength))
	{
		memset(Buffer, 0x00, (size_t)USBIO20->Capabilities.OutputReportByteLength);
		Buffer[0] = 0x00;
		Buffer[1] = 0xf8;
		Buffer[64] = 0x00;
		if(WriteFile(USBIO20->HidDeviceObject, Buffer, USBIO20->Capabilities.OutputReportByteLength, &NumberOfBytesWritten, NULL))
		{
			free(Buffer);
			if(Buffer = (BYTE*)malloc(USBIO20->Capabilities.InputReportByteLength))
			{
				memset(Buffer, 0xff, (size_t)USBIO20->Capabilities.InputReportByteLength);
				while(Timeout - timeGetTime() < 0x7fffffff)
				{
					if(ReadFile(USBIO20->HidDeviceObject, Buffer, USBIO20->Capabilities.InputReportByteLength, &NumberOfBytesRead, 0))
					{
						if(Buffer[64] == 0x00)
						{
							*PullUp = ((Buffer[3] & 0x01) ? TRUE : FALSE);
							*InputPin = ((DWORD)Buffer[6] & 0xff) | (((DWORD)Buffer[7] & 0x0f) << 8);
							Result = TRUE;
							break;
						}
					}
				}
				free(Buffer);
			}
		}
		else
			free(Buffer);
	}
	return Result;
}

BOOL SaveUSBIO20(void* Handle, BOOL PullUp, DWORD InputPin, DWORD Timeout)
{
	BOOL Result;
	USBIO20DATA* USBIO20;
	BYTE* Buffer;
	DWORD NumberOfBytesWritten;
	DWORD NumberOfBytesRead;
	Result = FALSE;
	USBIO20 = (USBIO20DATA*)Handle;
	Timeout = timeGetTime() + Timeout;
	if(Buffer = (BYTE*)malloc(USBIO20->Capabilities.OutputReportByteLength))
	{
		memset(Buffer, 0x00, (size_t)USBIO20->Capabilities.OutputReportByteLength);
		Buffer[0] = 0x00;
		Buffer[1] = 0xf9;
		Buffer[2] = 0x00;
		Buffer[3] = (PullUp ? 0x01 : 0x00);
		Buffer[4] = 0x00;
		Buffer[5] = 0x00;
		Buffer[6] = (BYTE)(InputPin & 0xff);
		Buffer[7] = (BYTE)((InputPin >> 8) & 0x0f);
		Buffer[64] = 0x00;
		if(WriteFile(USBIO20->HidDeviceObject, Buffer, USBIO20->Capabilities.OutputReportByteLength, &NumberOfBytesWritten, NULL))
		{
			free(Buffer);
			if(Buffer = (BYTE*)malloc(USBIO20->Capabilities.InputReportByteLength))
			{
				memset(Buffer, 0xff, (size_t)USBIO20->Capabilities.InputReportByteLength);
				while(Timeout - timeGetTime() < 0x7fffffff)
				{
					if(ReadFile(USBIO20->HidDeviceObject, Buffer, USBIO20->Capabilities.InputReportByteLength, &NumberOfBytesRead, 0))
					{
						if(Buffer[64] == 0x00)
						{
							Result = TRUE;
							break;
						}
					}
				}
				free(Buffer);
			}
		}
		else
			free(Buffer);
	}
	return Result;
}

BOOL WriteUSBIO20(void* Handle, DWORD Output, DWORD Timeout)
{
	BOOL Result;
	USBIO20DATA* USBIO20;
	BYTE* Buffer;
	DWORD NumberOfBytesWritten;
	Result = FALSE;
	USBIO20 = (USBIO20DATA*)Handle;
	Timeout = timeGetTime() + Timeout;
	if(Buffer = (BYTE*)malloc(USBIO20->Capabilities.OutputReportByteLength))
	{
		memset(Buffer, 0x00, (size_t)USBIO20->Capabilities.OutputReportByteLength);
		Buffer[0] = 0x00;
		Buffer[1] = 0x20;
		Buffer[2] = 0x01;
		Buffer[3] = (BYTE)(Output & 0xff);
		Buffer[4] = 0x02;
		Buffer[5] = (BYTE)((Output >> 8) & 0x0f);
		Buffer[64] = 0x00;
		while(Timeout - timeGetTime() < 0x7fffffff)
		{
			if(WriteFile(USBIO20->HidDeviceObject, Buffer, USBIO20->Capabilities.OutputReportByteLength, &NumberOfBytesWritten, NULL))
			{
				USBIO20->Write++;
				Result = TRUE;
				break;
			}
		}
		free(Buffer);
	}
	return Result;
}

BOOL ReadUSBIO20(void* Handle, DWORD* Input, DWORD Timeout)
{
	BOOL Result;
	USBIO20DATA* USBIO20;
	BYTE* Buffer;
	DWORD NumberOfBytesRead;
	Result = FALSE;
	USBIO20 = (USBIO20DATA*)Handle;
	Timeout = timeGetTime() + Timeout;
	if(Buffer = (BYTE*)malloc(USBIO20->Capabilities.InputReportByteLength))
	{
		memset(Buffer, 0xff, (size_t)USBIO20->Capabilities.InputReportByteLength);
		while(Timeout - timeGetTime() < 0x7fffffff)
		{
			if(USBIO20->Read == USBIO20->Write)
				break;
			if(ReadFile(USBIO20->HidDeviceObject, Buffer, USBIO20->Capabilities.InputReportByteLength, &NumberOfBytesRead, 0))
			{
				if(Buffer[64] == 0x00)
				{
					*Input = ((DWORD)Buffer[2] & 0xff) | (((DWORD)Buffer[3] & 0x0f) << 8);
					USBIO20->Read++;
					Result = TRUE;
					break;
				}
			}
		}
		free(Buffer);
	}
	return Result;
}

#define I2C_HIGH 1
#define I2C_LOW 0

void* g_Handle;
BOOL g_PullUp;
DWORD g_InputPin;
DWORD g_Output;
DWORD g_Input;

void Init()
{
	OpenUSBIO20(&g_Handle, 0);
	LoadUSBIO20(g_Handle, &g_PullUp, &g_InputPin, 1000);
	g_Output = 0x0000000a;
	WriteUSBIO20(g_Handle, g_Output | g_InputPin, 1000);
	ReadUSBIO20(g_Handle, &g_Input, 1000);
}

unsigned char GetSCL()
{
	return (g_Input & (1 << 0)) ? I2C_HIGH : I2C_LOW;
}

void SetSCL(unsigned char Status)
{
	g_Output = (g_Output & ~(1 << 1)) | (Status == I2C_LOW ? 0 : (1 << 1));
}

unsigned char GetSDA()
{
	return (g_Input & (1 << 2)) ? I2C_HIGH : I2C_LOW;
}

void SetSDA(unsigned char Status)
{
	g_Output = (g_Output & ~(1 << 3)) | (Status == I2C_LOW ? 0 : (1 << 3));
}

void WaitForQuarterClock()
{
	WriteUSBIO20(g_Handle, g_Output | g_InputPin, 1000);
	ReadUSBIO20(g_Handle, &g_Input, 1000);
}

void WaitForHalfClock()
{
	WaitForQuarterClock();
	WaitForQuarterClock();
}

void i2c_init()
{
	Init();
}

void i2c_stop()
{
	WaitForQuarterClock();
	if(GetSCL() != I2C_HIGH || GetSDA() != I2C_LOW)
	{
		SetSCL(I2C_LOW);
		WaitForQuarterClock();
		SetSDA(I2C_LOW);
		WaitForQuarterClock();
		SetSCL(I2C_HIGH);
		while(GetSCL() == I2C_LOW)
		{
			WaitForQuarterClock();
		}
		WaitForHalfClock();
	}
	SetSDA(I2C_HIGH);
	WaitForQuarterClock();
}

void i2c_start()
{
	WaitForQuarterClock();
	if(GetSCL() != I2C_HIGH || GetSDA() != I2C_HIGH)
	{
		SetSCL(I2C_LOW);
		WaitForQuarterClock();
		SetSDA(I2C_HIGH);
		WaitForQuarterClock();
		SetSCL(I2C_HIGH);
		while(GetSCL() == I2C_LOW)
		{
			WaitForQuarterClock();
		}
		WaitForHalfClock();
	}
	SetSDA(I2C_LOW);
	WaitForQuarterClock();
	SetSCL(I2C_LOW);
	WaitForQuarterClock();
}

void i2c_rep_start()
{
	return i2c_start();
}

unsigned char i2c_write(unsigned char data)
{
	unsigned char r;
	unsigned char i;
	if(GetSCL() != I2C_LOW)
	{
		SetSCL(I2C_LOW);
		WaitForQuarterClock();
	}
	for(i = 0; i < 8; i++)
	{
		SetSDA((data & (0x80 >> i)) ? I2C_HIGH : I2C_LOW);
		WaitForQuarterClock();
		SetSCL(I2C_HIGH);
		while(GetSCL() == I2C_LOW)
		{
			WaitForQuarterClock();
		}
		WaitForHalfClock();
		SetSCL(I2C_LOW);
		WaitForQuarterClock();
	}
	SetSDA(I2C_HIGH);
	WaitForQuarterClock();
	SetSCL(I2C_HIGH);
	while(GetSCL() == I2C_LOW)
	{
		WaitForQuarterClock();
	}
	WaitForHalfClock();
	r = (GetSDA() == I2C_LOW) ? 0 : 1;
	SetSCL(I2C_LOW);
	WaitForQuarterClock();
	return r;
}

unsigned char i2c_read(unsigned char ack)
{
	unsigned char r;
	unsigned char i;
	if(GetSCL() != I2C_LOW)
	{
		SetSCL(I2C_LOW);
		WaitForQuarterClock();
	}
	SetSDA(I2C_HIGH);
	r = 0;
	for(i = 0; i < 8; i++)
	{
		WaitForQuarterClock();
		SetSCL(I2C_HIGH);
		while(GetSCL() == I2C_LOW)
		{
			WaitForQuarterClock();
		}
		WaitForHalfClock();
		r |= (GetSDA() == I2C_HIGH) ? (0x80 >> i) : 0;
		SetSCL(I2C_LOW);
		WaitForQuarterClock();
	}
	SetSDA((ack == 0) ? I2C_HIGH : I2C_LOW);
	WaitForQuarterClock();
	SetSCL(I2C_HIGH);
	while(GetSCL() == I2C_LOW)
	{
		WaitForQuarterClock();
	}
	WaitForHalfClock();
	SetSCL(I2C_LOW);
	WaitForQuarterClock();
	return r;
}

int _tmain(int argc, _TCHAR* argv[])
{
	short s;
	printf("ADT7410 thermometer over Km2Net USB-IO2.0 / USB-IO2.0(AKI)\n");
	i2c_init();
	i2c_start();
	i2c_write((0x48 << 1) | 0x00);
	i2c_write(0x03);
	i2c_write(0x80);
	i2c_stop();
	while(1)
	{
		Sleep(500);
		i2c_start();
		i2c_write((0x48 << 1) | 0x00);
		i2c_write(0x00);
		i2c_rep_start();
		i2c_write((0x48 << 1) | 0x01);
		s = i2c_read(1) & 0xff;
		s = (s << 8) | (i2c_read(0) & 0xff);
		i2c_stop();
		printf("%.3f C\n", (float)s / 128);
	}
	return 0;
}

