#include "msghandler.h"
DWORD WINAPI startServerInternal(LPVOID ptr){
	printf("Hello from subthread\n");
	getchar();
	return 0;
}
static int done = 0;
void startServer(){
	if (done)
		return;
	done = 1;
	unsigned tid=1;
	DWORD ThreadId;
	HANDLE ThrHandle = CreateThread(NULL, 0, startServerInternal, NULL, CREATE_SUSPENDED
		,
		&ThreadId);
	ResumeThread(ThrHandle);
	printf( "CreateThread, and startServer, code=%u\n", tid);
}
int main(int c, char*arg[]){
	startServer();
}
void handleMessage(char* s1, char * s2){
	sprintf(s2, "hello:%s\0", s1);
	return;
}

