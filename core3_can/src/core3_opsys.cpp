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
    char **lines;
    size_t lines_count;
    int cmd_ptr;
    int lines_used;
    size_t program_counter;
} osProgram_t;

typedef enum
{
    VALUE_TYPE_EMPTY,
    VALUE_TYPE_ALLOCATED,
    VALUE_TYPE_FLOAT,
    VALUE_TYPE_INT,
    VALUE_TYPE_STRING,
    VALUE_TYPE_PROGRAM,
} osCmdValueType_t;

typedef struct
{
    void *Ptr;
    size_t PtrLen;
    float Float;
    int Int;
    osCmdValueType_t Type;

    int RefCount;
} osCmdValue_t;

typedef osCmdValue_t *(*execFunc)(char *cmd, char *cmdorig, int *tokens, osCmdValue_t *args, int arg_count);

typedef struct
{
    char *command;
    char *desc;
    int arg_count;
    bool raw_input;
    execFunc onExec;
} osCmd_t;

typedef struct
{
    char *name;
    char *desc;
    osCmdValue_t *value;
} osVar_t;

osCmdValue_t os_values[64];

// int cleanup_stack_idx = 0;
osCmdValue_t cleanup_stack[32];

int os_var_idx = 0;
osVar_t os_vars[64];

osVar_t *current_prog_var;

osProgram_t *core3_program_alloc(int line_size)
{
    osProgram_t *prog = (osProgram_t *)malloc(sizeof(osProgram_t));

    if (prog == NULL)
    {
        printf("core3_program_alloc out of memory\n");
        return NULL;
    }

    prog->lines_count = line_size;
    prog->cmd_ptr = 0;
    prog->lines_used = 0;
    prog->lines = (char **)malloc(sizeof(char *) * line_size);

    if (prog->lines == NULL)
    {
        printf("core3_program_alloc out of memory #2\n");
        return NULL;
    }

    prog->program_counter = 0;

    for (size_t i = 0; i < line_size; i++)
    {
        prog->lines[i] = NULL;
    }

    return prog;
}

char *core3_string_copy(const char *str)
{
    size_t newlen = strlen(str) + 1;
    char *newstr = (char *)malloc(newlen);

    if (newstr == NULL)
    {
        printf("core3_string_copy out of memory\n");
        return NULL;
    }

    memset(newstr, 0, newlen);
    sprintf(newstr, "%s", str);
    return newstr;
}

bool core3_string_contains(const char *str, const char *cont)
{
    return strstr(str, cont) != NULL;
}

char **core3_string_split(char *str, const char in_delim)
{
    char **result = 0;
    size_t count = 0;
    char *tmp = str;
    char *last_comma = 0;
    char delim[2];
    delim[0] = in_delim;
    delim[1] = 0;

    /* Count how many elements will be extracted. */
    while (*tmp)
    {
        if (in_delim == *tmp)
        {
            count++;
            last_comma = tmp;
        }
        tmp++;
    }

    /* Add space for trailing token. */
    count += last_comma < (str + strlen(str) - 1);

    /* Add space for terminating null string so caller
       knows where the list of returned strings ends. */
    count++;

    result = (char **)malloc(sizeof(char *) * count);

    if (result)
    {
        size_t idx = 0;
        char *token = strtok(str, delim);

        while (token)
        {
            *(result + idx++) = core3_string_copy(token);
            token = strtok(0, delim);
        }
        *(result + idx) = 0;
    }

    return result;
}

void core3_program_addlne(osProgram_t *prog, const char *line)
{
    if (prog->lines_used < prog->lines_count)
    {
        char *line_cpy = core3_string_copy(line);
        prog->lines[prog->lines_used] = line_cpy;
        prog->lines_used++;
    }
}

