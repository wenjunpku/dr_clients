#include "dr_api.h"
#include "drmgr.h"
#include "drsyms.h"
#include "utils.h"
#include "dr_tools.h"
#include <io.h>
#include <stdio.h>

#define LOOK(a) dr_printf("%s: %s\n", (#a), a);
#define LOOK_INT(a) dr_printf("%s: %d\n", (#a), a);
#define FILTER_MODNAME true

static void event_exit(void);
static void event_thread_init(void *drcontext);
static void event_thread_exit(void *drcontext);
static dr_emit_flags_t event_app_instruction(void *drcontext, void *tag,
	instrlist_t *bb, instr_t *instr,
	bool for_trace, bool translating,
	void *user_data);
static int tls_idx;

static client_id_t my_id;
static client_id_t user_id;
static bool has_valid_modname_file;
bool read_valid_modname();

DR_EXPORT void
dr_client_main(client_id_t id, int argc, const char *argv[])
{
	has_valid_modname_file = read_valid_modname();
	dr_set_client_name("Simple DynamoRIO Client 'call_trace'",
		"http://dynamorio.org/issues");
	drmgr_init();
	my_id = id;
	/* make it easy to tell, by looking at log file, which client executed */
	dr_log(NULL, LOG_ALL, 1, "Client 'instrcalls' initializing\n");
	/* also give notification to stderr */
#ifdef SHOW_RESULTS
	if (dr_is_notify_on()) {
# ifdef WINDOWS
		/* ask for best-effort printing to cmd window.  must be called at init. */
		dr_enable_console_printing();
# endif
		dr_fprintf(STDERR, "Client instrcalls is running\n");
	}
#endif
	dr_register_exit_event(event_exit);
	drmgr_register_bb_instrumentation_event(NULL, event_app_instruction, NULL);
	drmgr_register_thread_init_event(event_thread_init);
	drmgr_register_thread_exit_event(event_thread_exit);
	if (drsym_init(0) != DRSYM_SUCCESS) {
		dr_log(NULL, LOG_ALL, 1, "WARNING: unable to initialize symbol translation\n");
	}
	tls_idx = drmgr_register_tls_field();
	DR_ASSERT(tls_idx > -1);
	dr_printf("===============program start===================\n");
}

static void
event_exit(void)
{
	if (drsym_exit() != DRSYM_SUCCESS) {
		dr_log(NULL, LOG_ALL, 1, "WARNING: error cleaning up symbol library\n");
	}
	drmgr_unregister_tls_field(tls_idx);
	drmgr_exit();
}

#ifdef WINDOWS
# define IF_WINDOWS(x) x
#else
# define IF_WINDOWS(x) /* nothing */
#endif

static void
event_thread_init(void *drcontext)
{
	file_t f;
	char cur_dir[256];
	dr_get_current_directory(cur_dir, 256);
	bool create_flag = dr_create_dir(strcat(cur_dir, "\\trace\\logs"));
	if (create_flag) dr_printf("create dir: %s\n", cur_dir);

	f = log_file_open(my_id, drcontext, cur_dir,
		"call_trace",	
#ifndef WINDOWS
		DR_FILE_CLOSE_ON_FORK |
#endif
		DR_FILE_ALLOW_LARGE);
	DR_ASSERT(f != INVALID_FILE);

	/* store it in the slot provided in the drcontext */
	drmgr_set_tls_field(drcontext, tls_idx, (void *)(ptr_uint_t)f);
}

static void
event_thread_exit(void *drcontext)
{
	log_file_close((file_t)(ptr_uint_t)drmgr_get_tls_field(drcontext, tls_idx));
}

# define MAX_SYM_RESULT 256
# define MAX_MODENAME 128
# define MAX_PRINT_LEN "30"
# define MAX_FILE_NUM 512

#define MAXNUM 20000
char valid_files[MAXNUM][200];
int valid_file_num = 0;

bool read_valid_modname() {
	char cur_dir[256] = {0};
	dr_get_current_directory(cur_dir, 256);
	LOOK(cur_dir);
	
	file_t valid_file = dr_open_file(strcat(cur_dir, "\\trace\\valid_files.txt"), DR_FILE_READ);
	if (valid_file == INVALID_FILE) {
		dr_printf("%s: File not exist\n", cur_dir);
		dr_printf("Print all files");
		return false;
	}

	char text[1024];
	int file_num = 0;
	ssize_t file_len;
	file_len = dr_read_file(valid_file, text, 1024);
	dr_close_file(valid_file);
	
	if (file_len == 1024) dr_printf("Warning: valid_file length too long!");
	char line[128];
	int line_i = 0;
	for (int i = 0; i < file_len; i++) {
		if (text[i] == '\n') {
			line[line_i] = '\0';
			if (strlen(line) > 0) strcpy(valid_files[file_num++], line);
			line_i = 0;
			continue;
		}
		line[line_i++] = text[i];
	}

	dr_printf("Valid modnames: %d\n", file_num);
	for (int i = 0; i < file_num; i++) {
		dr_printf("\t%s\n", valid_files[i]);
	}
	dr_printf("\n");
	valid_file_num = file_num;
	if (file_num == 0) return false;
	return true;
}

bool is_valid_modname(char * modname)
{
	if (!has_valid_modname_file) return true;
	bool ret = false;
	for (int i = 0; i < valid_file_num; i++) {
		if (!strcmp(modname, valid_files[i])) {
			ret = true;
			break;
		}
	}
	return ret;
}


