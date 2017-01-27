/*
 * seg7digit.h
 *
 *  Created on: Sep 13, 2016
 *      Author: zzhang
 */

#ifndef SEG7DIGIT_H_
#define SEG7DIGIT_H_

// Update the 4-digit 7-segment display with digit numbers (not 7-segment display pattern)
// if s1 == 10 and 11 are for handling percent mode
void seg7DigitUpdate(uint8_t s1, uint8_t s2, uint8_t c1, uint8_t c2);

#endif /* SEG7DIGIT_H_ */
