

#include <avr/io.h>
#include <util/delay.h>
#include "isp.h"
#include "clock.h"
#include "usbasp.h"

#define spiHWdisable() SPCR = 0

extern unsigned int prog_pagesize;
extern uchar prog_pagecounter;
uchar low_byte;

uchar sck_sw_delay;
uchar sck_spcr;
uchar sck_spsr;
uchar isp_hiaddr;

//Parallel
uchar dev_type;

void avr_reset(void)
{
	VPP_LOW
	_delay_ms(10);
	VPP_HIGH
}

void puls_xt1(void)
{
	XTAIL_HIGH
	_delay_us(5);
	XTAIL_LOW
	_delay_us(5);
}

//Отправка/приём последовательного пакета
uchar avr_serialExchange(uchar instr, uchar data)
{
	uchar i;
	unsigned int command, dat, request = 0;
	command = instr << 2;
	dat = data<<2;

	DATA_IN
	for(i = 0; i < 11; i++)
	{
		_delay_us(1);
		//Выставляем SII
		if((command >> (10 - i) & 0x01)) SII_HIGH
		else SII_LOW
		//Выставляем SDI
		if((dat >> (10 - i) & 0x01)) SDI_HIGH
		else SDI_LOW
		SCI_HIGH
		_delay_us(1);
		SCI_LOW
		//Защёлкиваем SDO
		if(DATA_PIN & 0x01) request |= (1 << (10 - i));
	}
	return (request>>3);
}

void avr_bsySerial(void)
{
	unsigned int delay = 0;
	_delay_us(50);
	while(!(DATA_PIN & 0x01))
	{
		_delay_us(10);
		delay++;
		if(delay == 0xFFF)
		{
			avr_reset();
			return;
		}
	}
}

uchar avr_progMode(void)
{
	uchar i;
	//Full bus device - dev_type = 0
	VPP_HIGH
	XTAIL_LOW
	XA0_HIGH
	XA1_HIGH
	_delay_ms(10);
	//Reset to low
	VPP_LOW
	_delay_ms(10);
	//Toggle XTAL1 at least 6 times
	for(i = 0; i < 10; i++) puls_xt1();
	//Set the Prog_enable pins to "0000"
	PAGEL_LOW
	XA0_LOW
	XA1_LOW
	BS1_LOW
	_delay_ms(20);
	//Apply 11.5 - 12.5V to RESET
	VPP_HIGH
	_delay_ms(50);
	dev_type = 0x00;
	if(avr_getId(0) == 0x1E) return 0;

	//Shorrt bus device - dev_type = 1
	VDD_LOW
	_delay_ms(200);
	XA0_LOW
	XA1_LOW
	BS1_LOW
	WR_LOW
	OE_LOW
	VPP_LOW
	_delay_ms(20);
	VDD_HIGH
	_delay_ms(10);
	VPP_HIGH
	_delay_ms(500);
	WR_HIGH
	OE_HIGH

	dev_type = 0x01;
	if(avr_getId(0) == 0x1E) return 0;

	//Serial HV Programming
	VDD_LOW
	SCI_LOW
	DATA_OUT
	SDI_LOW
	SII_LOW
	SDO_LOW
	VPP_LOW
	_delay_ms(10);
	VDD_HIGH
	VPP_HIGH
	_delay_ms(20);
	DATA_IN
	_delay_us(500);
	dev_type = 0x02;
	if(avr_getId(0) == 0x1E) return 0;

	return 1;
}

//Загрузка команды
void avr_loadComm(uchar command)
{
	if(dev_type == 0x00 || dev_type == 0x01)
	{
		DATA_OUT
		DATA_PORT = command;
		//Устанавливаем биты XA на загрузку комманды [1:0]
		XA1_HIGH
		XA0_LOW
		//Устанавливаем биты BS
		BS1_LOW
		if(dev_type == 0) BS2_LOW
		//Устанавливаем код комманды
		_delay_us(1);
		//Даём импульс на XT1
		puls_xt1();
	}
}

