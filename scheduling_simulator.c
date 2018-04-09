#include "scheduling_simulator.h"

unsigned int count_quantum=0;
struct NODE *current=NULL, *node=NULL, *now=NULL;
struct NODE *waiting=NULL, *terminate=NULL,*temp=NULL;
ucontext_t uc_context;  //uc_context:handle event when task end
int control=0;   //0:not finish 1:finish
int k=0, suspend_handle[300][2]= {0};
/*current:point to node in ready queue,node:link every task
now:point to the task executing, waiting:handle task in waiting queue
terminate:handle task who is finish*/

void hw_suspend(int msec_10)    //put task into waiting queue
{
	//send signal every 0.01 seconds
	//printf("In suspend ,task pid %d\n",now->pid);
	suspend_handle[k][0]=msec_10;
	suspend_handle[k][1]=now->pid;
	//printf("suspend[%d][0] %d| suspend[k][1] %d\n",k,suspend_handle[k][0],
	//     suspend_handle[k][1]);
	k++;
	//put task from running to waiting & select a task to run
	if(waiting==NULL) {
		now->state=2;   //waiting state
		waiting=now;
		waiting->prev=NULL;
		waiting->next=NULL;
	} else {
		now->state=2;
		waiting->next=now;
		waiting->next->prev=waiting;
		waiting=waiting->next;
		waiting->next=NULL;
	}
	now=NULL;

	return;
}

int i=0,flag=0; //to determine whether to put into ready queue or not
struct itimerval zeroval;
void signal_handler(int sig)
{
	//puts("Receive Signal");
	switch(sig) {
	case SIGALRM:
		//count the rest time of task stay in waiting queue
		for(i=0; i<k; i++) {
			if(suspend_handle[i][0] > 0) { //to prevent overflow
				suspend_handle[i][0]--;
				//printf("suspend_handle[%d][0] is %d\n",i,suspend_handle[i][0]);
			}
			//sleep time's up,wake up
			if(suspend_handle[i][0] == 0) {
				hw_wakeup_pid(suspend_handle[i][1]);
			}
		}

		if(now!=NULL) {
			//printf("Runnning pid is %d\n",now->pid);
			count_quantum++;    //put task from running to ready
			if(now->SorL=='s' || now->SorL=='S') {
				if(count_quantum==1) {
					count_quantum=0;
					flag=1;
				}
			} else if(now->SorL=='l' || now->SorL=='L') {
				if(count_quantum==2) {
					count_quantum=0;
					flag=1;
				} else
					flag=0;
			}

			if(flag==1) {   //task from running to ready
				flag=0;
				printf("SIGALRM pid %d\n",now->pid);
				if(current!=NULL) {
					while(current->next!=NULL) {
						current=current->next;
					}
					now->state=1;   //put now into the end of ready queue
					now->timer1=time(NULL);
					current->next=now;
					current->next->prev=current;
					current->next->next=NULL;
					while(current->prev!=NULL)  //current point to first node
						current=current->prev;

					current->state=0;//execute first node
					current->timer2=time(NULL);
					current->total_time+=(current->timer2-current->timer1);
					now=current;
					if(current->next!=NULL) {
						current=current->next;
						current->prev=NULL;
					}
					while(current->next!=NULL)//point to last node
						current=current->next;

					//more than one task,context switch
					swapcontext(&current->context,&now->context);
				} else {    //only one task
					swapcontext(&now->context,&now->context);
				}
			}
		} else if(now==NULL && current!=NULL) {
			while(current->prev!=NULL)
				current=current->prev;
			current->state=0;
			current->timer2=time(NULL);
			current->total_time+=(current->timer2-current->timer1);
			now=current;
			if(current->next!=NULL) {
				current=current->next;
				current->prev=NULL;
			} else {
				current=NULL;
			}

			setcontext(&now->context);
		}

		//puts("Catch signal -- sigalrm");
		break;
	case SIGTSTP:
		zeroval.it_interval.tv_usec=0; //set a zero timer to stop old timer
		zeroval.it_value.tv_usec=0;
		zeroval.it_interval.tv_sec=0;
		zeroval.it_value.tv_sec=0;
		setitimer(ITIMER_REAL,&zeroval,NULL);
		control=1;
		puts("Catch signal -- ctrl+z");
		setcontext(&uc_context);    //jump to getcontext(&uc_context) in main
		break;
	}

	return;
}

