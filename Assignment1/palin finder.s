.global _start


_start:
	// Here your execution starts
	//address of the start of var input
	ldr r4, =input
	
	//value of the input string
	ldr r6, [r4]
	
	//r10: value of the register of the first character
	mov r10, r4
	add r14, r14, #16

	bl establish
	
	//r11: value of the register of the last character
	//establish returns the register of the last character of the string, that is a null character
	//therefore we substract one value to get the register of not null last character 
	sub r0, r0, #1
	mov r11, r0
	
	//to check if it has only one character
	bl check
	//If after removing the spaces the registers have the same values, it means it has only one character
	cmp r10, r11
	beq is_no_palindrom
	
	
	bl compare
	b _exit 

//this function establish the register of the last character and stores it in r0
establish:
    
    //r0 is the register of the first character
	//r4 contains the first register
    mov r0, r4
	//r1 contains the value of the register, so the first character
	ldrb r1, [r0]
repeat:
    
    
	cmp r1, #0x00 
    bne .+8
	mov r15, r14
	add r0, r0, #1
    ldrb r1, [r0]
    b repeat

//This function removes spaces
check:  
     cmp r2, #32
	 //if it's not a space character I can go on in this function and check if the other one is 
	 //if it's not a space I skip the following two instructions
	 bne .+16
     add r0, r0, #1 
	 ldrb r2, [r0]
	 beq check
	 
	 cmp r3, #32
	 bne .+16
	 //r1 is the register of the last character
     sub r1, r1, #1 
	 ldrb r3, [r1]
	 beq check
	 bx lr


compare: 
   //r0: value of the register of the first character
     mov r0, r10
   //r1: value of the register of the last character
   //moves these values in the registers r0 and r11 in order to pass them as parameters
	 mov r1, r11
	 	 
loop2:	
   //value stored in the position of the first register--> load the first character
     ldrb r2, [r0]
	 //value of the last character
     ldrb r3, [r1]
	 
	 //verifies if there is a white space, now we have the first character in r1 that
	 //is not a white space and the last character in r3
	 bl check
	 
	 
	 //Convert the current characters in lower case if they aren't yet
	 bl convert
	 //compare two characters: if they are not equal they are not palindrom
     cmp r2, r3
	 
	 bne is_no_palindrom
	 sub r4, r1, r0	 
	 //we have already checked if they are the same
	 //now we check the difference between the two registers 
	 cmp r4, #1
	 //substract the values of the two registers: if they are less than or equal to 1 it means they are palindrom

	 ble is_palindrom
	 add r0, r0, #1 
     sub r1, r1, #1 
	 
	 b loop2

convert:
     //r2=first character
	 //r3=last character
	 //I convert from upper case to lower case
	 cmp r2, #97//if it's gretaer than 61 it means it's already in lower case
	 bge .+8
	 //if it's not I add 32 
	 add r2, r2, #32
	 
	 cmp r3, #97//if it's gretaer than 90 it means it's already in lower case
	 bge .+8
	 //if it's not I add 32 
	 add r3, r3, #32	 
	 bx lr
     
	
	
is_palindrom:
   
	ldr r3, =0xFF200000
	//to switch on the 5 leftmost LEDs we have to write 1 bit for each position, so we have to write the number
	//1111100000 that is 992 in decimal
	mov r4, #31
	str r4, [r3]
	
	

	// Write 'Palindrom detected' to UART
	ldr r5, =0xFF201000
	ldr r6, =PALINDROM
	
//This loop takes every character from the string, verifies if it isn't null and writes it in the port JUART with the correct offset
for:	
	ldrb r7, [r6]
	cmp r7, #0
	// string is null-terminated, therefore it's the last character
    beq _exit 
    str r7, [r5]
    add r6, r6, #1
    b for
	str r6, [r5]
	b _exit


is_no_palindrom:
	// Switch on only the 5 leftmost LEDs
    ldr r0, =0xFF200000
	//to switch on the 5 rightmost LEDs we have to write 1 bit for each position, so we have to write the number
	//1111100000 that is 992 in decimal
	mov r1, #992
	str r1, [r0]
	
	// Write 'Not a palindrom' to UART
	ldr r5, =0xFF201000
	ldr r6, =NOT_PALINDROM
	b for

_exit:
	// Branch here for exit
	b .
	
.data
.align
	// This is the input you are supposed to check for a palindrom
	// You can modify the string during development, however you
	// are not allowed to change the name 'input'!
//'"G  "	
input: .asciz "aa ao"

PALINDROM: .asciz "\nPalindrom detected"
NOT_PALINDROM: .asciz "\nNot a palindrom"
output: 
.end