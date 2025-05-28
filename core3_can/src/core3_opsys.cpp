#include <core3.h>
#include <core3_opsys.h>
#include <stdlib.h>
#include <time.h>

#ifndef OPSYS_SIM
#include "bootloader_random.h"
#include "esp_random.h"
#else
#include <opsys_sim.h>
#endif

char user_name[32];
char station_name[32];
char cur_path[32];

typedef struct
{
	char** lines;
	size_t lines_count;
	int cmd_ptr;
	int lines_used;
} osProgram_t;

typedef enum
{
	VALUE_TYPE_EMPTY,
	VALUE_TYPE_FLOAT,
	VALUE_TYPE_INT,
	VALUE_TYPE_STRING,
	VALUE_TYPE_PROGRAM,
} osCmdValueType_t;

typedef struct
{
	void* Ptr;
	size_t PtrLen;
	float Float;
	int Int;

	osCmdValueType_t Type;
} osCmdValue_t;

typedef osCmdValue_t* (*execFunc)(char* cmd, char* cmdorig, int* tokens, osCmdValue_t* args, int arg_count);

typedef struct
{
	char* command;
	char* desc;
	int arg_count;
	bool raw_input;
	execFunc onExec;
} osCmd_t;

typedef struct
{
	char* name;
	char* desc;
	osCmdValue_t* value;
} osVar_t;

int os_stack_idx = 0;
osCmdValue_t os_stack[64];

int os_var_idx = 0;
osVar_t os_vars[64];

osProgram_t* current_prog;

osProgram_t* core3_program_alloc(int line_size)
{
	osProgram_t* prog = (osProgram_t*)malloc(sizeof(osProgram_t));

	prog->lines_count = line_size;
	prog->cmd_ptr = 0;
	prog->lines_used = 0;
	prog->lines = (char**)malloc(sizeof(char*) * line_size);
	return prog;
}

char* core3_string_copy(const char* str)
{
	size_t newlen = strlen(str) + 1;
	char* newstr = (char*)malloc(newlen);
	memset(newstr, 0, newlen);
	sprintf(newstr, "%s", str);
	return newstr;
}

void core3_program_addlne(osProgram_t* prog, const char* line)
{
	if (prog->lines_used < prog->lines_count)
	{
		char* line_cpy = core3_string_copy(line);
		prog->lines[prog->lines_used] = line_cpy;
		prog->lines_used++;
	}
}

void core3_program_clear(osProgram_t* prog)
{
}

void core3_program_run(osProgram_t* prog)
{
}

void core3_val_set_string(osCmdValue_t* cmdval, const char* str)
{
	size_t len = strlen(str);
	char* strbuf = (char*)malloc(len + 1);
	memset(strbuf, 0, len);
	sprintf(strbuf, "%s", str);

	cmdval->Ptr = strbuf;
	cmdval->PtrLen = len;
	cmdval->Int = -VALUE_TYPE_STRING;
	cmdval->Float = -VALUE_TYPE_STRING;
	cmdval->Type = VALUE_TYPE_STRING;
}

void core3_val_set_float(osCmdValue_t* cmdval, float val)
{
	cmdval->Ptr = NULL;
	cmdval->Int = -VALUE_TYPE_FLOAT;
	cmdval->Type = VALUE_TYPE_FLOAT;
	cmdval->Float = val;
}

void core3_val_set_int(osCmdValue_t* cmdval, int val)
{
	cmdval->Float = -VALUE_TYPE_INT;
	cmdval->Ptr = NULL;
	cmdval->Int = val;
	cmdval->Type = VALUE_TYPE_INT;
}

void core3_val_set_program(osCmdValue_t* cmdval, osProgram_t* prog)
{
	cmdval->Int = -VALUE_TYPE_PROGRAM;
	cmdval->Float = -VALUE_TYPE_PROGRAM;
	cmdval->Type = VALUE_TYPE_PROGRAM;

	cmdval->Ptr = prog;
	cmdval->PtrLen = sizeof(osProgram_t);
}

