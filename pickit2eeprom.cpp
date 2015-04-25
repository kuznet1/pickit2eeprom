/*
 * Pickit2eeprom - a tiny tool for program big EEPROMs of 25C series by PICkit2.
 * (25C often uses as BIOS flash)
 *
 * Copyright (C) 2015 Alexey Kuznetsov
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111 USA
 * or see <http://www.gnu.org/licenses/>
 */

#undef UNICODE
#pragma comment (lib, "hid.lib")
#pragma comment (lib, "setupapi.lib")
#include <windows.h>
#include "setupapi.h"
extern "C" 
{
	#include "hidsdi.h"
}
#include <stdio.h>
#include <iostream>
using namespace std;
const char idstr[]="vid_04d8&pid_0033"; //our device

#define SETVDD			0xA0
#define EXECUTE_SCRIPT	0xA6
#define VDD_ON			0xFF
#define BUSY_LED_ON		0xF5
#define VPP_OFF			0xFA
#define MCLR_GND_ON		0xF7
#define SET_ICSP_PINS	0xF3
#define SET_AUX			0xCF
#define SPI_WR_BYTE_LIT	0xC7
#define SPI_RD_BYTE_BUF	0xC5
#define MCLR_GND_OFF	0xF6
#define VPP_ON			0xFB
#define BUSY_LED_OFF	0xF4
#define VDD_OFF			0xFE
#define SET_ICSP_SPEED	0xEA
#define UPLOAD_DATA_NOLEN 0xAC
#define LOOP			0xE9
#define DELAY_LONG		0xE8
#define SPI_WR_BYTE_BUF	0xC6
#define IF_EQ_GOTO		0xE6
#define UPLOAD_DATA		0xAA
#define DOWNLOAD_DATA	0xA8
#define END_OF_BUFFER	0xAD

void PrintError()
{ 
	char* lpMsgBuf;
	FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER|FORMAT_MESSAGE_FROM_SYSTEM|FORMAT_MESSAGE_IGNORE_INSERTS,0,GetLastError(),MAKELANGID(LANG_NEUTRAL,SUBLANG_DEFAULT),(char*)&lpMsgBuf,0,0);
	printf(lpMsgBuf);
}
void PrintHelp()
{ 
	printf("  USAGE:\n\n Read:	-r <file_name> <size>[K|M]\n Prog:	-p <file_name>\n Erase:	-e\n\n\n  CONNECTING:\n\n PICkit2 Pin   25C Device Pin (DIP/SO8)\n");
	printf(" (1) VPP ----> 1 nCS\n (2) Vdd ----> 8 Vcc, 3 nWP, 7 nHOLD\n (3) GND ----> 4 Vss\n (4) PGD ----> 2 SO\n (5) PGC ----> 6 SCK\n (6) AUX ----> 5 SI\n\n");
}

int GetSize(char* str)
{
	int size;
	char multipl;
	switch(sscanf(str,"%d%c",&size,&multipl))
	{
	case 1:
		return size;
	case 2:
		switch(multipl)
		{
		case 'M':
			return size*1048576;
		case 'K':
			return size*1024;
		}
	}
	return 0;
}
HANDLE GetDeviceHandle()
{
	GUID HidGuid;
	HidD_GetHidGuid(&HidGuid);
	HDEVINFO PnpHandle=SetupDiGetClassDevs(&HidGuid,0,0,DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
	if(PnpHandle==INVALID_HANDLE_VALUE) return INVALID_HANDLE_VALUE;
	SP_DEVICE_INTERFACE_DATA DevInterData;
	DevInterData.cbSize=sizeof(DevInterData);
	SP_DEVINFO_DATA DevData;
	DevData.cbSize=sizeof(DevData);
	DWORD BytesRequired;
	SP_DEVICE_INTERFACE_DETAIL_DATA* FuncClassDevData=0;
	//enum of HID devs
	for(int i=0;SetupDiEnumDeviceInterfaces(PnpHandle,0,&HidGuid,i,&DevInterData);i++)
	{
		//get BytesRequired
		SetupDiGetDeviceInterfaceDetail(PnpHandle,&DevInterData,0,0,&BytesRequired,&DevData);
		//alloc mem
		FuncClassDevData=(SP_DEVICE_INTERFACE_DETAIL_DATA*)HeapAlloc(GetProcessHeap(),HEAP_NO_SERIALIZE,BytesRequired);
		FuncClassDevData->cbSize=sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA);
		//get the PNP name
		SetupDiGetDeviceInterfaceDetail(PnpHandle,&DevInterData,FuncClassDevData,BytesRequired,&BytesRequired,&DevData);
		if(strstr(FuncClassDevData->DevicePath,idstr)) break; //found device!!!
		HeapFree(GetProcessHeap(),HEAP_NO_SERIALIZE,FuncClassDevData);
	}
	SetupDiDestroyDeviceInfoList(PnpHandle);
	if(FuncClassDevData==0||!strstr(FuncClassDevData->DevicePath,idstr)) return INVALID_HANDLE_VALUE; //if not found...
	HANDLE DevHandle=CreateFile(FuncClassDevData->DevicePath,GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,0,OPEN_EXISTING,0,0); //open device handle
	HeapFree(GetProcessHeap(),HEAP_NO_SERIALIZE,FuncClassDevData);
	return DevHandle;
}