void hw_wakeup_pid(int pid)   //put task in waiting queue to ready queue
{
	//printf("In function hw_wakeup_pid\n");
	if(waiting!=NULL) {
		while(waiting->prev != NULL)   //let current point to head
			waiting=waiting->prev;

		//find corresponding pid
		while(waiting->pid!=pid) {
			if(waiting->next!=NULL)
				waiting=waiting->next;
			else
				goto A;
		}
		waiting->state=1;   //ready state
		waiting->timer1=time(NULL);
		//add task into the end of ready queue
		if(current!=NULL) {
			while(current->next!=NULL)
				current=current->next;
			current->next=waiting;
			current->next->prev=current;
			current->next->next=NULL;
		} else {
			current=waiting;
			current->next=NULL;
			current->prev=NULL;
		}

		if(waiting!=NULL) {
			if(waiting->prev!=NULL) {
				if(waiting->next!=NULL) {
					waiting=waiting->prev;
					waiting->next=waiting->next->next;
					waiting->next->prev=waiting;
				} else {
					waiting=waiting->prev;
					waiting->next=NULL;
				}
			} else {
				if(waiting->next!=NULL) {
					waiting=waiting->next;
					waiting->prev=NULL;
				} else {
					waiting=NULL;
				}
			}
		}
	}

A:
	return;
}

int hw_wakeup_taskname(char *task_name)
{
	//puts("In function hw_wakeup_taskname");
	int wakeup=0;

	if(waiting!=NULL) {
		while(waiting->prev!=NULL)
			waiting=waiting->prev;
	}

	while(waiting != NULL) { //change state from waiting to ready
		if(strcmp(waiting->name,task_name)==0) {
			//printf("wake up %s pid %d\n",waiting->name,waiting->pid);
			waiting->state = 1;
			waiting->timer1=time(NULL);
			wakeup++;
			//printf("wake up %d task\n",wakeup);
			if(current != NULL) {
				while(current->next!=NULL)
					current=current->next;
				current->next=waiting;
				current->next->prev=current;
				current=current->next;
				current->next=NULL;
			} else {
				current=waiting;
				current->prev=NULL;
				current->next=NULL;
			}
			if(waiting->prev!=NULL) {
				if(waiting->next!=NULL) {
					waiting=waiting->prev;
					waiting->next=waiting->next->next;
					waiting->next->prev=waiting;
				} else {
					waiting=waiting->prev;
					waiting->next=NULL;
				}
			} else {
				if(waiting->next!=NULL) {
					waiting=waiting->next;
					waiting->prev=NULL;
				} else {
					waiting=NULL;
				}
			}
		} else {
			if(waiting->next!=NULL)
				waiting=waiting->next;
			else
				goto jump;
		}
	}

jump:
	return wakeup;
}