osVar_t* core3_opsys_getvar(const char* name)
{
	osVar_t* var = NULL;

	printf("[getvar] '%s'\n", name);

	for (size_t i = 0; i < os_var_idx; i++)
	{
		if (strcmp(os_vars[i].name, name) == 0)
		{
			var = &os_vars[i];
			break;
		}
	}

	if (var == NULL)
	{
		if (os_var_idx >= sizeof(os_vars) / sizeof(*os_vars))
			return NULL;

		var = &os_vars[os_var_idx++];

		size_t name_len = strlen(name);
		var->name = (char*)malloc(name_len + 1);
		memset((void*)var->name, 0, name_len);
		sprintf(var->name, "%s", name);

		var->value = (osCmdValue_t*)malloc(sizeof(osCmdValue_t));
		core3_val_set_float(var->value, 0);
	}

	return var;
}

void core3_opsys_setvar(const char* name, osCmdValue_t* val)
{
	osVar_t* var = core3_opsys_getvar(name);

	printf("[setvar] '%s'\n", name);

	var->value->Float = val->Float;
	var->value->Int = val->Int;
	var->value->Type = val->Type;
	var->value->PtrLen = val->PtrLen;

	if (val->Ptr != NULL && val->PtrLen > 0)
	{
		var->value->Ptr = malloc(val->PtrLen);
		memcpy(var->value->Ptr, val->Ptr, val->PtrLen);
	}
}

osCmdValue_t* core3_opsys_stack_push()
{
	if (os_stack_idx >= 32)
		return NULL;

	osCmdValue_t* ret = &os_stack[os_stack_idx++];
	memset(ret, 0, sizeof(osCmdValue_t));
	return ret;
}

void core3_opsys_clone_value(osCmdValue_t* dst, osCmdValue_t* src)
{
	dst->Float = src->Float;
	dst->Int = src->Int;
	dst->Ptr = src->Ptr;
	dst->PtrLen = src->PtrLen;
	dst->Type = src->Type;

	if (dst->Ptr != NULL && dst->PtrLen > 0)
	{
		uint8_t* new_mem = (uint8_t*)malloc(dst->PtrLen);
		memcpy(new_mem, dst->Ptr, dst->PtrLen);
		dst->Ptr = new_mem;
	}
}

osCmdValue_t* core3_opsys_stack_push_copy(osCmdValue_t* copy)
{
	osCmdValue_t* new_val = core3_opsys_stack_push();

	if (copy != NULL)
	{
		core3_opsys_clone_value(new_val, copy);
	}

	return new_val;
}

osCmdValue_t* core3_opsys_stack_peek(int backIdx)
{
	return &os_stack[os_stack_idx - backIdx - 1];
}

osCmdValue_t* core3_opsys_stack_pop(bool cleanup)
{
	os_stack_idx--;
	osCmdValue_t* ret = &os_stack[os_stack_idx];

	if (cleanup && ret->Ptr != NULL)
	{
		free(ret->Ptr);
		ret->Ptr = NULL;
	}

	return ret;

	if (os_stack_idx < 0)
		os_stack_idx = 0;
}

void core3_opsys_stack_clear()
{
	for (size_t i = 0; i < os_stack_idx; i++)
	{
		if (os_stack[i].Ptr != NULL && os_stack[i].PtrLen > 0)
		{
			free(os_stack[i].Ptr);
			os_stack[i].Ptr = NULL;
			os_stack[i].PtrLen = 0;
		}
	}

	os_stack_idx = 0;
}

int os_cmds_idx = 0;
osCmd_t os_cmds[16];

osCmdValue_t* last_command_return;

void core3_opsys_register(const char* command, bool raw_input, int arg_count, execFunc onExec)
{
	int idx = os_cmds_idx++;

	size_t cmd_len = strlen(command) + 1;
	os_cmds[idx].command = (char*)malloc(cmd_len);
	memset(os_cmds[idx].command, 0, cmd_len);
	sprintf(os_cmds[idx].command, "%s", command);
	os_cmds[idx].onExec = onExec;
	os_cmds[idx].arg_count = arg_count;
	os_cmds[idx].raw_input = raw_input;

	printf("Reg> %s - 0x%p\n", command, onExec);
}

osCmd_t* core3_opsys_find_command(const char* command)
{
	for (size_t i = 0; i < os_cmds_idx; i++)
	{
		if (strcmp(command, os_cmds[i].command) == 0)
			return &os_cmds[i];
	}

	return NULL;
}

