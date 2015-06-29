#include <stdio.h>
#include <malloc.h>
#include "../src/rsoinfo.h"
#include "cpptest.h"
#include "cJSON.h"

rso rsao = {1, 0, NULL, NULL};
cpphello hello;

char * Exec(char * soData, int * replylen) {
	printf("\nIn cpptest.cpp, SoData : %s\n", soData);
	hello.SayHelloToRedis();
	*replylen = 140;
	char * replystr = (char *)malloc(*replylen);
	sprintf(replystr, "sum:err");

	cJSON *root_json = cJSON_Parse(soData);
	if (NULL == root_json) {
		printf("error:%s\n", cJSON_GetErrorPtr());
		cJSON_Delete(root_json);
		return replystr;
	}
	cJSON *argv1 = cJSON_GetObjectItem(root_json, "argv1");
	if (!argv1) {
		cJSON_Delete(root_json);
		return replystr;
	}
	int intargv1 = argv1->valueint;

	cJSON *argv2 = cJSON_GetObjectItem(root_json, "argv2");
	if (!argv2) {
		cJSON_Delete(root_json);
		return replystr;
	}
	int intargv2 = argv2->valueint;

	int sum = intargv1 + intargv2;
	sprintf(replystr, "sum:%d", sum);
	cJSON_Delete(root_json);
	return replystr;
}

int TimerEvnet() {
	printf("\ncpptest, On Timer Event, i am cpp\n");
	return 1;
}

void OnInit() {
	rsao.hasTimer = 40000;
	rsao.timerID = 0;
	rsao.onExec = (soExecProc*)Exec;
	rsao.onTimer = (soTimerProc*)TimerEvnet;
}

void cpphello::SayHelloToRedis() {
	printf("\nHello redis, I'm CPP\n");
}
