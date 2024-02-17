/*
 * USBasp - USB in-circuit programmer for Atmel AVR controllers
 *
 * Thomas Fischl <tfischl@gmx.de>
 * 2020 fixes and tweaks by Ralph Doncaster (Nerd Ralph)
 * 2021 WCID support by Dimitrios Chr. Ioannidis ( d.ioannidis@nephelae.eu )
 *      ( based on Marius Greuel's https://github.com/mariusgreuel/USBasp )
 * 2022 Composite WCID and HID by Dimitrios Chr. Ioannidis ( d.ioannidis@nephelae.eu )
 * 2023 Serial Number write via HID and
 *      descriptors stored in EEPROM by Dimitrios Chr. Ioannidis ( d.ioannidis@nephelae.eu )
 *
 * Add parallel programming support by Andrej Choo <andrejchukov@yandex.ru>
 *
 * Target.........: ATMega16 at 16 MHz
 * Creation Date..: 2024-02-16
 *
 */

#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <avr/wdt.h>
#include <util/delay.h>

#include "usbasp.h"
#include "usbdrv.h"
#include "isp.h"
#include "clock.h"
#include "tpi.h"
#include "tpi_defs.h"

static uchar replyBuffer[8];

static uchar prog_state = PROG_STATE_IDLE;
static uchar prog_sck = USBASP_ISP_SCK_AUTO;

static uchar prog_address_newmode = 0;
static unsigned long prog_address;
static unsigned int prog_nbytes = 0;
unsigned int prog_pagesize;
static uchar prog_blockflags;
uchar prog_pagecounter;