//Загрузка байта адреса
void avr_loadAdd(uchar add, uchar hi_lo)
{
	if(dev_type == 0x00 || dev_type == 0x01)
	{
		//Устанавливаем биты XA на загрузку комманды [0:0]
		XA1_LOW
		XA0_LOW
		//Установка BS0 (старший байт адреса - 1, младший - 0)
		if(hi_lo) BS1_HIGH
		else BS1_LOW
		if(dev_type == 0x00) BS2_LOW
		//Устанавливаем адрес
		DATA_OUT
		DATA_PORT = add;
		_delay_us(5);
		//Даём импульс на XT1
		puls_xt1();
	}
}

uchar avr_getId(uchar idadd)
{
	uchar result;

	if(dev_type == 0x00 || dev_type == 0x01)
	{
		avr_loadComm(0x08);
		avr_loadAdd(idadd, 0);
		//Read first byte
		DATA_IN
		OE_LOW
		_delay_ms(1);
		result = DATA_PIN;
		OE_HIGH
	}
	else
	{
		//Read first byte
		avr_serialExchange(0x4C, 0x08);
		avr_serialExchange(0x0C, idadd);
		avr_serialExchange(0x68, 0x00);
		result = avr_serialExchange(0x6C, 0x00);
	}

	return result;
}

uchar avr_getFuse(uchar bt) //bt=0 HFuse, bt=1 LFuse, bt=2 EFuse, bt=3 LBits
{
	uchar result = 0;

	if(bt > 3) bt = bt % 3;
	if(dev_type == 0x00 || dev_type == 0x01)
	{
		avr_loadComm(0x04);
		DATA_IN
		if(bt == 0)
		{
			BS1_HIGH
			if(dev_type == 0x00) BS2_HIGH
			else XA1_HIGH
		}
		if(bt == 1)
		{
			BS1_LOW
			if(dev_type == 0x00) BS2_LOW
			else XA1_LOW
		}
		if(bt == 2)
		{
			BS1_LOW
			if(dev_type == 0x00) BS2_HIGH
			XA1_HIGH
		}
		if(bt == 3)
		{
			BS1_HIGH
			if(dev_type == 0x00) BS2_LOW
			XA1_LOW
		}
		OE_LOW
		_delay_ms(1);
		result = DATA_PIN; //HIGH BITS
		OE_HIGH
	}
	else
	{
		avr_serialExchange(0x4C, 0x04);
		if(bt == 1)
		{
			avr_serialExchange(0x68, 0x00);
			result = avr_serialExchange(0x6C, 0x00);
		}
		if(bt == 0)
		{
			avr_serialExchange(0x7A, 0x00);
			result = avr_serialExchange(0x7E, 0x00);
		}
		if(bt == 2)
		{
			avr_serialExchange(0x6A, 0x00);
			result = avr_serialExchange(0x6E, 0x00);
		}
		if(bt == 3)
		{
			avr_serialExchange(0x78, 0x00);
			result = avr_serialExchange(0x7C, 0x00);
		}
	}

	return result;
}