size_t getLineInput(char buf[], size_t len)
{
	memset(buf, 0, len);

	fflush(stdout);
	fflush(stdin);
	fpurge(stdin); // clears any junk in stdin
	size_t write_len = 0;

	char* bufp = buf;
	while (true)
	{
		vTaskDelay(pdMS_TO_TICKS(10));

		*bufp = getchar();
		if (*bufp != '\0' && *bufp != 0xFF && *bufp != '\r') // ignores null input, 0xFF, CR in CRLF
		{
			//'enter' (EOL) handler
			if (*bufp == '\n')
			{
				printf("\n");
				fflush(stdout);
				*bufp = '\0';

				getchar();
				break;
			} // backspace handler
			else if (*bufp == '\b')
			{
				if (bufp - buf >= 1)
				{
					printf("\b \b");
					fflush(stdout);
					bufp--;
					write_len--;
				}
			}
			else
			{
				printf("%c", *bufp);
				fflush(stdout);
				// pointer to next character
				bufp++;
				write_len++;
			}
		}

		// only accept len-1 characters, (len) character being null terminator.
		if (bufp - buf > (len)-2)
		{
			bufp = buf + (len - 1);
			*bufp = '\0';
			break;
		}
	}

	return write_len;
}

void tokenize(char* cmd, int len, int* tokens, int tokens_len, int* cur_tok)
{
	bool last_was_null = true;
	bool parsing_quote = false;
	int bracket_cnt = 0;

	for (size_t i = 0; i < tokens_len; i++)
	{
		tokens[i] = 0;
	}

	for (size_t i = 0; i < len; i++)
	{
		if (cmd[i] == '"')
		{
			parsing_quote = !parsing_quote;

			if (!parsing_quote)
			{
				cmd[i] = 0;
				last_was_null = true;
			}
		}
		else if ((cmd[i] == '{' || cmd[i] == '}') && !parsing_quote)
		{
			if (cmd[i] == '{')
			{
				if (bracket_cnt == 0)
				{
					last_was_null = false;
					tokens[*cur_tok] = i + 1;
					*cur_tok = *cur_tok + 1;
				}

				bracket_cnt++;
			}
			else
			{
				bracket_cnt--;

				if (bracket_cnt == 0)
				{
					cmd[i] = 0;
					last_was_null = true;
				}
			}
		}
		else if ((cmd[i] == ' ' || cmd[i] == '\t') && !parsing_quote && bracket_cnt == 0)
		{
			cmd[i] = 0;
			last_was_null = true;
		}
		else
		{
			if (last_was_null)
			{
				last_was_null = false;
				tokens[*cur_tok] = i;
				*cur_tok = *cur_tok + 1;
			}
		}
	}
}

void print_last_command_ret()
{
	if (last_command_return != NULL)
	{
		printf("= [%f, %d]\n", last_command_return->Float, last_command_return->Int);
	}
}

osCmdValue_t* perform_command(char* cmd, int len)
{
	char cmd_orig[128];
	memset(cmd_orig, 0, sizeof(cmd_orig));
	sprintf(cmd_orig, "%s", cmd);

	char cmd_orig2[128];
	memset(cmd_orig2, 0, sizeof(cmd_orig2));
	sprintf(cmd_orig2, "%s", cmd);

	printf("[cmd in] '%s'\n", cmd);

	// Tokenizing
	int cur_tok = 0;
	int tokens[32];
	tokenize(cmd, len, tokens, sizeof(tokens) / sizeof(*tokens), &cur_tok);

	/*for (size_t i = 0; i < cur_tok; i++)
	{
		printf("[tok %d] %s\n", i, &cmd[tokens[i]]);
	}
	return NULL;
	*/

	int arg_counter = 0;
	osCmdValue_t arg_stack[8];

	if (cur_tok == 0)
		return NULL;

	char* str_cmd = &cmd[tokens[0]];
	osCmd_t* osCmd = core3_opsys_find_command(str_cmd);

	last_command_return = NULL;

	if (osCmd == NULL)
	{
		if (strspn(str_cmd, "-0123456789.") == strlen(str_cmd))
		{

			osCmdValue_t* val = core3_opsys_stack_push();
			core3_val_set_float(val, strtof(str_cmd, NULL));

			printf("Is number '%f'\n", val->Float);
			last_command_return = val;
			return val;
		}

		if (str_cmd[0] == '$')
		{
			osCmdValue_t* val = core3_opsys_stack_push();
			core3_val_set_string(val, str_cmd + 1);

			last_command_return = val;
			return val;
		}

		if (str_cmd[0] == '@')
		{
			osVar_t* var = core3_opsys_getvar(str_cmd + 1);
			osCmdValue_t* val = core3_opsys_stack_push_copy(var->value);

			last_command_return = val;
			return val;
		}

		printf("Command not found '%s'\n", str_cmd);
	}
	else
	{
		if (cur_tok < osCmd->arg_count)
		{
			printf("%s - Invalid number of args, expected %d, got %d\n", str_cmd, osCmd->arg_count, cur_tok);
			return NULL;
		}
		else
		{
			if (osCmd->raw_input)
			{
			}
			else
			{
				for (size_t i = 1; i < cur_tok; i++) //(size_t i = 1; i < cur_tok; i++)
				{
					char* cmd2 = &cmd[tokens[i]];
					osCmdValue_t* arg_ret = perform_command(cmd2, strlen(cmd2));

					if (arg_ret == NULL)
						return NULL;

					arg_stack[arg_counter++] = *arg_ret;
				}
			}

			last_command_return = osCmd->onExec(cmd, cmd_orig2, tokens, &arg_stack[0], arg_counter);
			if (last_command_return != NULL)
				core3_opsys_stack_pop(true);

			for (size_t i = 0; i < arg_counter; i++)
			{
				core3_opsys_stack_pop(true);
			}

			if (last_command_return != NULL)
				core3_opsys_stack_push_copy(last_command_return);

			// print_last_command_ret();
		}
	}

	return last_command_return;
}

