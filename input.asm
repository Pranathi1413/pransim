#bubble sort
.data
arr  
	.word 5 -2 1 -4 6 3
.text 
main:	
	la r0,arr     
	li r1,1		#r1 has true value
	li r2,0      	#r2 is i
	li r4,5 	#r4 has n-1
for1:   slt r5,r2,r4  	
	bne r5,r1,exit1
	li r3,0 	#r3 has j
for2:	sub r6,r4,r2	#r6 has n-i-1
	slt r5,r3,r6
	bne r5,r1,exit2
	lw r7,0(r0)
	lw r8,4(r0)
	slt r5,r8,r7
	bne r5,r1,lab
		sw r7,4(r0)
		sw r8,0(r0)
lab: 	addi r0,r0,4
	addi r3,r3,1
	j for2
exit2:  la r0,arr
	addi r2,r2,1
	j for1
exit1:	