void core3_val_set_string(osCmdValue_t *cmdval, const char *str)
{
    if (cmdval->Ptr != NULL && cmdval->PtrLen > 0)
    {
        free(cmdval->Ptr);
        cmdval->Ptr = NULL;
        cmdval->PtrLen = 0;
    }

    size_t len = strlen(str);
    char *strbuf = (char *)malloc(len + 1);

    if (strbuf == NULL)
    {
        printf("core3_val_set_string out of memory\n");
        return;
    }

    memset(strbuf, 0, len);
    sprintf(strbuf, "%s", str);

    cmdval->Ptr = strbuf;
    cmdval->PtrLen = len;
    cmdval->Int = -VALUE_TYPE_STRING;
    cmdval->Float = -VALUE_TYPE_STRING;
    cmdval->Type = VALUE_TYPE_STRING;
}

void core3_val_set_float(osCmdValue_t *cmdval, float val)
{
    cmdval->Ptr = NULL;
    cmdval->Int = -VALUE_TYPE_FLOAT;
    cmdval->Type = VALUE_TYPE_FLOAT;
    cmdval->Float = val;
}

void core3_val_set_int(osCmdValue_t *cmdval, int val)
{
    cmdval->Float = -VALUE_TYPE_INT;
    cmdval->Ptr = NULL;
    cmdval->Int = val;
    cmdval->Type = VALUE_TYPE_INT;
}

void core3_val_set_program(osCmdValue_t *cmdval, osProgram_t *prog)
{
    cmdval->Int = -VALUE_TYPE_PROGRAM;
    cmdval->Float = -VALUE_TYPE_PROGRAM;
    cmdval->Type = VALUE_TYPE_PROGRAM;

    cmdval->Ptr = prog;
    cmdval->PtrLen = sizeof(osProgram_t);
}

osVar_t *core3_opsys_getvar(const char *name)
{
    osVar_t *var = NULL;

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

        if (var->name != NULL)
        {
            free(var->name);
            var->name = NULL;
        }

        if (var->value != NULL)
        {
            free(var->value);
            var->value = NULL;
        }

        size_t name_len = strlen(name);
        var->name = (char *)malloc(name_len + 1);

        if (var->name == NULL)
        {
            printf("core3_opsys_getvar out of memory\n");
            return NULL;
        }

        memset((void *)var->name, 0, name_len);
        sprintf(var->name, "%s", name);

        var->value = (osCmdValue_t *)malloc(sizeof(osCmdValue_t));
        core3_val_set_float(var->value, 0);
    }

    return var;
}

void core3_opsys_setvar(const char *name, osCmdValue_t *val)
{
    osVar_t *var = core3_opsys_getvar(name);

    printf("[setvar] '%s'\n", name);

    var->value->Float = val->Float;
    var->value->Int = val->Int;
    var->value->Type = val->Type;
    var->value->PtrLen = val->PtrLen;

    if (val->Ptr != NULL && val->PtrLen > 0)
    {
        var->value->Ptr = malloc(val->PtrLen);

        if (var->value->Ptr == NULL)
        {
            printf("core3_opsys_setvar out of memory\n");
            return;
        }

        memcpy(var->value->Ptr, val->Ptr, val->PtrLen);
    }
}

size_t core3_value_count()
{
    size_t sz = 0;

    for (size_t i = 0; i < sizeof(os_values) / sizeof(*os_values); i++)
    {
        if (os_values[i].Type != VALUE_TYPE_EMPTY)
        {
            sz++;
        }
    }

    return sz;
}

size_t core3_variable_count()

{
    return os_var_idx;
}

osCmdValue_t *core3_value_alloc()
{
    osCmdValue_t *ret = NULL;

    for (size_t i = 0; i < sizeof(os_values) / sizeof(*os_values); i++)
    {
        osCmdValue_t *sel = &(os_values[i]);

        if (sel->Type == VALUE_TYPE_EMPTY)
        {
            sel->RefCount = 0;
            sel->Type = VALUE_TYPE_ALLOCATED;
            ret = sel;
            break;
        }
    }

    return ret;
}