osCmdValue_t* core3_cmd_print(char* cmd, char* cmdorig, int* tokens, osCmdValue_t* args, int arg_count)
{
	for (size_t i = 0; i < arg_count; i++)
	{
		switch (args[i].Type)
		{
		case VALUE_TYPE_EMPTY:
			break;

		case VALUE_TYPE_FLOAT:
			printf("%f\n", args[i].Float);
			break;

		case VALUE_TYPE_INT:
			printf("%d\n", args[i].Int);
			break;

		case VALUE_TYPE_STRING:
			if (args[i].Ptr != NULL && args[i].PtrLen > 0)
				printf("%s\n", (const char*)args[i].Ptr);
			else
				printf("ERR_STRING\n");
			break;

		case VALUE_TYPE_PROGRAM: {
			osProgram_t* prog = (osProgram_t*)args[i].Ptr;
			printf("=== PROGRAM ===\n");

			if (prog == NULL)
			{
				printf("# null\n");
			}
			else
			{
				printf("# lines %d\n", prog->lines_used);

				for (size_t i = 0; i < prog->lines_used; i++)
				{
					printf("%s\n", prog->lines[i]);
				}
			}

			printf("=== END PROGRAM ===\n");
		}
							   break;

		default:
			printf("print - Unknown variable type %d\n", args[i].Type);
			break;
		}
	}

	return NULL;
}

osCmdValue_t* core3_cmd_rnd(char* cmd, char* cmdorig, int* tokens, osCmdValue_t* args, int arg_count)
{
	osCmdValue_t* val = core3_opsys_stack_push();
	core3_val_set_int(val, esp_random());
	return val;
}

osCmdValue_t* core3_cmd_rndf(char* cmd, char* cmdorig, int* tokens, osCmdValue_t* args, int arg_count)
{
	osCmdValue_t* val = core3_opsys_stack_push();
	core3_val_set_float(val, (float)esp_random() / UINT32_MAX);
	return val;
}

osCmdValue_t* core3_cmd_add(char* cmd, char* cmdorig, int* tokens, osCmdValue_t* args, int arg_count)
{
	float sum = 0;

	for (size_t i = 0; i < arg_count; i++)
	{
		sum = sum + args[i].Float;
	}

	osCmdValue_t* ret = core3_opsys_stack_push();
	core3_val_set_float(ret, sum);
	return ret;
}

osCmdValue_t* core3_cmd_program(char* cmd, char* cmdorig, int* tokens, osCmdValue_t* args, int arg_count)
{
	osCmdValue_t* ret = NULL;

	char* arg1 = &cmd[tokens[1]];
	// size_t arg1_len = strlen(arg1);

	// printf("Variable '%s'\n", (const char *)args[0].Ptr);

	osVar_t* var = core3_opsys_getvar((const char*)arg1);

	if (var->value->Type == VALUE_TYPE_PROGRAM)
	{
		ret = core3_opsys_stack_push_copy(var->value);
	}
	else
	{
		osProgram_t* prog = core3_program_alloc(64);
		current_prog = prog;

		ret = core3_opsys_stack_push();
		core3_val_set_program(ret, prog);

		core3_opsys_clone_value(var->value, ret);
	}

	return ret;
}