BYTE GetInfoScript[]={
	0,SETVDD,0,0x1D,0x76,EXECUTE_SCRIPT,17,
	VDD_ON,BUSY_LED_ON,VPP_OFF,MCLR_GND_ON,SET_ICSP_PINS,2,SET_AUX,0,
	SPI_WR_BYTE_LIT,0x9F,SPI_RD_BYTE_BUF,SPI_RD_BYTE_BUF,SPI_RD_BYTE_BUF,
	MCLR_GND_OFF,VPP_ON,BUSY_LED_OFF,VDD_OFF,
	UPLOAD_DATA_NOLEN,END_OF_BUFFER
};
bool GetInfo(HANDLE dev)
{
	DWORD cWritten;
	BYTE buf[65];
	if(dev==INVALID_HANDLE_VALUE)
	{
		printf("Error: couldn't connect with PICkit2.\n");
		return false;
	}
	memcpy(buf,GetInfoScript,sizeof(GetInfoScript));
	WriteFile(dev,buf,65,&cWritten,0);
	ReadFile(dev,buf,65,&cWritten,0);
	printf("Vendor: %X Device: %X Capacity: %X\n",buf[1],buf[2],buf[3]);
	return true;
}

BYTE PrepareToRead[]={
	0,SETVDD,0,0x1D,0x76,EXECUTE_SCRIPT,18,
	VDD_ON,BUSY_LED_ON,VPP_OFF,MCLR_GND_ON,SET_ICSP_PINS,2,SET_AUX,0,
	SET_ICSP_SPEED,1,
	SPI_WR_BYTE_LIT,0x03,
	SPI_WR_BYTE_LIT,0,
	SPI_WR_BYTE_LIT,0,
	SPI_WR_BYTE_LIT,0,
	END_OF_BUFFER
};

BYTE Read64andSend[]={
	0,EXECUTE_SCRIPT,4,
	SPI_RD_BYTE_BUF,
	LOOP,1,63,
	UPLOAD_DATA_NOLEN,
	END_OF_BUFFER
};

BYTE FinishRead[]={
	0,EXECUTE_SCRIPT,4,
	MCLR_GND_OFF,VPP_ON,BUSY_LED_OFF,VDD_OFF,
	END_OF_BUFFER
};
BYTE PrepareToProgram[]={
	0,SETVDD,0,0x1D,0x76,EXECUTE_SCRIPT,8,
	VDD_ON,BUSY_LED_ON,SET_ICSP_PINS,2,SET_AUX,0,
	DELAY_LONG,2,
	END_OF_BUFFER
};
BYTE Send32andProgram[]={
	EXECUTE_SCRIPT,22,
	VPP_OFF,MCLR_GND_ON,
	SPI_WR_BYTE_LIT,0x06,
	MCLR_GND_OFF,VPP_ON,VPP_OFF,MCLR_GND_ON,
	SPI_WR_BYTE_LIT,0x02,
	SPI_WR_BYTE_LIT,0, //address
	SPI_WR_BYTE_LIT,0, //to write
	SPI_WR_BYTE_LIT,0, //is here...
	SPI_WR_BYTE_BUF,
	LOOP,1,31,
	MCLR_GND_OFF,VPP_ON,
	END_OF_BUFFER
};

BYTE FinishProgram[]={
	0,EXECUTE_SCRIPT,4,
	MCLR_GND_OFF,VPP_ON,BUSY_LED_OFF,VDD_OFF,
	END_OF_BUFFER
};

