#include "onewire.h"
#include <avr/interrupt.h>

#define sbi(reg,bit) reg |= (1<<bit)
#define cbi(reg,bit) reg &= ~(1<<bit)
#define ibi(reg,bit) reg ^= (1<<bit)
#define CheckBit(reg,bit) (reg&(1<<bit))

uint8_t OW_Reset(void)
{
	UCSRB=(1<<RXEN)|(1<<TXEN);
	//9600
	UBRRL = USART_BAUDRATE_9600;
	UBRRH = (USART_BAUDRATE_9600 >> 8);
	//UCSRA &= ~(1<<U2X);
	
	while(CheckBit(UCSRA, RXC)) UDR; //Зачистка буферов
	cli();
	UDR = 0xF0;
	UCSRA = (1<<TXC);
	sei();
	while(!CheckBit(UCSRA, RXC)) /*run_tasks()*/;
	if (UDR != 0xF0) return 1;
	return 0;
}

void OW_WriteBit(uint8_t bit)
{
	UBRRL = USART_BAUDRATE_115200;	//115200
	UBRRH = (USART_BAUDRATE_115200 >> 8);
	//UCSRA |= (1<<U2X);	
	
	uint8_t d = 0x00;	
	while(CheckBit(UCSRA, RXC)) UDR; // clean buffers
	if (bit) d = 0xFF;
	cli();
	UDR = d;
	UCSRA=(1<<TXC);
	sei();
	while(!CheckBit(UCSRA,TXC));
	while(CheckBit(UCSRA, RXC)) UDR; // clean buffers	
}

uint8_t OW_ReadBit(void)
{	
	//115200
	UBRRL = USART_BAUDRATE_115200;
	UBRRH = (USART_BAUDRATE_115200 >> 8);
	//UCSRA |= (1<<U2X);
	
	uint8_t c;
	while(CheckBit(UCSRA, RXC)) UDR; // clean buffers
	cli();		
	UDR = 0xFF;
	UCSRA=(1<<TXC);
	sei();
	while(!CheckBit(UCSRA, TXC));
	while(!CheckBit(UCSRA, RXC));
	c = UDR;
	if (c>0xFE) return 1;
	return 0;
}

uint8_t OW_WriteByte(uint8_t byte)
{
	uint8_t i = 8;
	UBRRL = USART_BAUDRATE_115200;			//115200
	UBRRH = (USART_BAUDRATE_115200 >> 8);
	//UCSRA |= (1<<U2X);
	do
	{
		uint8_t d = 0x00;
		if (byte&1) d = 0xFF;
		cli();
		UDR = d;
		UCSRA=(1<<TXC);
		sei();
		while(!CheckBit(UCSRA,RXC)) /*run_tasks()*/;
		byte>>=1;
		if (UDR>0xFE) byte|=128;
	}
	while(--i);
	return byte&255;
}

uint8_t OW_ReadByte(void)
{
	uint8_t n = 0;
	for(uint8_t i=0; i<8; i++) if (OW_ReadBit()) sbi(n, i);
	return n;
}

uint8_t OW_SearchROM(uint8_t diff, uint8_t *id )
{ 	
	uint8_t i, j, next_diff;
	uint8_t b;

	if(!OW_Reset()) 
		return OW_PRESENCE_ERR;			// error, no device found

	OW_WriteByte(OW_CMD_SEARCHROM);     // ROM search command
	next_diff = OW_LAST_DEVICE;			// unchanged on last device
	
	i = OW_ROMCODE_SIZE * 8;			// 8 bytes
	do 
	{	
		j = 8;							// 8 bits
		do 
		{ 
			b = OW_ReadBit();			// read bit
			if( OW_ReadBit() ) 
			{							// read complement bit
				if( b )                 // 11
				return OW_DATA_ERR;		// data error
			}
			else 
			{ 
				if( !b ) {				// 00 = 2 devices
				if( diff > i || ((*id & 1) && diff != i) ) { 
						b = 1;               // now 1
						next_diff = i;       // next pass 0
					}
				}
			}
         OW_WriteBit( b );              // write bit
         *id >>= 1;
         if( b ) *id |= 0x80;			// store bit
         i--;
		} 
		while( --j );
		id++;                           // next byte
    } 
	while( i );
	return next_diff;					// to continue search
}

void OW_FindROM(uint8_t *diff, uint8_t id[])
{
	while(1)
    {
		*diff = OW_SearchROM( *diff, &id[0] );
    	if ( *diff==OW_PRESENCE_ERR || *diff==OW_DATA_ERR ||
    		*diff == OW_LAST_DEVICE ) return;
    	//if ( id[0] == DS18B20_ID || id[0] == DS18S20_ID ) 
		return;
    }
}

uint8_t OW_ReadROM(uint8_t *buffer)
{
	if (!OW_Reset()) return 0;
	OW_WriteByte(OW_CMD_READROM);
	for (uint8_t i = 0; i < 8; i++)
	{
		buffer[i] = OW_ReadByte();
	}
	return 1;
}

uint8_t OW_MatchROM(uint8_t *rom)
{
 	if (!OW_Reset()) return 0;
	OW_WriteByte(OW_CMD_MATCHROM);	
	for(uint8_t i = 0; i < 8; i++)
	{
		OW_WriteByte(rom[i]);
	}
	return 1;
}
