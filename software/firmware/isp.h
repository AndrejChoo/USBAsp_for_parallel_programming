
#ifndef __isp_h_included__
#define	__isp_h_included__

#ifndef uchar
#define	uchar	unsigned char
#endif

#define	ISP_OUT   PORTB
#define ISP_IN    PINB
#define ISP_DDR   DDRB
#define ISP_RST   PB4
#define ISP_MOSI  PB5
#define ISP_MISO  PB6
#define ISP_SCK   PB7


//Parallel programming defines
#define DATA_IN				DDRA = 0x00;
#define DATA_OUT			DDRA = 0xFF;
#define DATA_PORT			PORTA
#define DATA_PIN			PINA

#define CONTROL_DDR			DDRC
#define CONTROL_PORT		PORTC
#define XA0_HIGH			CONTROL_PORT |= (1 << 0);
#define XA0_LOW				CONTROL_PORT &= ~(1 << 0);
#define PAGEL_HIGH			CONTROL_PORT |= (1 << 1);
#define PAGEL_LOW			CONTROL_PORT &= ~(1 << 1);
#define XTAIL_HIGH			CONTROL_PORT |= (1 << 2);
#define XTAIL_LOW			CONTROL_PORT &= ~(1 << 2);
#define XA1_HIGH			CONTROL_PORT |= (1 << 3);
#define XA1_LOW				CONTROL_PORT &= ~(1 << 3);
#define BS1_HIGH			CONTROL_PORT |= (1 << 4);
#define BS1_LOW				CONTROL_PORT &= ~(1 << 4);
#define BS2_HIGH			CONTROL_PORT |= (1 << 5);
#define BS2_LOW				CONTROL_PORT &= ~(1 << 5);
#define OE_HIGH				CONTROL_PORT |= (1 << 6);
#define OE_LOW				CONTROL_PORT &= ~(1 << 6);
#define WR_HIGH				CONTROL_PORT |= (1 << 7);
#define WR_LOW				CONTROL_PORT &= ~(1 << 7);
//Serial HV Programming
#define SDI_HIGH			CONTROL_PORT |= (1 << 1);
#define SDI_LOW				CONTROL_PORT &= ~(1 << 1);
#define SII_HIGH			CONTROL_PORT |= (1 << 0);
#define SII_LOW				CONTROL_PORT &= ~(1 << 0);
#define SCI_HIGH			CONTROL_PORT |= (1 << 3);
#define SCI_LOW				CONTROL_PORT &= ~(1 << 3);
#define SDO_HIGH			DATA_PORT |= (1 << 0);
#define SDO_LOW				DATA_PORT &= ~(1 << 0);


#define POWER_DDR			DDRD
#define POWER_PORT			PORTD
#define VDD_PIN				6
#define VPP_PIN				7
#define VDD_HIGH			POWER_PORT |= (1 << VDD_PIN);
#define VDD_LOW				POWER_PORT &= ~(1 << VDD_PIN);
#define VPP_HIGH			POWER_PORT |= (1 << VPP_PIN);
#define VPP_LOW				POWER_PORT &= ~(1 << VPP_PIN);


void avr_reset(void);
void puls_xt1(void);
uchar avr_serialExchange(uchar instr, uchar data);
void avr_bsySerial(void);
uchar avr_progMode(void);
void avr_loadComm(uchar command);
void avr_loadAdd(uchar add, uchar hi_lo);
uchar avr_getId(uchar);
uchar avr_getFuse(uchar);
void avrSetFuse(uchar, uchar);
void avr_erase(void);


/* Prepare connection to target device */
void ispConnect();

/* Close connection to target device */
void ispDisconnect();

/* read an write a byte from isp using software (slow) */
uchar ispTransmit_sw(uchar send_byte);

/* read an write a byte from isp using hardware (fast) */
uchar ispTransmit_hw(uchar send_byte);

/* enter programming mode */
uchar ispEnterProgrammingMode();

/* read byte from eeprom at given address */
uchar ispReadEEPROM(unsigned int address);

/* write byte to flash at given address */
uchar ispWriteFlash(unsigned long address, uchar data, uchar pollmode);

uchar ispFlushPage(unsigned long address, uchar pollvalue);

/* read byte from flash at given address */
uchar ispReadFlash(unsigned long address);

/* write byte to eeprom at given address */
uchar ispWriteEEPROM(unsigned int address, uchar data);

/* pointer to sw or hw transmit function */
uchar (*ispTransmit)(uchar);

/* set SCK speed. call before ispConnect! */
void ispSetSCKOption(uchar sckoption);

/* load extended address byte */
void ispLoadExtendedAddressByte(unsigned long address);

#endif /* __isp_h_included__ */
