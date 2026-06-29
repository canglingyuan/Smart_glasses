#ifndef __CMD_PARSER_H
#define __CMD_PARSER_H

// 解析 OpenMV 发来的指令并执行对应播报
void CMD_ParseAndExecute(const char *cmd);

#endif