void avrSetFuse(uchar fs, uchar vl)
{
	if(dev_type == 0x00 || dev_type == 0x01) //Full bus or short bus
	{
		PAGEL_LOW
		if(fs == 0xE0) avr_loadComm(0x20);
		else avr_loadComm(0x40);
		_delay_us(10);
		XA1_LOW
		XA0_HIGH
		DATA_PORT = vl;
		_delay_us(10);
		puls_xt1();
		if(fs == 0xA0)//Low fuse
		{
			BS1_LOW
			if(dev_type == 0x00) BS2_LOW
			else XA1_LOW
		}
		if(fs == 0xA8) //High fuse
		{
			BS1_HIGH
			if(dev_type == 0x00) BS2_LOW
			else XA1_LOW
		}
		if(fs == 0xA4) //Ext fuse
		{
			BS1_LOW
			if(dev_type == 0x00) BS2_HIGH
			else XA1_HIGH
		}
		_delay_us(10);
		WR_LOW
		_delay_ms(1);
		WR_HIGH
		_delay_ms(100);
	}
	else
	{
		if(fs == 0xA0)//Low fuse
		{
			avr_serialExchange(0x4C, 0x40);
			avr_serialExchange(0x2C, vl);
			avr_serialExchange(0x64, 0x00);
			avr_serialExchange(0x6C, 0x00);
			avr_bsySerial();
		}
		if(fs == 0xA8) //High fuse
		{
			avr_serialExchange(0x4C, 0x40);
			avr_serialExchange(0x2C, vl);
			avr_serialExchange(0x74, 0x00);
			avr_serialExchange(0x7C, 0x00);
			avr_bsySerial();
		}
		if(fs == 0xA4) //Ext fuse
		{
			avr_serialExchange(0x4C, 0x40);
			avr_serialExchange(0x2C, vl);
			avr_serialExchange(0x66, 0x00);
			avr_serialExchange(0x6E, 0x00);
			avr_bsySerial();
		}
		if(fs == 0xE0) //LOCK Fuse
		{
		avr_serialExchange(0x4C, 0x20);
		avr_serialExchange(0x2C, vl);
		avr_serialExchange(0x64, 0x00);
		avr_serialExchange(0x6C, 0x00);
		avr_bsySerial();
		}
	}
}

void avr_erase(void)
{
	if(dev_type == 0x00 || dev_type == 0x01)
	{
		avr_loadComm(0x80);
		WR_LOW
		_delay_us(200);
		WR_HIGH
		_delay_ms(150);
	}
	else
	{

	}
}

//
void spiHWenable() {
	//SPCR = sck_spcr;
	//SPSR = sck_spsr;
}

void ispSetSCKOption(uchar option)
{
	avr_serialExchange(0x4C, 0x80);
	avr_serialExchange(0x64, 0x00);
	avr_serialExchange(0x6C, 0x00);
	avr_bsySerial();
}

void ispDelay()
{
	uint8_t starttime = TIMERVALUE;
	while ((uchar) (TIMERVALUE - starttime) < sck_sw_delay) { }
}

void ispConnect()
{
	//Parallel init
	CONTROL_PORT = 0xFF;
	CONTROL_DDR = 0xFF;
	DATA_IN
	POWER_DDR |= (1 << VDD_PIN)|(1 << VPP_PIN);
	
	WR_HIGH
	OE_HIGH
	// Initial extended address value
	isp_hiaddr = 0;
}

void ispDisconnect()
{
	DATA_IN
	CONTROL_DDR = 0x00;
	VDD_LOW
	VPP_LOW
	POWER_DDR &= ~((1 << VDD_PIN)|(1 << VPP_PIN));
}

uchar ispTransmit_sw(uchar send_byte)
{
	return 0xFF;
}

uchar ispTransmit_hw(uchar send_byte)
{
	return 0xFF;
}

uchar ispEnterProgrammingMode()
{
	//Parallel
	return avr_progMode();
}

static void ispUpdateExtended(unsigned long address)
{
	uchar curr_hiaddr;

	curr_hiaddr = (address >> 17);

	/* check if extended address byte is changed */
	if(isp_hiaddr != curr_hiaddr)
	{
		isp_hiaddr = curr_hiaddr;
		/* Load Extended Address byte */
		DATA_OUT
		XA0_LOW
		XA1_LOW
		BS1_LOW
		BS2_HIGH
		DATA_PORT = isp_hiaddr;
		puls_xt1();
	}
}

uchar ispReadFlash(unsigned long address)
{
	uchar result = 0;

	if(dev_type == 0x00 || dev_type == 0x01)
	{
		avr_loadComm(0x02);
		ispUpdateExtended(address);
		avr_loadAdd((address >> 9), 1);
		avr_loadAdd(((address >> 1) & 0xFF), 0);
		DATA_IN
		if(address & 0x1) BS1_HIGH
		else BS1_LOW
		OE_LOW
		_delay_us(1);
		result = DATA_PIN;
		OE_HIGH
	}
	else
	{
		avr_serialExchange(0x4C, 0x02);
		avr_serialExchange(0x0C, (address >> 1) & 0xFF);
		avr_serialExchange(0x1C, (address >> 9));
		avr_serialExchange(0x68, 0x00);
		if(address & 0x1) //High byte
		{
			avr_serialExchange(0x78, 0x00);
			result = avr_serialExchange(0x7C, 0x00);
		}
		else
		{
			avr_serialExchange(0x68, 0x00);
			result = avr_serialExchange(0x6C, 0x00);
		}
	}

	return result;
}