BYTE Erase[]={
	0,SETVDD,0,0x1D,0x76,EXECUTE_SCRIPT,34,
	VDD_ON,BUSY_LED_ON,VPP_OFF,MCLR_GND_ON,SET_ICSP_PINS,2,SET_AUX,0,
	DELAY_LONG,2,
	SPI_WR_BYTE_LIT,0x06,
	MCLR_GND_OFF,VPP_ON,VPP_OFF,MCLR_GND_ON,
	SPI_WR_BYTE_LIT,0xC7,
	MCLR_GND_OFF,VPP_ON,VPP_OFF,MCLR_GND_ON,
	SPI_WR_BYTE_LIT,0x05,
	SPI_RD_BYTE_BUF,
	DELAY_LONG,30,
	IF_EQ_GOTO,3,-3,
	MCLR_GND_OFF,VPP_ON,BUSY_LED_OFF,VDD_OFF,
	UPLOAD_DATA,END_OF_BUFFER
};

int main(int argc, char* argv[])
{
	BYTE outbuf[65];
	BYTE inbuf[65];
	DWORD cWritten,size;
	HANDLE hFile,dev=GetDeviceHandle();
	setlocale(0,"");

	switch(argc)
	{
	case 4:
		size=GetSize(argv[3]);
		if(strcmp("-r",argv[1])||!size)
		{
			PrintHelp();
			return 2;
		}
		if(!GetInfo(dev)) return 3;
		hFile=CreateFile(argv[2],GENERIC_WRITE,FILE_SHARE_READ,0,CREATE_ALWAYS,0,0);
		if(!hFile)
		{
			PrintError();
			return 1;
		}
		memcpy(&outbuf,&PrepareToRead,sizeof(PrepareToRead));
		WriteFile(dev,&outbuf,65,&cWritten,0);
		printf("Reading: 00%%...");
		memcpy(&outbuf,&Read64andSend,sizeof(Read64andSend));
		for(int i=0;i<size;i+=64)		{
			printf("\b\b\b\b\b\b%0*d%%...",2,i*100/size);
			WriteFile(dev,&outbuf,65,&cWritten,0);
			ReadFile(dev,&inbuf,65,&cWritten,0);
			WriteFile(hFile,&(inbuf[1]),64,&cWritten,0);
		}
		printf("done.\n");
		memcpy(&outbuf,&FinishRead,sizeof(FinishRead));
		WriteFile(dev,&outbuf,65,&cWritten,0);
		CloseHandle(hFile);
		return 0;
	case 3:
		if(strcmp("-p",argv[1]))
		{
			PrintHelp();
			return 2;
		}
		if(!GetInfo(dev)) return 3;
		hFile=CreateFile(argv[2],GENERIC_READ,FILE_SHARE_READ,0,OPEN_EXISTING,0,0);
		if(!hFile)
		{
			PrintError();
			return 1;
		}
		size=GetFileSize(hFile,0);
		memcpy(&outbuf,&PrepareToProgram,sizeof(PrepareToProgram));
		WriteFile(dev,&outbuf,65,&cWritten,0);
		printf("Programming: 00%%...");
		outbuf[0]=0;
		outbuf[1]=DOWNLOAD_DATA;
		outbuf[2]=32;
		memcpy(&outbuf[35],&Send32andProgram,sizeof(Send32andProgram));
		for(int i=0;i<size;i+=32)
		{
			outbuf[48]=(i>>16)&0xFF; //sets
			outbuf[50]=(i>>8)&0xFF; //address
			outbuf[52]=i&0xFF;	   //to write
			printf("\b\b\b\b\b\b%0*d%%...",2,i*100/size);
			ReadFile(hFile,&(outbuf[3]),32,&cWritten,0);
			WriteFile(dev,&outbuf,65,&cWritten,0);

		}
		printf("done.\n");
		memcpy(&outbuf,&FinishProgram,sizeof(FinishProgram));
		WriteFile(dev,&outbuf,65,&cWritten,0);
		CloseHandle(hFile);
		return 0;
	case 2:
		if(strcmp("-e",argv[1]))
		{
			PrintHelp();
			return 2;
		}
		if(!GetInfo(dev)) return 3;
		memcpy(&outbuf,&Erase,sizeof(Erase));
		WriteFile(dev,&outbuf,65,&cWritten,0);
		printf("Erasing...");
		ReadFile(dev,&inbuf,65,&cWritten,0);
		printf("done.\n");
		return 0;
	}
	PrintHelp();
	return 2;
}
