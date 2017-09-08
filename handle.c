#include<stdio.h>

typedef int(*callmehod)(int);
static void startFake(int param){
	int addr = 0x00401530;
		//0x00401530;
		// 0x00401500;
	callmehod m = (callmehod)addr;
	
	int i = m(param);
	printf("Fakecall:%d\n", i);
	return;
}
