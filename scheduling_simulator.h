#ifndef SCHEDULING_SIMULATOR_H
#define SCHEDULING_SIMULATOR_H

#include <stdio.h>
#include <stdlib.h>
#include <ucontext.h>
#include <time.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <unistd.h>

#include "task.h"

enum TASK_STATE {
	TASK_RUNNING,   //0
	TASK_READY,     //1
	TASK_WAITING,   //2
	TASK_TERMINATED //3
};

typedef struct NODE { //create task
	int pid;
	int state;  //run or ready or wait or terminate
	char SorL;
	char name[10];  //task name
	struct NODE *prev;
	struct NODE *next;
	time_t timer1, timer2, total_time; //time stay in ready state
	ucontext_t context;
} NODE;

void hw_suspend(int msec_10);
void signal_handler(int sig);
void hw_wakeup_pid(int pid);
int hw_wakeup_taskname(char *task_name);
int hw_task_create(char *task_name);

#endif