osCmdValue_t *core3_value_ref(osCmdValue_t *val)
{
    if (val == NULL)
        return NULL;

    val->RefCount++;
    return val;
}

osCmdValue_t *core3_value_copy(osCmdValue_t *src)
{
    if (src == NULL)
        return NULL;

    // return core3_value_ref(src);

    osCmdValue_t *dst = core3_value_alloc();

    dst->Float = src->Float;
    dst->Int = src->Int;
    dst->Ptr = src->Ptr;
    dst->PtrLen = src->PtrLen;
    dst->Type = src->Type;

    if (dst->Ptr != NULL && dst->PtrLen > 0)
    {
        uint8_t *new_mem = (uint8_t *)malloc(dst->PtrLen);

        if (new_mem == NULL)
        {
            printf("core3_value_copy out of memory\n");
            return NULL;
        }

        memcpy(new_mem, dst->Ptr, dst->PtrLen);
        dst->Ptr = new_mem;
    }

    return dst;
}

void core3_value_free(osCmdValue_t *val)
{
    if (val == NULL)
        return;

    val->RefCount--;
    if (val->RefCount > 0)
        return;

    for (size_t i = 0; i < sizeof(os_vars) / sizeof(*os_vars); i++)
    {
        if (os_vars[i].value == val)
            return;
    }

    if (val->PtrLen > 0 && val->Ptr != NULL)
    {
        free(val->Ptr);
        val->Ptr = NULL;
        val->PtrLen = 0;
    }

    val->Float = 0;
    val->Int = 0;
    val->RefCount = 0;
    val->Type = VALUE_TYPE_EMPTY;
}

void core3_value_perform_gc()
{
    for (size_t i = 0; i < sizeof(os_values) / sizeof(*os_values); i++)
    {
        osCmdValue_t *sel = &(os_values[i]);

        if (sel->Type != VALUE_TYPE_EMPTY && sel->RefCount <= 0)
        {
            core3_value_free(sel);
        }
    }
}

int os_cmds_idx = 0;
osCmd_t os_cmds[16];

// osCmdValue_t* last_command_return;

void core3_opsys_register(const char *command, bool raw_input, int arg_count, execFunc onExec)
{
    int idx = os_cmds_idx++;

    size_t cmd_len = strlen(command) + 1;
    os_cmds[idx].command = (char *)malloc(cmd_len);

    if (os_cmds[idx].command == NULL)
    {
        printf("core3_opsys_register out of memory\n");
        return;
    }

    memset(os_cmds[idx].command, 0, cmd_len);
    sprintf(os_cmds[idx].command, "%s", command);
    os_cmds[idx].onExec = onExec;
    os_cmds[idx].arg_count = arg_count;
    os_cmds[idx].raw_input = raw_input;

    printf("Reg> %s - 0x%p\n", command, onExec);
}

osCmd_t *core3_opsys_find_command(const char *command)
{
    for (size_t i = 0; i < os_cmds_idx; i++)
    {
        if (strcmp(command, os_cmds[i].command) == 0)
            return &os_cmds[i];
    }

    return NULL;
}

void input_printf(const char *str, ...)
{
#ifndef OPSYS_SIM
    va_list args;
    va_start(args, str);
    vprintf(str, args);
    va_end(args);

    fflush(stdin);
#else
#endif
}