uchar ispWriteFlash(unsigned long address, uchar data, uchar pollmode)
{
	if(dev_type == 0x00 || dev_type == 0x01)
	{
		if(prog_pagecounter == prog_pagesize) avr_loadComm(0x10); //Page Write command
		if(!(address & 0x1))
		{
			low_byte = data;
			return 0;
		}
		avr_loadAdd((address >> 1) & 0xFF, 0);
		XA0_HIGH
		XA1_LOW
		DATA_PORT = low_byte;
		puls_xt1();
		BS1_HIGH
		DATA_PORT = data;
		puls_xt1();
		PAGEL_HIGH
		_delay_us(1);
		PAGEL_LOW
		_delay_us(1);
		return 0;
	}
	else
	{
		if(prog_pagecounter == prog_pagesize) avr_serialExchange(0x4C, 0x10);//Load "Write Flash" commang
		if(!(address & 0x1))
		{
			low_byte = data;
			return 0;
		}
		avr_serialExchange(0x0C, ((address >> 1) & 0xFF)); //Load LOW address byte
		avr_serialExchange(0x2C, low_byte);		   //Load LOW data byte
		avr_serialExchange(0x3C, data);	   //Load HIGH data byte
		avr_serialExchange(0x7D, 0x00);
		avr_serialExchange(0x7C, 0x00);
		return 0;
	}
	return 1; // error
}

uchar ispFlushPage(unsigned long address, uchar pollvalue)
{
	if(dev_type == 0x00 || dev_type == 0x01)
	{
		avr_loadAdd((address >> 9), 1);
		ispUpdateExtended(address);
		WR_LOW
		_delay_us(1);
		WR_HIGH
		_delay_ms(8);
		XA1_HIGH
		XA0_LOW
		DATA_PORT = 0x00;
		puls_xt1();
		return 0;
	}
	else
	{
		avr_serialExchange(0x1C, (address >> 9));
		avr_serialExchange(0x64, 0x00);
		avr_serialExchange(0x6C, 0x00);
		//avr_bsySerial();
		_delay_ms(8);
		avr_serialExchange(0x4C, 0x00);
	}

	return 1; /* error */
}

uchar ispReadEEPROM(unsigned int address)
{
	uchar result = 0;

	if(dev_type == 0 || dev_type == 1)
	{
		avr_loadComm(0x03);
		avr_loadAdd((address & 0xFF), 1);
		avr_loadAdd((address & 0xFF),0);
		DATA_IN
		BS1_LOW
		OE_LOW
		_delay_us(1);
		result = DATA_PIN;
		OE_HIGH
	}
	else
	{
		avr_serialExchange(0x4C, 0x03);
		avr_serialExchange(0x0C, (address & 0xFF));
		avr_serialExchange(0x1C, ((address >> 8) & 0xFF));
		avr_serialExchange(0x68, 0x00);
		result = avr_serialExchange(0x6C, 0x00);
	}

	return result;
}

uchar ispWriteEEPROM(unsigned int address, uchar data)
{
	if(dev_type == 0 || dev_type == 1)
	{
		//NOP
	}
	else
	{
		avr_serialExchange(0x4C, 0x11);
		avr_serialExchange(0x0C, (address & 0xFF));
		avr_serialExchange(0x1C, (address >> 8));
		avr_serialExchange(0x2C, data);
		avr_serialExchange(0x6D, 0x00);
		avr_serialExchange(0x64, 0x00);
		avr_serialExchange(0x6C, 0x00);
		avr_bsySerial();
	}
	return 0;
}
