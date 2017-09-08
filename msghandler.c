#include "msghandler.h"
#include "dr_api.h"
#include "dr_tools.h"
#include "handle.c"


void startServer(){
	unsigned tid = 1;
	printf("CreateThread Begin");
	DWORD ThreadId;
	HANDLE ThrHandle = CreateThread(NULL, 0, startServerInternal, NULL, CREATE_SUSPENDED,
		&ThreadId);
	ResumeThread(ThrHandle);
	WaitForSingleObject(ThrHandle, INFINITE);
	printf("CreateThread, and startServer, code=%u\n", tid);
}
static int done = 0;

unsigned __stdcall startServerInternal(void * ptr){
	dr_printf("Created Client Thread\n");
	dr_client_thread_set_suspendable(true);
	WORD wVersionRequested;
	WSADATA wsaData;
	int err;

	//read file 
	char path[1024];
	char buff[1024];
	dr_get_current_directory(path, 1024);
	dr_printf(path+ '\n');
	int count = 0;
	while (1){
		dr_sleep(100);
		count++;
		dr_printf("hello form child thread %d\n", count);

		// you need to replace this path with your communication file's absolute path
		file_t fin = dr_open_file("C:\\internetware\\code\\call_trace\\communication.txt", DR_FILE_READ);
		if (fin == INVALID_FILE)
			dr_printf("Open file failure! %d\n", fin);

		// read one line, then close the file
		ssize_t line_len = dr_read_file(fin, buff, 1024);
		dr_close_file(fin);

		if (line_len > 0){
			dr_printf("read %d bytes\n", line_len);
			dr_printf("%s\n", buff);
			int param = atoi(buff);
			dr_printf("param is %d\n", param);
			if (param > 0)
				startFake(param);
			else
				dr_printf("Invalid params!\n");
			done = 1;
			break;
		}
		
	}
	return 0;
}


void startClientServer(){
	dr_printf("Client server start!\n");
	if (!dr_create_client_thread(startServerInternal, NULL))
		dr_printf("Create child process failure\n");
	dr_client_thread_set_suspendable(true);
	while (!done)
		dr_sleep(100);
}

int main(int c, char*arg[]){
	startServer();
}
void handleMessage(char* s1, char * s2){
	sprintf(s2, "hello:%s\0", s1);
	return;
}