size_t getLineInput(char buf[], size_t len)
{
    memset(buf, 0, len);

    fflush(stdout);
    fflush(stdin);
    fpurge(stdin); // clears any junk in stdin
    size_t write_len = 0;

    char *bufp = buf;
    while (true)
    {
#ifndef OPSYS_SIM
        vTaskDelay(pdMS_TO_TICKS(10));
#endif

        *bufp = getchar();
        if (*bufp != '\0' && *bufp != 0xFF && *bufp != '\r') // ignores null input, 0xFF, CR in CRLF
        {
            //'enter' (EOL) handler
            if (*bufp == '\n')
            {
                input_printf("\n");
                *bufp = '\0';

#ifndef OPSYS_SIM
                getchar();
#endif
                break;
            } // backspace handler
            else if (*bufp == '\b')
            {
                if (bufp - buf >= 1)
                {
                    input_printf("\b \b");
                    bufp--;
                    write_len--;
                }
            }
            else
            {
                input_printf("%c", *bufp);
                // pointer to next character
                bufp++;
                write_len++;
            }
        }

        // only accept len-1 characters, (len) character being null terminator.
        if ((size_t)(bufp - buf) > (len)-2)
        {
            bufp = buf + (len - 1);
            *bufp = '\0';
            break;
        }
    }

    return write_len;
}

void tokenize(char *cmd, int len, int *tokens, int tokens_len, int *cur_tok)
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
                    tokens[*cur_tok] = (int)(i + 1);
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
                tokens[*cur_tok] = (int)i;
                *cur_tok = *cur_tok + 1;
            }
        }
    }
}

osCmdValue_t *perform_command(char *cmd, size_t len)
{
    char cmd_orig[128];
    memset(cmd_orig, 0, sizeof(cmd_orig));
    sprintf(cmd_orig, "%s", cmd);

    char cmd_orig2[128];
    memset(cmd_orig2, 0, sizeof(cmd_orig2));
    sprintf(cmd_orig2, "%s", cmd);

    core3_value_perform_gc();

    if (strlen(cmd) == 0)
        return NULL;

    bool whitespace_only = true;
    for (size_t i = 0; i < len; i++)
    {
        if (cmd[i] != ' ')
        {
            whitespace_only = false;
            break;
        }
    }

    if (whitespace_only)
        return NULL;

    // Comment
    if (cmd[0] == '#')
        return NULL;

    if (cmd[0] == ';')
        return NULL;

    // Tokenizing
    int cur_tok = 0;
    int tokens[32];
    tokenize(cmd, len, tokens, sizeof(tokens) / sizeof(*tokens), &cur_tok);

    if (cur_tok == 0)
        return NULL;

    int arg_counter = 0;
    osCmdValue_t arg_stack[8];

    if (cur_tok == 0)
        return NULL;

    char *str_cmd = &cmd[tokens[0]];
    osCmd_t *osCmd = core3_opsys_find_command(str_cmd);
    osCmdValue_t *last_command_return = NULL;

    if (osCmd == NULL)
    {
        if (strspn(str_cmd, "-0123456789.") == strlen(str_cmd))
        {

            osCmdValue_t *val = core3_value_alloc();
            core3_val_set_float(val, strtof(str_cmd, NULL));

            // printf("Is number '%f'\n", val->Float);
            last_command_return = val;
            return val;
        }

        if (str_cmd[0] == '$')
        {
            osCmdValue_t *val = core3_value_alloc();
            core3_val_set_string(val, (&cmd_orig[tokens[0]]) + 1);

            last_command_return = val;
            return val;
        }

        if (str_cmd[0] == '@')
        {
            osVar_t *var = core3_opsys_getvar(str_cmd + 1);
            osCmdValue_t *val = core3_value_copy(var->value);

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
                for (size_t i = 1; i < cur_tok; i++)
                {
                    char *cmd2 = &cmd[tokens[i]];
                    osCmdValue_t *arg_ret = perform_command(cmd2, strlen(cmd2));

                    if (arg_ret == NULL)
                        return NULL;

                    arg_stack[arg_counter++] = *arg_ret;
                }
            }

            last_command_return = osCmd->onExec(cmd, cmd_orig2, tokens, &arg_stack[0], arg_counter);
            arg_counter = 0;
        }
    }

    return last_command_return;
}

