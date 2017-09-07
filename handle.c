#include<stdio.h>

typedef int(*callmehod)(int);
static void startFake(){
	int addr = 0x00401500;
	callmehod m = (callmehod)addr;
	int i = m(5);
	printf("Fakecall:%d\n", i);
	return;
}
