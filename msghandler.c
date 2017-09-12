#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "dr_api.h"
#include "dr_tools.h"
#include "handle.c"

static bool log_trace_flag = false;
char * file_path;

void startServerInternal(void * ptr){
	dr_printf("Created Client Thread\n");
	dr_client_thread_set_suspendable(true);
	char buff[1024];
	//read file 
	int count = 0;
	while (1){
		count++;
		dr_printf("=============================\n");
		dr_printf("hello form child thread %d\n", count);
		file_t fin = dr_open_file(file_path, DR_FILE_READ);
		if (fin == INVALID_FILE)
			dr_printf("Open file failure! %d\n", fin);

		// read one line, then close the file
		ssize_t line_len = dr_read_file(fin, buff, 1024);
		buff[line_len-1] = '\0';
		dr_close_file(fin);
		if (line_len > 0){
			dr_printf("read %d bytes\n", line_len);
			dr_printf("content is %s\n", buff);
			
			if (strstr(buff, "StartFake")){
				char func[64];
				char param[64];
				dr_sscanf(buff, "%s %s", func, param);
				dr_printf("func name is %s\n", func);
				dr_printf("params is %s\n", param);
				int param_num = atoi(param);
				if (param_num > 0)
					startFake(param_num);
				else
					dr_printf("Invalid params!\n");
			}
			else if (strstr(buff, "StartTrace")){
				log_trace_flag = TRUE;
				dr_printf("Start Trace Log!\n");
			}
			else if (strstr(buff, "EndTrace")){
				log_trace_flag = FALSE;
				dr_printf("End Trace Log!\n");
			}
			dr_sleep(1000);
		}
		dr_sleep(100);
	}
}


void startClientServer(){
	char path[1024];
	dr_get_current_directory(path, 1024);
	strlen(path);
	dr_printf("%s \n", path);
	const char * comm_path = "\\communicate.txt";
	file_path = strcat(path, comm_path);
	if (!dr_file_exists(file_path)){
		file_t fin = dr_open_file(file_path, DR_FILE_WRITE_APPEND);
		dr_flush_file(fin);
		dr_close_file(file_path);
	}

	dr_printf("Client server start!\n");
	if (!dr_create_client_thread(startServerInternal, NULL))
		dr_printf("Create child process failure\n");

}

bool start_log_trace(){
	return log_trace_flag;
}