osCmdValue_t *core3_cmd_stats(char *cmd, char *cmdorig, int *tokens, osCmdValue_t *args, int arg_count)
{
    printf("] Vals used: %d\n", (int)core3_value_count());
    printf("] Vars used: %d\n", (int)core3_variable_count());
    return NULL;
}

osCmdValue_t *core3_cmd_print(char *cmd, char *cmdorig, int *tokens, osCmdValue_t *args, int arg_count)
{
    for (size_t i = 0; i < arg_count; i++)
    {
        switch (args[i].Type)
        {
        case VALUE_TYPE_EMPTY:
            break;

        case VALUE_TYPE_FLOAT:
            printf("%f", args[i].Float);
            break;

        case VALUE_TYPE_INT:
            printf("%d", args[i].Int);
            break;

        case VALUE_TYPE_STRING:
            if (args[i].Ptr != NULL && args[i].PtrLen > 0)
            {
                const char *pstr = (const char *)args[i].Ptr;

                if (strncmp(pstr, "\\n", args[i].PtrLen) == 0)
                {
                    printf("\n");
                }
                else
                {
                    printf("%s", pstr);
                }
            }
            else
                printf("ERR_STRING\n");
            break;

        case VALUE_TYPE_PROGRAM:
        {
            osProgram_t *prog = (osProgram_t *)args[i].Ptr;
            printf("program Prog\n");

            if (prog == NULL)
            {
                printf("# null\n");
            }
            else
            {
                printf("clearprogram\n");

                for (size_t i = 0; i < prog->lines_used; i++)
                {
                    printf("l %d %s\n", (int)i, prog->lines[i]);
                }
            }
        }
        break;

        default:
            printf("print - Unknown variable type %d\n", args[i].Type);
            break;
        }
    }

    printf("\n");
    return NULL;
}

osCmdValue_t *core3_cmd_rnd(char *cmd, char *cmdorig, int *tokens, osCmdValue_t *args, int arg_count)
{
    osCmdValue_t *val = core3_value_alloc();
    core3_val_set_int(val, esp_random());
    return val;
}

osCmdValue_t *core3_cmd_rndf(char *cmd, char *cmdorig, int *tokens, osCmdValue_t *args, int arg_count)
{
    osCmdValue_t *val = core3_value_alloc();
    core3_val_set_float(val, (float)esp_random() / UINT32_MAX);
    return val;
}

osCmdValue_t *core3_cmd_yield(char *cmd, char *cmdorig, int *tokens, osCmdValue_t *args, int arg_count)
{
    if (args[0].Type != VALUE_TYPE_FLOAT)
        return NULL;

    int ms = (int)args[0].Float;
    vTaskDelay(pdMS_TO_TICKS(ms));
    return NULL;
}

osCmdValue_t *core3_cmd_add(char *cmd, char *cmdorig, int *tokens, osCmdValue_t *args, int arg_count)
{
    float sum = 0;

    for (size_t i = 0; i < arg_count; i++)
    {
        sum = sum + args[i].Float;
        args[i].RefCount--;
    }

    osCmdValue_t *ret = core3_value_alloc();
    core3_val_set_float(ret, sum);
    return ret;
}

osCmdValue_t *exec_raw_line(char *program_line)
{
    char *progline2 = core3_string_copy(program_line);
    osCmdValue_t *ret = perform_command(progline2, strlen(progline2));
    free(progline2);
    return ret;
}

osCmdValue_t *exec_raw(osProgram_t *prog)
{
    prog->program_counter = 0;
    osCmdValue_t *ret = NULL;

    while (prog->program_counter < prog->lines_used)
    {
        if (prog->program_counter < 0 || prog->program_counter > prog->lines_count)
        {
            printf("[Exception] Program counter out of bounds\n");
            return NULL;
        }

        char *program_line = prog->lines[prog->program_counter];
        prog->program_counter = prog->program_counter + 1;
        ret = exec_raw_line(program_line);
    }

    return core3_value_ref(ret);
}