static void
print_address(file_t f, app_pc addr, const char *prefix)
{
	drsym_error_t symres;
	drsym_info_t sym;
	char name[MAX_SYM_RESULT] = "";
	char file[MAXIMUM_PATH] = "";
	const char * modname = "<no_name>";
	module_data_t *data;
	data = dr_lookup_module(addr);

	sym.struct_size = sizeof(sym);
	sym.name = name;
	sym.name_size = MAX_SYM_RESULT;
	sym.file = file;
	sym.file_size = MAXIMUM_PATH;

	if (data == NULL) {
		sym.name = "<not found>";
		sym.file = "";
		dr_fprintf(f, "%s"PFX" AT %"MAX_PRINT_LEN"s+"PIFX" IN %"MAX_PRINT_LEN"s", prefix, addr,
			sym.name, 0, modname);
	}
	else {
		symres = drsym_lookup_address(data->full_path, addr - data->start, &sym,
			DRSYM_DEFAULT_FLAGS);
		if (symres == DRSYM_SUCCESS || symres == DRSYM_ERROR_LINE_NOT_AVAILABLE) {
			const char * _modname = dr_module_preferred_name(data);
			if (_modname != NULL)
				modname = _modname;

			if (symres == DRSYM_ERROR_LINE_NOT_AVAILABLE) {
				//sym.name = "<error cannot find>";
				sym.file = "";
			}
		}
		
		dr_fprintf(f, "%s"PFX" AT %"MAX_PRINT_LEN"s+"PFX" IN %"MAX_PRINT_LEN"s\n", prefix, addr,
			sym.name, addr - data->start - sym.start_offs, modname);
		dr_free_module_data(data);
	}
}

static bool valid_instr(app_pc instr_addr, app_pc target_addr) {
	if (!FILTER_MODNAME) return true;
	
	drsym_error_t symres;
	drsym_info_t sym;
	char name[MAX_SYM_RESULT] = "";
	char file[MAXIMUM_PATH] = "";
	sym.struct_size = sizeof(sym);
	sym.name = name;
	sym.name_size = MAX_SYM_RESULT;
	sym.file = file;
	sym.file_size = MAXIMUM_PATH;
	module_data_t *data;
	
	bool ret1 = false;
	data = dr_lookup_module(instr_addr);
	if (data != NULL) {
		symres = drsym_lookup_address(data->full_path, instr_addr - data->start, &sym,
			DRSYM_DEFAULT_FLAGS);
		if (symres == DRSYM_SUCCESS || symres == DRSYM_ERROR_LINE_NOT_AVAILABLE) {
			const char * modname = dr_module_preferred_name(data);
			if (modname != NULL && is_valid_modname(modname))
				ret1 = true;
		}
	}

	bool ret2 = false;
	data = dr_lookup_module(instr_addr);
	if (data != NULL) {
		symres = drsym_lookup_address(data->full_path, instr_addr - data->start, &sym,
			DRSYM_DEFAULT_FLAGS);
		if (symres == DRSYM_SUCCESS || symres == DRSYM_ERROR_LINE_NOT_AVAILABLE) {
			const char * modname = dr_module_preferred_name(data);
			if (modname != NULL && is_valid_modname(modname))
				ret2 = true;
		}
	}
	return ret1 || ret2;
}

static void
at_call(app_pc instr_addr, app_pc target_addr)
{
	if (!valid_instr(instr_addr, target_addr)) return;
	file_t f = (file_t)(ptr_uint_t)
		drmgr_get_tls_field(dr_get_current_drcontext(), tls_idx);
	dr_mcontext_t mc = { sizeof(mc), DR_MC_CONTROL/*only need xsp*/ };
	dr_get_mcontext(dr_get_current_drcontext(), &mc);
	print_address(f, instr_addr,  "CALL FROM: ");
	print_address(f, target_addr, "       TO: ");
	//dr_fprintf(f,                 "    STACK: "PFX"\n", mc.xsp);
	dr_fprintf(f, "\n");
}

static void
at_call_ind(app_pc instr_addr, app_pc target_addr)
{
	if (!valid_instr(instr_addr, target_addr)) return;
	file_t f = (file_t)(ptr_uint_t)
		drmgr_get_tls_field(dr_get_current_drcontext(), tls_idx);
	print_address(f, instr_addr,  "INDCALL F: ");
	print_address(f, target_addr, "       TO: ");
	dr_fprintf(f, "\n");
}

static void
at_return(app_pc instr_addr, app_pc target_addr)
{
	if (!valid_instr(instr_addr, target_addr)) return;
	file_t f = (file_t)(ptr_uint_t)
		drmgr_get_tls_field(dr_get_current_drcontext(), tls_idx);
	print_address(f, instr_addr,  "RETURN FR: ");
	print_address(f, target_addr, "       TO: ");
	dr_fprintf(f, "\n");
}

static dr_emit_flags_t
event_app_instruction(void *drcontext, void *tag, instrlist_t *bb, instr_t *instr,
bool for_trace, bool translating, void *user_data)
{
#ifdef VERBOSE
	if (drmgr_is_first_instr(drcontext, instr)) {
		dr_printf("in dr_basic_block(tag="PFX")\n", tag);
# if VERBOSE_VERBOSE
		instrlist_disassemble(drcontext, tag, bb, STDOUT);
# endif
	}
#endif
	/* instrument calls and returns -- ignore far calls/rets */
	if (instr_is_call_direct(instr)) {
		dr_insert_call_instrumentation(drcontext, bb, instr, (app_pc)at_call);
	}
	else if (instr_is_call_indirect(instr)) {
		dr_insert_mbr_instrumentation(drcontext, bb, instr, (app_pc)at_call_ind,
			SPILL_SLOT_1);
	}
	else if (instr_is_return(instr)) {
		dr_insert_mbr_instrumentation(drcontext, bb, instr, (app_pc)at_return,
			SPILL_SLOT_1);
	}
	return DR_EMIT_DEFAULT;
}
