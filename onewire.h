#ifndef ONEWIRE_H
#define ONEWIRE_H

#include <avr/io.h>

#define F_CPU 16000000UL

#define true 1
#define false 0

#define MAXDEVICES 1

#define USART_BAUDRATE_57600 (((F_CPU / (57600 * 16UL))) - 1)
#define USART_BAUDRATE_115200 (((F_CPU / (115200 * 16UL))) - 1)
#define USART_BAUDRATE_9600 (((F_CPU / (9600 * 16UL))) - 1)

#define OW_CMD_SEARCHROM	0xF0
#define OW_CMD_READROM		0x33
#define OW_CMD_MATCHROM		0x55
#define OW_CMD_SKIPROM		0xCC

#define	OW_SEARCH_FIRST	0xFF		// start new search
#define	OW_PRESENCE_ERR	0xFF
#define	OW_DATA_ERR	    0xFE
#define OW_LAST_DEVICE	0x00		// last device found
//			0x01 ... 0x40: continue searching

#define OW_DS1990_FAMILY_CODE	1
#define OW_DS2405_FAMILY_CODE	5
#define OW_DS2413_FAMILY_CODE	0x3A
#define OW_DS1822_FAMILY_CODE	0x22
#define OW_DS2430_FAMILY_CODE	0x14
#define OW_DS1990_FAMILY_CODE	1
#define OW_DS2431_FAMILY_CODE	0x2d
#define OW_DS18S20_FAMILY_CODE	0x10
#define OW_DS18B20_FAMILY_CODE	0x28
#define OW_DS2433_FAMILY_CODE	0x23

// rom-code size including CRC
#define OW_ROMCODE_SIZE	8

extern void run_tasks(void);

uint8_t OW_Reset(void);
void OW_WriteBit(uint8_t bit);
uint8_t OW_ReadBit(void);
uint8_t OW_ReadByte(void);
uint8_t OW_WriteByte(uint8_t byte);
uint8_t OW_SearchROM(uint8_t diff, uint8_t *id );
void OW_FindROM(uint8_t *diff, uint8_t id[]);
uint8_t OW_ReadROM(uint8_t *buffer);
uint8_t OW_MatchROM(uint8_t *rom);

#endif