osCmdValue_t *core3_cmd_exec(char *cmd, char *cmdorig, int *tokens, osCmdValue_t *args, int arg_count)
{
    osCmdValue_t *ret = NULL;
    char *arg1 = &cmd[tokens[1]];

    osVar_t *prog_var = core3_opsys_getvar((const char *)arg1);
    if (prog_var->value->Type == VALUE_TYPE_PROGRAM && prog_var->value->Ptr != NULL)
    {
        ret = exec_raw((osProgram_t *)prog_var->value->Ptr);
    }
    else
    {
        printf("Variable '%s' not a program\n", arg1);
    }

    return ret;
}

osCmdValue_t *core3_cmd_compare(char *cmd, char *cmdorig, int *tokens, osCmdValue_t *args, int arg_count)
{
    char *var_name1 = (&cmd[tokens[1]]);
    char *var_name2 = (&cmd[tokens[2]]);

    osVar_t *var1 = core3_opsys_getvar((const char *)var_name1);
    osVar_t *var2 = core3_opsys_getvar((const char *)var_name2);

    if (var1->value == NULL || var2->value == NULL)
    {
        return NULL;
    }

    osVar_t *equal = core3_opsys_getvar("equal");
    osVar_t *bigger = core3_opsys_getvar("bigger");
    osVar_t *smaller = core3_opsys_getvar("smaller");

    if (var1->value->Type == var2->value->Type)
    {
        switch (var1->value->Type)
        {
        case VALUE_TYPE_FLOAT:
        {
            float v1 = var1->value->Float;
            float v2 = var2->value->Float;

            equal->value = core3_value_alloc();
            core3_val_set_float(equal->value, v1 == v2 ? 1.0f : 0.0f);

            bigger->value = core3_value_alloc();
            core3_val_set_float(bigger->value, v1 > v2 ? 1.0f : 0.0f);

            smaller->value = core3_value_alloc();
            core3_val_set_float(smaller->value, v1 < v2 ? 1.0f : 0.0f);
        }
        break;

        case VALUE_TYPE_INT:
        {
            int v1 = var1->value->Int;
            int v2 = var2->value->Int;

            equal->value = core3_value_alloc();
            core3_val_set_float(equal->value, v1 == v2 ? 1.0f : 0.0f);

            bigger->value = core3_value_alloc();
            core3_val_set_float(bigger->value, v1 > v2 ? 1.0f : 0.0f);

            smaller->value = core3_value_alloc();
            core3_val_set_float(smaller->value, v1 < v2 ? 1.0f : 0.0f);
        }
        break;

        case VALUE_TYPE_STRING:
        {
            size_t v1len = var1->value->PtrLen + 1;
            char *v1 = (char *)malloc(v1len);

            if (v1 == NULL)
            {
                printf("core3_cmd_compare out of memory\n");
                break;
            }

            memset(v1, 0, v1len);
            memcpy(v1, var1->value->Ptr, v1len - 1);

            size_t v2len = var2->value->PtrLen + 1;
            char *v2 = (char *)malloc(v2len);

            if (v2 == NULL)
            {
                printf("core3_cmd_compare out of memory\n");
                break;
            }

            memset(v2, 0, v2len);
            memcpy(v2, var2->value->Ptr, v2len - 1);

            int strcmp_res = strcmp(v1, v2);

            free(v1);
            free(v2);

            equal->value = core3_value_alloc();
            core3_val_set_float(equal->value, (strcmp_res == 0) ? 1.0f : 0.0f);

            bigger->value = core3_value_alloc();
            core3_val_set_float(bigger->value, (strcmp_res > 0) ? 1.0f : 0.0f);

            smaller->value = core3_value_alloc();
            core3_val_set_float(smaller->value, (strcmp_res < 0) ? 1.0f : 0.0f);
        }
        break;

        default:
        {
            void *v1 = var1->value->Ptr;
            void *v2 = var2->value->Ptr;

            bool eq = v1 == v2 || var1->value == var2->value;
            bool big = var1->value->PtrLen > var2->value->PtrLen;
            bool small = var1->value->PtrLen < var2->value->PtrLen;

            equal->value = core3_value_alloc();
            core3_val_set_float(equal->value, eq ? 1.0f : 0.0f);

            bigger->value = core3_value_alloc();
            core3_val_set_float(bigger->value, big ? 1.0f : 0.0f);

            smaller->value = core3_value_alloc();
            core3_val_set_float(smaller->value, small ? 1.0f : 0.0f);
        }
        break;
        }
    }

    return NULL;
}