int count = 0;
char quantum[50];
int hw_task_create(char *task_name)
{
	//create node
	node = (NODE*)malloc(sizeof(NODE));
	node->context.uc_stack.ss_sp = malloc(1024*10);
	node->context.uc_stack.ss_size = 1024*10;
	node->context.uc_link=&uc_context;
	node->pid = count+1;
	node->state = 1;  //ready state
	if(quantum[count]=='S' || quantum[count]=='s' || quantum[count]=='L'
	   || quantum[count]=='l')
		node->SorL = quantum[count];
	else
		node->SorL = 'S';
	node->next = NULL;
	node->prev=NULL;
	node->timer1=time(NULL);
	node->total_time=0;
	sprintf(node->name,"%s",task_name);
	getcontext(&node->context);
	count++;    //count number of task

	//link node
	if(current == NULL) {  //head node
		current = node;
		current->next = NULL;
		current->prev = NULL;
	} else {
		while(current->next!=NULL)    //let current point to last node
			current=current->next;
		//link
		current->next = node;
		current->next->prev = current;
		current = current->next;
		current->next = NULL;
	}

	//create task
	if(strcmp(task_name,"task1") == 0) {
		makecontext(&node->context,task1,0);
	} else if(strcmp(task_name,"task2") == 0) {
		makecontext(&node->context,task2,0);
	} else if(strcmp(task_name,"task3") == 0) {
		makecontext(&node->context,task3,0);
	} else if(strcmp(task_name,"task4") == 0) {
		makecontext(&node->context,task4,0);
	} else if(strcmp(task_name,"task5") == 0) {
		makecontext(&node->context,task5,0);
	} else if(strcmp(task_name,"task6") == 0) {
		makecontext(&node->context,task6,0);
	} else
		return -1;

	//printf("Create: pid %d| quantum %c \n",node->pid,node->SorL);

	return node->pid; // the pid of created task name
}

