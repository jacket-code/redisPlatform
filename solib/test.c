#include <stdio.h>
#include <malloc.h>
#include "../src/rsoinfo.h"
#include "test.h"

rso rsao = {1, 0, NULL, NULL};

char * Exec(char * soData, int * replylen) {
	/* write your code here */
	printf("\nIn Test.c, SoData : %s\n", soData);
	*replylen = 40;
	char * replystr = (char *)malloc(*replylen);
	sprintf(replystr, "jackettest");
	return replystr;
}

int TimerEvnet() {
	/* write your code here */
	printf("\n test.c : On Timer Event\n");

	return 1;	
}

void OnInit() {
	rsao.hasTimer = 10000;
	rsao.timerID = 0;
	rsao.onExec = (soExecProc*)Exec;
	rsao.onTimer = (soTimerProc*)TimerEvnet;
}