uchar usbFunctionSetup(uchar data[8]) {

	uchar len = 0;

	if (data[1] == USBASP_FUNC_CONNECT) {

		/* set SCK speed */
		if ((SLOW_SCK_PIN & (1 << SLOW_SCK_NUM)) == 0) {
			ispSetSCKOption(USBASP_ISP_SCK_8);
		} else {
			ispSetSCKOption(prog_sck);
		}

		/* set compatibility mode of address delivering */
		prog_address_newmode = 0;

		ledRedOn();
		ispConnect();

	} else if (data[1] == USBASP_FUNC_DISCONNECT) {
		ispDisconnect();
		ledRedOff();

	} else if (data[1] == USBASP_FUNC_TRANSMIT) {
		switch(data[2])
		{
			case 0x30: //Get ID
			{
				replyBuffer[0] = 0x00;
				replyBuffer[1] = 0x30;
				replyBuffer[2] = 0x00;
				replyBuffer[3] = avr_getId(data[4] & 0x3);
				break;
			}
			case 0x50: //Read Low/Ext Fuse
			{
				if(data[3] == 0x08)//Ext fuse
				{
					replyBuffer[0] = 0x00;
					replyBuffer[1] = 0x50;
					replyBuffer[2] = 0x00;
					replyBuffer[3] = avr_getFuse(0x02);
					break;
				}
				else //Low fuse
				{
					replyBuffer[0] = 0x00;
					replyBuffer[1] = 0x50;
					replyBuffer[2] = 0x00;
					replyBuffer[3] = avr_getFuse(0x01);
					break;
				}
			}
			case 0x58: //Read High Fuse/Lockbits
			{
				if(data[3] == 0x08)//High fuse
				{
					replyBuffer[0] = 0x00;
					replyBuffer[1] = 0x58;
					replyBuffer[2] = 0x00;
					replyBuffer[3] = avr_getFuse(0x00);
					break;
				}
				else //Lock bits
				{
					replyBuffer[0] = 0x00;
					replyBuffer[1] = 0x58;
					replyBuffer[2] = 0x00;
					replyBuffer[3] = avr_getFuse(0x03);
					break;
				}
			}
			case 0xAC: //Write All fuse/Lock bits/Erase/Prog Enable
			{
				replyBuffer[0] = 0x00;
				replyBuffer[1] = 0xAC;
				replyBuffer[2] = data[3];
				replyBuffer[3] = data[4];
				if(data[3] == 0x53) break; //Prog Enable
				else if(data[3] == 0x80) avr_erase(); //Erase
				else avrSetFuse(data[3], data[5]);
				break;
			}
			default:
			{
				replyBuffer[0] = 0;
				replyBuffer[1] = 0;
				replyBuffer[2] = 0;
				replyBuffer[3] = 0;
			}
		}




		len = 4;

	} else if (data[1] == USBASP_FUNC_READFLASH) {

		if (!prog_address_newmode)
			prog_address = (data[3] << 8) | data[2];

		prog_nbytes = (data[7] << 8) | data[6];
		prog_state = PROG_STATE_READFLASH;
		len = 0xff; /* multiple in */

	} else if (data[1] == USBASP_FUNC_READEEPROM) {

		if (!prog_address_newmode)
			prog_address = (data[3] << 8) | data[2];

		prog_nbytes = (data[7] << 8) | data[6];
		prog_state = PROG_STATE_READEEPROM;
		len = 0xff; /* multiple in */

	} else if (data[1] == USBASP_FUNC_ENABLEPROG) {
		replyBuffer[0] = ispEnterProgrammingMode();
		len = 1;

	} else if (data[1] == USBASP_FUNC_WRITEFLASH) {

		if (!prog_address_newmode)
			prog_address = (data[3] << 8) | data[2];

		prog_pagesize = data[4];
		prog_blockflags = data[5] & 0x0F;
		prog_pagesize += (((unsigned int) data[5] & 0xF0) << 4);
		if (prog_blockflags & PROG_BLOCKFLAG_FIRST) {
			prog_pagecounter = prog_pagesize;
		}
		prog_nbytes = (data[7] << 8) | data[6];
		prog_state = PROG_STATE_WRITEFLASH;
		len = 0xff; /* multiple out */

	} else if (data[1] == USBASP_FUNC_WRITEEEPROM) {

		if (!prog_address_newmode)
			prog_address = (data[3] << 8) | data[2];

		prog_pagesize = 0;
		prog_blockflags = 0;
		prog_nbytes = (data[7] << 8) | data[6];
		prog_state = PROG_STATE_WRITEEEPROM;
		len = 0xff; /* multiple out */

	} else if (data[1] == USBASP_FUNC_SETLONGADDRESS) {

		/* set new mode of address delivering (ignore address delivered in commands) */
		prog_address_newmode = 1;
		/* set new address */
		prog_address = *((unsigned long*) &data[2]);

	} else if (data[1] == USBASP_FUNC_SETISPSCK) {

		/* set sck option */
		prog_sck = data[2];
		replyBuffer[0] = 0;
		len = 1;

	} else if (data[1] == USBASP_FUNC_TPI_CONNECT) {
		tpi_dly_cnt = data[2] | (data[3] << 8);

		/* RST high */
		ISP_OUT |= (1 << ISP_RST);
		ISP_DDR |= (1 << ISP_RST);

		clockWait(3);

		/* RST low */
		ISP_OUT &= ~(1 << ISP_RST);
		ledRedOn();

		clockWait(16);
		tpi_init();

	} else if (data[1] == USBASP_FUNC_TPI_DISCONNECT) {

		tpi_send_byte(TPI_OP_SSTCS(TPISR));
		tpi_send_byte(0);

		clockWait(10);

		/* pulse RST */
		ISP_OUT |= (1 << ISP_RST);
		clockWait(5);
		ISP_OUT &= ~(1 << ISP_RST);
		clockWait(5);

		/* set all ISP pins inputs */
		ISP_DDR &= ~((1 << ISP_RST) | (1 << ISP_SCK) | (1 << ISP_MOSI));
		/* switch pullups off */
		ISP_OUT &= ~((1 << ISP_RST) | (1 << ISP_SCK) | (1 << ISP_MOSI));

		ledRedOff();

	} else if (data[1] == USBASP_FUNC_TPI_RAWREAD) {
		replyBuffer[0] = tpi_recv_byte();
		len = 1;

	} else if (data[1] == USBASP_FUNC_TPI_RAWWRITE) {
		tpi_send_byte(data[2]);

	} else if (data[1] == USBASP_FUNC_TPI_READBLOCK) {
		prog_address = (data[3] << 8) | data[2];
		prog_nbytes = (data[7] << 8) | data[6];
		prog_state = PROG_STATE_TPI_READ;
		len = 0xff; /* multiple in */

	} else if (data[1] == USBASP_FUNC_TPI_WRITEBLOCK) {
		prog_address = (data[3] << 8) | data[2];
		prog_nbytes = (data[7] << 8) | data[6];
		prog_state = PROG_STATE_TPI_WRITE;
		len = 0xff; /* multiple out */

	} else if (data[1] == USBASP_FUNC_GETCAPABILITIES) {
		replyBuffer[0] = USBASP_CAP_0_TPI;
		replyBuffer[1] = 0;
		replyBuffer[2] = 0;
		replyBuffer[3] = 0;
		len = 4;
	}

	usbMsgPtr = replyBuffer;

	return len;
}