osCmdValue_t *core3_cmd_if(char *cmd, char *cmdorig, int *tokens, osCmdValue_t *args, int arg_count)
{
    char *var_name = (&cmd[tokens[1]]);
    // char* code_if = (&cmdorig[tokens[2]]);
    char *code_if = (&cmd[tokens[2]]);
    char *code_else = (&cmd[tokens[3]]);

    osVar_t *var1 = core3_opsys_getvar((const char *)var_name);
    osCmdValue_t *ret = NULL;

    if (var1 != NULL && var1->value != NULL)
    {
        if (var1->value->Type == VALUE_TYPE_FLOAT)
        {
            if (var1->value->Float > 0)
            {
                ret = exec_raw_line(code_if);
            }
            else
            {
                ret = exec_raw_line(code_else);
            }
        }
    }

    return ret;
}

osCmdValue_t *core3_cmd_jump(char *cmd, char *cmdorig, int *tokens, osCmdValue_t *args, int arg_count)
{
    if (current_prog_var == NULL || current_prog_var->value == NULL)
        return NULL;

    if (current_prog_var->value->Type != VALUE_TYPE_PROGRAM || current_prog_var->value->Ptr == NULL)
        return NULL;

    osProgram_t *current_prog = (osProgram_t *)current_prog_var->value->Ptr;

    if (args[0].Type != VALUE_TYPE_FLOAT)
    {
        return NULL;
    }

    int new_pc = (int)args[0].Float;
    current_prog->program_counter = new_pc;

    return NULL;
}

osCmdValue_t *core3_cmd_program(char *cmd, char *cmdorig, int *tokens, osCmdValue_t *args, int arg_count)
{
    osCmdValue_t *ret = NULL;
    char *arg1 = &cmd[tokens[1]];
    // size_t arg1_len = strlen(arg1);

    // printf("Variable '%s'\n", (const char *)args[0].Ptr);

    osVar_t *var = core3_opsys_getvar((const char *)arg1);

    if (var->value->Type == VALUE_TYPE_PROGRAM)
    {
        ret = core3_value_copy(var->value);
    }
    else
    {
        current_prog_var = var;
        ret = core3_value_alloc();

        osProgram_t *prog = core3_program_alloc(64);
        core3_val_set_program(ret, prog);

        core3_value_free(var->value);
        var->value = core3_value_copy(ret);
    }

    return ret;
}

osCmdValue_t *core3_cmd_clearprogram(char *cmd, char *cmdorig, int *tokens, osCmdValue_t *args, int arg_count)
{
    if (current_prog_var == NULL || current_prog_var->value == NULL)
        return NULL;

    if (current_prog_var->value->Type != VALUE_TYPE_PROGRAM || current_prog_var->value->Ptr == NULL)
        return NULL;

    osProgram_t *current_prog = (osProgram_t *)current_prog_var->value->Ptr;

    for (size_t i = 0; i < current_prog->lines_used; i++)
    {
        free(current_prog->lines[i]);
        current_prog->lines[i] = NULL;
    }

    current_prog->lines_used = 0;
    return NULL;
}