osCmdValue_t* core3_cmd_progline(char* cmd, char* cmdorig, int* tokens, osCmdValue_t* args, int arg_count)
{
	if (current_prog == NULL)
		return NULL;

	int line_no = atoi(&cmdorig[tokens[1]]);
	char* line = (&cmdorig[tokens[2]]);

	// printf("Line %d - '%s'\n", line_no, line);

	int idx = line_no;

	if (line_no >= current_prog->lines_used)
	{
		idx = current_prog->lines_used++;
	}

	size_t len = strlen(line) + 1;
	current_prog->lines[idx] = (char*)malloc(len);
	memset(current_prog->lines[idx], 0, len);
	sprintf(current_prog->lines[idx], "%s", line);

	return NULL;
}

osCmdValue_t* core3_cmd_get(char* cmd, char* cmdorig, int* tokens, osCmdValue_t* args, int arg_count)
{
	osCmdValue_t* ret = NULL;
	char* arg1 = (&cmd[tokens[1]]);

	printf("Variable '%s'\n", (const char*)arg1);

	osVar_t* var = core3_opsys_getvar((const char*)arg1);
	ret = core3_opsys_stack_push_copy(var->value);

	return ret;
}

osCmdValue_t* core3_cmd_set(char* cmd, char* cmdorig, int* tokens, osCmdValue_t* args, int arg_count)
{
	osCmdValue_t* ret = NULL;

	char* arg1 = (&cmd[tokens[1]]);
	char* arg2 = (&cmd[tokens[2]]);

	osVar_t* var = core3_opsys_getvar((const char*)arg1);
	osCmdValue_t* arg2_val = perform_command(arg2, strlen(arg2));

	if (arg2_val != NULL)
	{
		printf("Type = %d\n", arg2_val->Type);
		printf("Ptr = %p\n", arg2_val->Ptr);
		printf("PtrLen = %d\n", arg2_val->PtrLen);
		printf("F = %f\n", arg2_val->Float);
		printf("I = %d\n", arg2_val->Int);

		ret = core3_opsys_stack_push_copy(arg2_val);
		core3_opsys_clone_value(var->value, arg2_val);

		printf("Type = %d\n", var->value->Type);
		printf("Ptr = %p\n", var->value->Ptr);
		printf("PtrLen = %d\n", var->value->PtrLen);
		printf("F = %f\n", var->value->Float);
		printf("I = %d\n", var->value->Int);
	}

	return ret;
}

osCmdValue_t* core3_cmd_result(char* cmd, char* cmdorig, int* tokens, osCmdValue_t* args, int arg_count)
{
	osCmdValue_t* ret = core3_opsys_stack_push_copy(last_command_return);
	return ret;
}

void core3_opsys_init()
{
	char prompt[128];

	memset(user_name, 0, sizeof(user_name));
	memset(station_name, 0, sizeof(station_name));
	memset(cur_path, 0, sizeof(cur_path));
	sprintf(user_name, "user");
	sprintf(station_name, "core3");
	sprintf(cur_path, "/");

	core3_opsys_register("print", false, 1, core3_cmd_print);
	core3_opsys_register("add", false, 2, core3_cmd_add);
	core3_opsys_register("get", true, 1, core3_cmd_get);
	core3_opsys_register("set", true, 2, core3_cmd_set);
	core3_opsys_register("rnd", false, 0, core3_cmd_rnd);
	core3_opsys_register("rndf", false, 0, core3_cmd_rnd);
	core3_opsys_register("result", false, 0, core3_cmd_result);
	core3_opsys_register("program", true, 1, core3_cmd_program);
	core3_opsys_register("progline", true, 1, core3_cmd_progline);

	char line_mem[128] = { 0 };
	size_t line_size = 0;

	while (true)
	{
		memset(prompt, 0, sizeof(prompt));
		sprintf(prompt, "%s@%s %s# ", user_name, station_name, cur_path);
		printf("%s", prompt);

		line_size = getLineInput(line_mem, sizeof(line_mem));

		osCmdValue_t* val = perform_command(line_mem, line_size);
		if (val != NULL)
		{
			core3_cmd_print(NULL, NULL, NULL, val, 1);
		}

		core3_opsys_stack_clear();

		// dprintf("Stack counter: %d\n", os_stack_idx);

		fflush(stdout);

		vTaskDelay(pdMS_TO_TICKS(10));
	}
}