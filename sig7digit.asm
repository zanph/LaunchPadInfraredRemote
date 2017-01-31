; Assembly file for seg7DigitUpdate() function
; Lab 3, ECE 367, fall 2016
; Created by Zhao Zhang

; The 7-segment coding table
					.data
g_seg7Coding		.byte 	00111111b			; digit 0
					.byte 	00000110b 			; digit 1
					.byte 	01011011b			; digit 2
					.byte	01001111b			; digit 3
					.byte   01100110b			; digit 4
					.byte	01101101b			; digit 5
					.byte	01111101b			; digit 6
					.byte	00000111b			; digit 7
					.byte	01111111b			; digit 8
					.byte	01101111b			; digit 9
					.byte	00000000b			; blank (10)

; Output code, equivalent to declare "uint8_t code[4]" as static variable
					.bss 	g_code, 4

; Function prototype: seg7DigitUpdate(uint8_t s1, uint8_t s2, uint8_t c1, uint8_t c2);
; The function encoded the digits and calls seg7Update() to display the digits on
; the 4-digit 7-segment display. The colon should always be on.
					.text
					.global seg7DigitUpdate		; make this symbol visible to outside
					.global seg7Update			; use this global symbol

; TI assembler requires that symbols in data/bss sections be re-declared in code section before use
seg7Coding			.field	g_seg7Coding
code				.field  g_code


seg7DigitUpdate

					PUSH {lr} 
					PUSH {r5}

					LDR	r5, seg7Coding ; base address of g_seg7Coding		
					LDR	r9, code ; base address of g_code		

					LDRB	r0, [r5, r0] ; use supplied args r0-r3 to grab their 7seg encoded counterparts
					LDRB	r1, [r5, r1]
					LDRB	r2, [r5, r2]
					LDRB	r3, [r5, r3]

					STRB	r0, [r9, #3] ; leftmost number
					STRB	r1, [r9, #2] ; second from left
					STRB	r2, [r9, #1] ; second from right
					STRB	r3, [r9, #0] ; rightmost number
exit
					LDR	r0, code ; argument for seg7Update		

					BL	seg7Update	
					POP		{r5}
					POP		{lr}
					BX		lr

					.end
