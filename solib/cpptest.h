#ifndef __CPPTEST_H__
#define __CPPTEST_H__

#include <stdio.h>

char * Exec(char * soData, int * replylen);
int TimerEvnet();
extern "C" void OnInit(); 

class cpphello {
public:
	cpphello() {}
	~cpphello() {}
public:
	void SayHelloToRedis();

};

#endif
