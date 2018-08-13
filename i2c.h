/* I2C (TwoWire) AVR library
 *
 * Copyright (C) 2015-2017 Sergey Denisov.
 * Rewritten by Sergey Denisov aka LittleBuster (DenisovS21@gmail.com)
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public Licence
 * as published by the Free Software Foundation; either version 3
 * of the Licence, or (at your option) any later version.
 */

#ifndef __I2C_H__
#define __I2C_H__

#include <avr/io.h>
#include <twi.h>

/** defines the data direction (reading from I2C device) in i2c_start(),i2c_rep_start() */
#define I2C_READ    1

/** defines the data direction (writing to I2C device) in i2c_start(),i2c_rep_start() */
#define I2C_WRITE   0

#ifndef F_CPU
	#define F_CPU 16000000UL
#endif

/* I2C clock in Hz */
#define SCL_CLOCK  10000L
#define TWI_SPEED (uint8_t)(((F_CPU/SCL_CLOCK)-16)/2)

/*
 * Init lib
 */
void i2c_init(void);

/*
 * Start sending data
 */
void i2c_start(void);
uint8_t i2c_start_addr(unsigned char address);
void i2c_start_wait(unsigned char address);
uint8_t i2c_rep_start(unsigned char address);

/*
 * Stop sending data
 */
 void i2c_stop(void);
void i2c_stop_condition(void);

/**
 * Send byte to device
 * @byte: sending byte
 */
 uint8_t i2c_write( unsigned char data );
void i2c_send_byte(unsigned char byte);

/**
 * Send packet to device
 * @value: sending data
 * @address: device address
 */
void i2c_send_packet(unsigned char value, unsigned char address);

/**
 * Receive byte from device
 *
 * returns: byte
 */
uint8_t i2c_readAck(void);

/**
 * Receiving last byte from device
 *
 * returns: byte
 */
uint8_t i2c_readNak(void);


#endif
