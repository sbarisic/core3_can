#include <core3.h>
#include <core3_opsys.h>

#include <time.h>
#include <stdlib.h>
#include "esp_random.h"

char user_name[32];
char station_name[32];
char cur_path[32];

typedef enum
{
    VALUE_TYPE_FLOAT,
    VALUE_TYPE_INT
} osCmdValueType_t;

typedef struct
{
    void *Ptr;
    float Float;
    int Int;

    osCmdValueType_t Type;
} osCmdValue_t;

typedef osCmdValue_t *(*execFunc)(char *cmd, osCmdValue_t *args, int arg_count);

typedef struct
{
    const char *command;
    const char *desc;
    int arg_count;
    execFunc onExec;
} osCmd_t;

void core3_val_set_float(osCmdValue_t *cmdval, float val)
{
    cmdval->Int = 0;
    cmdval->Ptr = NULL;
    cmdval->Float = val;
    cmdval->Type = VALUE_TYPE_FLOAT;
}

void core3_val_set_int(osCmdValue_t *cmdval, int val)
{
    cmdval->Float = 0;
    cmdval->Ptr = NULL;
    cmdval->Int = val;
    cmdval->Type = VALUE_TYPE_INT;
}

int os_stack_idx = 0;
osCmdValue_t os_stack[32];

osCmdValue_t *core3_opsys_stack_push()
{
    if (os_stack_idx >= 32)
        return NULL;

    osCmdValue_t *ret = &os_stack[os_stack_idx++];
    memset(ret, 0, sizeof(osCmdValue_t));
    return ret;
}

osCmdValue_t *core3_opsys_stack_push_copy(osCmdValue_t *copy)
{
    osCmdValue_t *new_val = core3_opsys_stack_push();
    *new_val = *copy;
    return new_val;
}

osCmdValue_t *core3_opsys_stack_peek(int backIdx)
{
    return &os_stack[os_stack_idx - backIdx - 1];
}

osCmdValue_t *core3_opsys_stack_pop()
{
    os_stack_idx--;
    return &os_stack[os_stack_idx];

    if (os_stack_idx < 0)
        os_stack_idx = 0;
}

void core3_opsys_stack_clear()
{
    os_stack_idx = 0;
}

int os_cmds_idx = 0;
osCmd_t os_cmds[16];

osCmdValue_t *last_command_return;

void core3_opsys_register(const char *command, int arg_count, execFunc onExec)
{
    int idx = os_cmds_idx++;
    os_cmds[idx].command = command;
    os_cmds[idx].onExec = onExec;
    os_cmds[idx].arg_count = arg_count;

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

void tokenize(char *cmd, int len, int *tokens, int *cur_tok)
{
    bool last_was_null = true;
    bool parsing_quote = false;
    int bracket_cnt = 0;

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
        else if (cmd[i] == ' ' || cmd[i] == '\t')
        {
            if (!parsing_quote && bracket_cnt == 0)
            {
                cmd[i] = 0;
                last_was_null = true;
            }
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

osCmdValue_t *perform_command(char *cmd, int len)
{
    char cmd_orig[128];
    memset(cmd_orig, 0, sizeof(cmd_orig));
    sprintf(cmd_orig, "%s", cmd);

    // Tokenizing
    int cur_tok = 0;
    int tokens[32];
    tokenize(cmd, len, tokens, &cur_tok);

    /*for (size_t i = 0; i < cur_tok; i++)
    {
        printf("%d - '%s'\n", i, &cmd[tokens[i]]);
    }
    return NULL;*/

    int arg_counter = 0;
    osCmdValue_t arg_stack[8];

    if (cur_tok == 0)
        return NULL;

    char *str_cmd = &cmd[tokens[0]];
    osCmd_t *osCmd = core3_opsys_find_command(str_cmd);

    last_command_return = NULL;

    if (osCmd == NULL)
    {
        if (strspn(str_cmd, "-0123456789.") == strlen(str_cmd))
        {
            osCmdValue_t *val = core3_opsys_stack_push();
            core3_val_set_float(val, strtof(str_cmd, NULL));
            last_command_return = val;
            return val;
        }

        printf("Command not found '%s'\n", str_cmd);
    }
    else
    {
        if ((cur_tok - 1) < osCmd->arg_count)
        {
            printf("%s - Invalid number of args, expected %d, got %d\n", str_cmd, osCmd->arg_count, cur_tok - 1);
            return NULL;
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

            last_command_return = osCmd->onExec(cmd, &arg_stack[0], arg_counter);
            if (last_command_return != NULL)
                core3_opsys_stack_pop();

            for (size_t i = 0; i < arg_counter; i++)
            {
                core3_opsys_stack_pop();
            }

            if (last_command_return != NULL)
                core3_opsys_stack_push_copy(last_command_return);

            // print_last_command_ret();
        }
    }

    return last_command_return;
}

osCmdValue_t *core3_cmd_print(char *cmd, osCmdValue_t *args, int arg_count)
{
    for (size_t i = 0; i < arg_count; i++)
    {
        switch (args[i].Type)
        {
        case VALUE_TYPE_FLOAT:
            printf("%f\n", args[i].Float);
            break;

        case VALUE_TYPE_INT:
            printf("%d\n", args[i].Int);
            break;

        default:
            break;
        }
    }

    return NULL;
}

osCmdValue_t *core3_cmd_rnd(char *cmd, osCmdValue_t *args, int arg_count)
{
    osCmdValue_t *val = core3_opsys_stack_push();
    core3_val_set_int(val, esp_random());
    return val;
}

osCmdValue_t *core3_cmd_rndf(char *cmd, osCmdValue_t *args, int arg_count)
{
    osCmdValue_t *val = core3_opsys_stack_push();
    core3_val_set_float(val, (float)esp_random() / UINT32_MAX);
    return val;
}

osCmdValue_t *core3_cmd_add(char *cmd, osCmdValue_t *args, int arg_count)
{
    float sum = 0;

    for (size_t i = 0; i < arg_count; i++)
    {
        sum = sum + args[i].Float;
    }

    osCmdValue_t *ret = core3_opsys_stack_push();
    core3_val_set_float(ret, sum);
    return ret;
}

osCmdValue_t *core3_cmd_get(char *cmd, osCmdValue_t *args, int arg_count)
{

    osCmdValue_t *ret = core3_opsys_stack_push();
    core3_val_set_int(ret, 42);
    return ret;
}

void core3_opsys_init()
{
    srand(time(NULL));
    char prompt[128];

    memset(user_name, 0, sizeof(user_name));
    memset(station_name, 0, sizeof(station_name));
    memset(cur_path, 0, sizeof(cur_path));
    sprintf(user_name, "user");
    sprintf(station_name, "core3");
    sprintf(cur_path, "/");

    core3_opsys_register("print", 1, core3_cmd_print);
    core3_opsys_register("add", 2, core3_cmd_add);
    core3_opsys_register("get", 0, core3_cmd_get);
    core3_opsys_register("rnd", 0, core3_cmd_rnd);
    core3_opsys_register("rndf", 0, core3_cmd_rnd);

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
            core3_cmd_print(NULL, val, 1);
        }

        core3_opsys_stack_clear();

        // dprintf("Stack counter: %d\n", os_stack_idx);

        fflush(stdout);

        vTaskDelay(pdMS_TO_TICKS(10));
    }
}