int main()
{
	int value=0;
	char string[10], skip, task[50][10];
	struct itimerval t, oldvalue;
	//send signal every 0.01 seconds
	t.it_interval.tv_usec=10000; //10000 microseconds(10ms)
	t.it_value.tv_usec=10000; //10000 microseconds
	t.it_interval.tv_sec=0;
	t.it_value.tv_sec=0;

	//input
	printf("add/start/remove/ps: ");
	scanf("%s%c",&string,&skip);
	while(string[0]=='s' || string[0]=='p' || string[0]=='r' || string[0]=='a') {
		if(strcmp(string,"start")==0) {
			if(current!=NULL) {
				while(current->prev != NULL)
					current=current->prev;
			} else if(waiting==NULL && current==NULL && now==NULL) {
				puts("No task could run");
				goto input;
			}
			//send signal every 0.01 seconds
			t.it_interval.tv_usec=10000;
			t.it_value.tv_usec=10000;
			t.it_interval.tv_sec=0;
			t.it_value.tv_sec=0;
			setitimer(ITIMER_REAL,&t,&oldvalue);
			signal(SIGTSTP,signal_handler); //ctrl+z
			signal(SIGALRM,signal_handler);

			//push ctrl+z
			if(control==1) {
				control=0;
				if(now!=NULL) {
					setitimer(ITIMER_REAL,&t,&oldvalue);
					signal(SIGTSTP,signal_handler); //ctrl+z
					signal(SIGALRM,signal_handler);

					setcontext(&now->context);
				} else if(current!=NULL && now==NULL) {
					while(current->prev!=NULL)
						current=current->prev;
					current->state=0;
					current->timer2=time(NULL);
					current->total_time+=(current->timer2-current->timer1);
					now=current;
					if(current->next!=NULL) {
						current=current->next;
						current->prev=NULL;
					} else {
						current=NULL;
					}

					setcontext(&now->context);
				} else if(waiting!=NULL && now==NULL && current==NULL) {
				}
			}
			//linux could have only one setitimer at a time
			setitimer(ITIMER_REAL,&t,&oldvalue);
			signal(SIGTSTP,signal_handler); //ctrl+z
			signal(SIGALRM,signal_handler);

			//when task finish or get ctrl+z signal come here
			getcontext(&uc_context);
			if(control==1)
				goto input;
			if(control==0) {
				//task finish
				if(now!=NULL) {
					now->state=3;
					if(terminate==NULL) { //terminate
						terminate=now;
						terminate->next=NULL;
						terminate->prev=NULL;
						now=NULL;
					} else {
						while(terminate->next!=NULL)
							terminate=terminate->next;
						terminate->next=now;
						terminate->next->prev=terminate;
						terminate->next->next=NULL;
						now=NULL;
					}
				}
				//no task is running now
				if(current!=NULL && now==NULL) {
					while(current->prev!=NULL)
						current=current->prev;
					current->state=0;
					current->timer2=time(NULL);
					current->total_time+=(current->timer2-current->timer1);

					now=current;
					if(current->next!=NULL) {
						current=current->next;
						current->prev=NULL;
					} else {
						current=NULL;
					}

					setcontext(&now->context);
				}
				//all task finish,stop setitimer
				if(current==NULL && waiting==NULL) {
					puts("All task are finish");
					t.it_interval.tv_usec=0;
					t.it_value.tv_usec=0;
					t.it_interval.tv_sec=0;
					t.it_value.tv_sec=0;
					setitimer(ITIMER_REAL,&t,NULL);
					goto input;
				}
			}
			if(now != NULL && control==1) {   //checck if any task in ready

				while(current->prev != NULL)   //let current point to head
					current=current->prev;
				//execute first task in ready queue
				current->state = 0;
				current->timer2=time(NULL);
				current->total_time+=(current->timer2-current->timer1);
				current->timer1=current->timer2;
				now=current;
				if(current->next != NULL) {  //more than one task
					current=current->next;
					current->prev=NULL;
				} else { //only one task
					current=NULL;
				}

				setcontext(&now->context);
			} else if(waiting==NULL && current==NULL && control==1) {
				puts("No Task need to be executed");
				goto input;
			}

			goto input;

		} else if(strcmp(string,"add") == 0) { //add task
			scanf("%s%c",&task[count],&skip);
			if(skip == '\n') {
				quantum[count]='S';
			} else {
				scanf("%s%c",&string,&skip);    //-t
				scanf("%c%c",&quantum[count],&skip);    //S or L

				if(strcmp(string,"-t")!=0 || !(quantum[count]=='S' || quantum[count]=='s'
				                               || quantum[count]=='L' || quantum[count]=='l')) {
					goto input;
				}
			}

			//task number must be 1~6
			if(task[count][0]=='t'&& task[count][1]=='a' && task[count][2]=='s'
			   && task[count][3]=='k' && (int)task[count][4]>48 && (int)task[count][4]<55) {

				sprintf(task[count],"task%c",task[count][4]);
				value = hw_task_create(&task[count]);
				//printf("name %s| quantum %c\n",task[count],quantum[count]);

			} else {
				//puts("Enter wrong commad, enter again");
				goto input;
			}
			goto input;
		} else if(strcmp(string,"ps") == 0) {
			//printf("ps\n");
			if(current!=NULL) {
				while(current->prev != NULL)   //let current point to head
					current=current->prev;
			}
			if(waiting!=NULL) {
				while(waiting->prev!=NULL)
					waiting=waiting->prev;
			}
			if(terminate!=NULL) {
				while(terminate->prev!=NULL)
					terminate=terminate->prev;
			}

			char task_state[20];
			printf("-----------------------------------------------------------------\n");
			printf("PID\tTask_Name\tTask_State\tQueueing_Time\n");
			while(current != NULL) {
				current->timer2=time(NULL);
				//printf("pid %d time1 %ld time2 %ld\n",current->pid,current->timer1,current->timer2);
				current->total_time+=(current->timer2-current->timer1);
				current->timer1=current->timer2;

				sprintf(task_state,"TASK_READY");
				printf(" %d\t %s\t\t%s\t  %ld\tsec\n",current->pid,current->name,task_state,
				       current->total_time);
				if(current->next!=NULL)
					current=current->next;
				else
					break;
			}
			if(now!=NULL) {
				sprintf(task_state,"TASK_RUNNING");
				printf(" %d\t %s\t\t%s\t  %ld\tsec\n",now->pid,now->name,task_state,
				       now->total_time);
			}
			while(waiting != NULL) {
				sprintf(task_state,"TASK_WAITING");
				printf(" %d\t %s\t\t%s\t  %ld\tsec\n",waiting->pid,waiting->name,task_state,
				       waiting->total_time);
				if(waiting->next!=NULL)
					waiting=waiting->next;
				else
					break;
			}
			while(terminate != NULL) {
				sprintf(task_state,"TASK_TERMINATED");
				printf(" %d\t %s\t\t%s\t  %ld\tsec\n",terminate->pid,terminate->name,task_state,
				       terminate->total_time);
				if(terminate->next!=NULL)
					terminate=terminate->next;
				else
					break;
			}
			goto input;

		} else if(strcmp(string,"remove") == 0) {
			//printf("remove\n");
			int remove_pid = 0,flag=0;
			scanf("%d%c",&remove_pid,&skip);

			struct NODE *remove=NULL;
			if(current!=NULL && flag!=1) {
				while(current->prev != NULL)   //let current point to head
					current=current->prev;
				while(current!=NULL && flag!=1) {
					if(current->pid == remove_pid) { //remove pid exist
						remove=current;
						flag=1;

						if(current->prev!=NULL) {
							if(current->next!=NULL) {
								current=current->prev;
								current->next=current->next->next;
								current->next->prev=current;
							} else {
								current=current->prev;
								current->next=NULL;
							}
						} else {
							if(current->next!=NULL) {
								current=current->next;
								current->prev=NULL;
							} else {
								current=NULL;
							}
						}
					} else {
						if(current->next!=NULL) {
							current=current->next;
						} else
							goto A;
					}
				}
			}
A:
			if(waiting!=NULL && waiting!=1) {
				while(waiting->prev!=NULL)
					waiting=waiting->prev;
				while(waiting!=NULL && flag!=1) {
					if(waiting->pid==remove_pid) {
						remove=waiting;
						flag=1;
						if(waiting->prev!=NULL) {
							if(waiting->next!=NULL) {
								waiting=waiting->prev;
								waiting->next=waiting->next->next;
								waiting->next->prev=waiting;
							} else {
								waiting=waiting->prev;
								waiting->next=NULL;
							}
						} else {
							if(waiting->next!=NULL) {
								waiting=waiting->next;
								waiting->prev=NULL;
							} else {
								waiting=NULL;
							}
						}
					} else {
						if(waiting->next!=NULL)
							waiting=waiting->next;
						else
							goto B;
					}
				}
			}
B:
			if(terminate!=NULL && flag!=1) {
				while(terminate->prev!=NULL)
					terminate=terminate->prev;
				while(terminate!=NULL && flag!=1) {
					if(terminate->pid==remove_pid) {
						remove=terminate;
						if(terminate->prev!=NULL) {
							if(terminate->next!=NULL) {
								terminate=terminate->prev;
								terminate->next=terminate->next->next;
								terminate->next->prev=terminate;
							} else {
								terminate=terminate->prev;
								terminate->next=NULL;
							}
						} else {
							if(terminate->next!=NULL) {
								terminate=terminate->next;
								terminate->prev=NULL;
							} else {
								terminate=NULL;
							}
						}
					} else {
						if(terminate->next!=NULL)
							terminate=terminate->next;
						else
							goto C;
					}
				}
			}
C:
			if(now!=NULL && flag!=1) {
				if(now->pid==remove_pid) {
					remove=now;
					now=NULL;
				} else
					break;
			}
			if(remove!=NULL)
				free(remove);   //free memory
			else
				puts("No Such pid");
			//end of remove
		} else {
			puts("Enter wrong commad, enter again");
			goto input;
		}

input:
		printf("add/start/remove/ps: ");
		scanf("%s%c",&string,&skip);

	}   //end while

	return 0;
}
