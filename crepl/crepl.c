#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include "dlfcn.h"

int exec(char *cmd)
{
  if(fork() == 0) {
    execl("/bin/sh", "sh", "-c", cmd, NULL);
    exit(1);
  } else {
    int status;
    if(wait(&status) == -1) {
      return -1;
    }
    return status;
  }
}

void* add_function(char *code) {
  char code_file[]  = "/tmp/crepl.XXXXXX.c";
  int res = mkstemps(code_file, 2);
  if(res == -1) {
    return NULL;
  }
  FILE *fp = fdopen(res, "w");
  if(fp == NULL) {
    return NULL;
  }

  fprintf(fp, "%s", code);
  fclose(fp);

  char so_file[] = "/tmp/crepl.XXXXXX.so";
  char cmd[100];
  strncpy(so_file + 11, code_file + 11, 6);
  sprintf(cmd, "gcc -shared -fPIC %s -o %s", code_file, so_file);
  if(exec(cmd) != 0) {
    return NULL;
  }

  void *handle = dlopen(so_file, RTLD_NOW | RTLD_GLOBAL);
  if(handle == NULL) {
    return NULL;
  }

  return handle;
}

int eval(char *code, int *result) {
  static char buf[8192], func[100];
  static int counter = 0;
  sprintf(func, "__expr_wrapper_%d", counter++);
  sprintf(buf, "int %s() { return %s; }", func, code);

  void *handle = add_function(buf);
  if(handle == NULL) {
    return -1;
  }

  int (*func_ptr)() = dlsym(handle, func);
  if(func_ptr == NULL) {
    return -1;
  }

  *result = func_ptr();
  return 0;
}

int main(int argc, char *argv[]) {
  static char line[4096];
  while (1) {
    printf("crepl> ");
    fflush(stdout);
    if (!fgets(line, sizeof(line), stdin)) {
      break;
    }
    printf("Got %zu chars.\n", strlen(line)); // ??

    char begin[5];
    strncpy(begin, line, 4); begin[4] = '\0';
    if (strcmp(begin, "int ") == 0) { // is a function
      printf("[Function]\n");

      if(add_function(line) != NULL) {
        printf("Added: %s\n", line);
      } else {
        printf("Error\n");
      }
    } else {
      printf("[Expression]\n");

      int result = -1;
      if(eval(line, &result) == 0) {
        printf("(%s) == %d\n", line, result);
      } else {
        printf("Error\n");
      }
    }
  }
}
