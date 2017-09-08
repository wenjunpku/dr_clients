#include "dr_api.h"
#include "drmgr.h"
#include "drsyms.h"
#include "utils.h"

#define LOOK(a) dr_printf("%s: %s\n", (#a), a);


static void serverfunc(void * arg){
	dr_printf("Created Client Thread\n");
}
void startClientServer(){
	dr_printf("Client server start!\n");
	if (!dr_create_client_thread(serverfunc, NULL))
		dr_printf("Create child process failure\n");
	//printf(dr_create_client_thread(serverfunc, NULL));
	dr_sleep(1000);
}


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

DR_EXPORT void
dr_client_main(client_id_t id, int argc, const char *argv[])
{
	if (argc != 2) {
		dr_printf("you must input a 5 bit id after the .dll file!\n"
			"Default id: 00000\n");
		user_id = 0;
	}
	user_id = atoi(argv[1]);
	startClientServer();

	dr_set_client_name("DynamoRIO Sample Client 'call_trace'",
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
	/* We're going to dump our data to a per-thread file.
	* On Windows we need an absolute path so we place it in
	* the same directory as our library. We could also pass
	* in a path as a client argument.
	*/
	char * client_path = dr_get_client_path(my_id);
	int len = strlen(client_path);
	int lastsep = 0;
	int cnt = 0;
	for (int i = len - 1; i >= 0; i--){
		lastsep = i;
		if (client_path[i] == '\\') {
			cnt++;
		}
		if (cnt == 3)
			break;
	}
	client_path[lastsep + 1] = '\0';
	char * logdir = strcat(client_path, "logs");
	f = log_file_open(user_id, drcontext, NULL,
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

static void
at_call(app_pc instr_addr, app_pc target_addr)
{
	file_t f = (file_t)(ptr_uint_t)
		drmgr_get_tls_field(dr_get_current_drcontext(), tls_idx);
	dr_mcontext_t mc = { sizeof(mc), DR_MC_CONTROL/*only need xsp*/ };
	dr_get_mcontext(dr_get_current_drcontext(), &mc);
	print_address(f, instr_addr,  "CALL FROM: ");
	print_address(f, target_addr, "       TO: ");
	dr_fprintf(f,                 "    STACK: "PFX"\n", mc.xsp);
	dr_fprintf(f, "\n");
}

static void
at_call_ind(app_pc instr_addr, app_pc target_addr)
{
	file_t f = (file_t)(ptr_uint_t)
		drmgr_get_tls_field(dr_get_current_drcontext(), tls_idx);
	print_address(f, instr_addr,  "INDCALL F: ");
	print_address(f, target_addr, "       TO: ");
	dr_fprintf(f, "\n");
}

static void
at_return(app_pc instr_addr, app_pc target_addr)
{
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

