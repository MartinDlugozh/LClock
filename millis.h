#ifndef MILLIS_H_
#define MILLIS_H_
#pragma once

#ifndef F_CPU
	#define F_CPU 16000000UL
#endif

// Calculate the value needed for
// the CTC match value in OCR0.
#define SYS_CTC_MATCH_OVERFLOW ((F_CPU / 1000UL) / 64)-1 //	CTC_MATCH_OVERFLOW = ((F_CPU / DESIRED_FREQUENCY) / PRESCALLER)-1

void millis_init(void);
uint32_t millis(void);
void millis_reset(void);

#endif /* MILLIS_H_ */