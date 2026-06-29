#include "cmd_parser.h"
#include "voice_prompts.h"
#include "syn6288.h"
#include <string.h>
#include <stdlib.h>

void CMD_ParseAndExecute(const char *cmd)
{
    if (strcmp(cmd, "RED") == 0)
    {
        SYN6288_Speak("[v16] Red light ahead, please wait");
    }
    else if (strcmp(cmd, "GREEN") == 0)
    {
        SYN6288_Speak("[v14] Green light ahead, you may go");
    }
    else if (strcmp(cmd, "ZEBRA") == 0)
    {
        SYN6288_Speak("[v14] Zebra crossing ahead");
    }
    else if (strcmp(cmd, "OBSTACLE") == 0)
    {
        SYN6288_Speak("[v16] Obstacle ahead, please go around");
    }
    else if (strncmp(cmd, "PRICE:", 6) == 0)
    {
        int price = atoi(cmd + 6);
        Voice_Speak_Price(price);
    }
    // ... 其他指令
}