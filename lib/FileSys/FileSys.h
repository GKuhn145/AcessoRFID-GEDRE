#ifndef FILESYS_H
#define FILESYS_H
#include "control.h"
#include "Secrets.h"

#include <LittleFS.h>

// Inicializa o sistema de arquivos LittleFS
void FileSysInit();

void setDateTime();
#endif