uchar usbFunctionRead(uchar *data, uchar len) {

	uchar i;

	/* check if programmer is in correct read state */
	if ((prog_state != PROG_STATE_READFLASH) && (prog_state
			!= PROG_STATE_READEEPROM) && (prog_state != PROG_STATE_TPI_READ)) {
		return 0xff;
	}

	/* fill packet TPI mode */
	if(prog_state == PROG_STATE_TPI_READ)
	{
		tpi_read_block(prog_address, data, len);
		prog_address += len;
		return len;
	}

	/* fill packet ISP mode */
	for (i = 0; i < len; i++) {
		if (prog_state == PROG_STATE_READFLASH) {
			data[i] = ispReadFlash(prog_address);
		} else {
			data[i] = ispReadEEPROM(prog_address);
		}
		prog_address++;
	}

	/* last packet? */
	if (len < 8) {
		prog_state = PROG_STATE_IDLE;
	}

	return len;
}

uchar usbFunctionWrite(uchar *data, uchar len) {

	uchar retVal = 0;
	uchar i;

	/* check if programmer is in correct write state */
	if ((prog_state != PROG_STATE_WRITEFLASH) && (prog_state
			!= PROG_STATE_WRITEEEPROM) && (prog_state != PROG_STATE_TPI_WRITE)) {
		return 0xff;
	}

	if (prog_state == PROG_STATE_TPI_WRITE)
	{
		tpi_write_block(prog_address, data, len);
		prog_address += len;
		prog_nbytes -= len;
		if(prog_nbytes <= 0)
		{
			prog_state = PROG_STATE_IDLE;
			return 1;
		}
		return 0;
	}

	for (i = 0; i < len; i++) {

		if (prog_state == PROG_STATE_WRITEFLASH) {
			/* Flash */

			if (prog_pagesize == 0) {
				/* not paged */
				ispWriteFlash(prog_address, data[i], 1);
			} else {
				/* paged */
				ispWriteFlash(prog_address, data[i], 0);
				prog_pagecounter--;
				if (prog_pagecounter == 0) {
					ispFlushPage(prog_address, data[i]);
					prog_pagecounter = prog_pagesize;
				}
			}

		} else {
			/* EEPROM */
			ispWriteEEPROM(prog_address, data[i]);
		}

		prog_nbytes--;

		if (prog_nbytes == 0) {
			prog_state = PROG_STATE_IDLE;
			if ((prog_blockflags & PROG_BLOCKFLAG_LAST) && (prog_pagecounter
					!= prog_pagesize)) {

				/* last block and page flush pending, so flush it now */
				ispFlushPage(prog_address, data[i]);
			}

			retVal = 1; // Need to return 1 when no more data is to be received
		}

		prog_address++;
	}

	return retVal;
}

void hardwareInit(void) {

	uchar i;

	PORTB &= ~((1 << 0) | (1 << 1)); /* LEDs off */

	usbDeviceDisconnect();  /* enforce re-enumeration, do this while interrupts are disabled! */
	i = 0;
	while(--i){             /* fake USB disconnect for > 250 ms */
		wdt_reset();
		_delay_ms(1);
	}
	usbDeviceConnect();

	/* all inputs except PC0, PC1 */
	DDRB |= 0x03;

	/* enable pull up on jumper */
	SLOW_SCK_PORT |= (1 << SLOW_SCK_NUM);
}

void usbHadReset() {
	ledGreenOff();
}

void usbAddressAssigned() {
	ledGreenOn();
}

int main(void) {
	usbInit();

	/* init ports */
	hardwareInit();

	/* init timer */
	clockInit();

	/* start interrupts for USB */
	sei();

	/* main loop */
	for (;;) {
		usbPoll();
	}

	return 0;
}

