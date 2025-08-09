#ifndef _SHELL_H
#define _SHELL_H

void shell_loop();
void shell_filesystem_commands(const char* cmd, const char* arg1, const char* arg2, int args);

#endif