osCmdValue_t *core3_cmd_progline(char *cmd, char *cmdorig, int *tokens, osCmdValue_t *args, int arg_count)
{
    if (current_prog_var == NULL || current_prog_var->value == NULL)
        return NULL;

    if (current_prog_var->value->Type != VALUE_TYPE_PROGRAM || current_prog_var->value->Ptr == NULL)
        return NULL;

    osProgram_t *current_prog = (osProgram_t *)current_prog_var->value->Ptr;

    int line_no = atoi(&cmdorig[tokens[1]]);
    char *line = (&cmdorig[tokens[2]]);

    if (line[0] == '$')
        line++;

    // printf("Line %d - '%s'\n", line_no, line);

    int idx = line_no;

    if (line_no >= current_prog->lines_used)
    {
        idx = current_prog->lines_used++;
    }

    size_t len = strlen(line) + 1;

    if (current_prog->lines[idx] != NULL)
    {
        free(current_prog->lines[idx]);
        current_prog->lines[idx] = NULL;
    }

    current_prog->lines[idx] = (char *)malloc(len);

    if (current_prog->lines[idx] == NULL)
    {
        printf("core3_cmd_progline out of memory\n");
        return NULL;
    }

    memset(current_prog->lines[idx], 0, len);
    sprintf(current_prog->lines[idx], "%s", line);

    return NULL;
}

osCmdValue_t *core3_cmd_get(char *cmd, char *cmdorig, int *tokens, osCmdValue_t *args, int arg_count)
{
    osCmdValue_t *ret = NULL;
    char *arg1 = (&cmd[tokens[1]]);

    // printf("Variable '%s'\n", (const char*)arg1);

    osVar_t *var = core3_opsys_getvar((const char *)arg1);
    ret = core3_value_copy(var->value);

    return ret;
}

osCmdValue_t *core3_cmd_set(char *cmd, char *cmdorig, int *tokens, osCmdValue_t *args, int arg_count)
{
    char *arg1 = (&cmd[tokens[1]]);
    char *arg2 = (&cmd[tokens[2]]);

    osVar_t *var = core3_opsys_getvar((const char *)arg1);
    osCmdValue_t *arg2_val = perform_command(arg2, strlen(arg2));

    if (arg2_val != NULL)
    {
        core3_value_free(var->value);
        var->value = core3_value_copy(arg2_val);
    }

    return NULL;
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

    core3_opsys_register("stats", false, 0, core3_cmd_stats);
    core3_opsys_register("print", false, 1, core3_cmd_print);
    core3_opsys_register("add", false, 2, core3_cmd_add);
    core3_opsys_register("yield", false, 1, core3_cmd_yield);
    core3_opsys_register("get", true, 1, core3_cmd_get);
    core3_opsys_register("set", true, 2, core3_cmd_set);
    core3_opsys_register("rnd", false, 0, core3_cmd_rnd);
    core3_opsys_register("rndf", false, 0, core3_cmd_rnd);
    core3_opsys_register("program", true, 1, core3_cmd_program);
    core3_opsys_register("clearprogram", false, 0, core3_cmd_clearprogram);
    core3_opsys_register("l", true, 1, core3_cmd_progline);
    core3_opsys_register("exec", true, 1, core3_cmd_exec);
    core3_opsys_register("compare", true, 2, core3_cmd_compare);
    core3_opsys_register("if", true, 3, core3_cmd_if);
    core3_opsys_register("jump", false, 1, core3_cmd_jump);

    char line_mem[128] = {0};
    size_t line_size = 0;

    while (true)
    {
        memset(prompt, 0, sizeof(prompt));
        sprintf(prompt, "%s@%s %s# ", user_name, station_name, cur_path);
        printf("%s", prompt);

        line_size = getLineInput(line_mem, sizeof(line_mem));

        osCmdValue_t *val = perform_command(line_mem, line_size);
        if (val != NULL)
        {
            core3_cmd_print(NULL, NULL, NULL, val, 1);
        }

        core3_value_free(val);
        fflush(stdout);
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}