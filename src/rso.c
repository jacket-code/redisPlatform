/*
 * Copyright (c) 2015, tencent jacketzhong <jacketzhong at gmail dot com>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *   * Redistributions of source code must retain the above copyright notice,
 *     this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *   * Neither the name of Redis nor the names of its contributors may be used
 *     to endorse or promote products derived from this software without
 *     specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include "fmacros.h"
#include <dlfcn.h>
#include "rso.h"
#include "rsoinfo.h"
#include "redis.h"

extern void addReplyStatusLength(redisClient *c, char *s, size_t len);

/* A case insensitive version used for the command lookup table and other
 * places where case insensitive non binary-safe comparison is needed. */
int rsodictSdsKeyCaseCompare(void *privdata, const void *key1,
        const void *key2)
{
    DICT_NOTUSED(privdata);
    return strcasecmp(key1, key2) == 0;
}

unsigned int rsodictSdsCaseHash(const void *key) {
	return dictGenCaseHashFunction((unsigned char*)key, sdslen((char*)key));
}

void rsodictSdsDestructor(void *privdata, void *val) {
	DICT_NOTUSED(privdata);
	redisLog(REDIS_NOTICE, "close so %s", val);
	sdsfree(val);
}

void rsodictSODestructor(void *privdata, void *val) {
	DICT_NOTUSED(privdata);
	if (val == NULL) {
		return;
	}
	dlclose(val);  
	redisLog(REDIS_NOTICE, "close so lib");
}

/* rso table. sds string -> command struct pointer. */
dictType rsoDictType = {
	rsodictSdsCaseHash,			/* hash function */
	NULL,						/* key dup */
	NULL,						/* val dup */
	rsodictSdsKeyCaseCompare,	/* key compare */
	rsodictSdsDestructor,		/* key destructor */
	rsodictSODestructor			/* val destructor */
};

/* rso table. sds string -> command struct pointer. */
dictType rsoRemoteDictType = {
	rsodictSdsCaseHash,			/* hash function */
	NULL,						/* key dup */
	NULL,						/* val dup */
	rsodictSdsKeyCaseCompare,	/* key compare */
	rsodictSdsDestructor,		/* key destructor */
	rsodictSdsDestructor		/* val destructor */
};

/* rso table. sds string -> command struct pointer. */
dictType rsoTimerDictType = {
	rsodictSdsCaseHash,			/* hash function */
	NULL,						/* key dup */
	NULL,						/* val dup */
	rsodictSdsKeyCaseCompare,	/* key compare */
	rsodictSdsDestructor,		/* key destructor */
	NULL,						/* val destructor */
};

dict * rsodict = NULL;			/* rso table */
dict * remotersodict = NULL;	/* remote rso tbale */
dict * rsotimerdict = NULL;	/* remote rso tbale */

int loadrso(char * cmd, char * rsoname) {
	redisLog(REDIS_NOTICE, "load rso, cmd is %s, the rsoname is %s", cmd, rsoname);
	if (rsodict == NULL) {
		rsodict = dictCreate(&rsoDictType,NULL);
	}
	char * sopath = getAbsolutePath(rsoname);
	void * lib = dlopen(sopath, RTLD_LAZY);  
	if(lib == NULL) {  
		redisLog(REDIS_WARNING, "load so error, the rsoname is : %s", sopath);
	} else {
		sds cmdkey = sdsnew(cmd);
		dictAdd(rsodict, cmdkey, lib);
	} 
	return REDIS_OK;
}

int loadremoterso(char * cmd, char * address) {
	redisLog(REDIS_NOTICE, "load remote rso, cmd is %s, the address is %s", cmd, address);
	if (remotersodict == NULL) {
		remotersodict = dictCreate(&rsoRemoteDictType,NULL);
	}
	sds cmdkey = sdsnew(cmd);
	sds libaddress = sdsnew(address);
	dictAdd(remotersodict, cmdkey, libaddress);
	return REDIS_OK;
}

int rsoTimer(struct aeEventLoop *eventLoop, long long id, void *clientData) {
	redisLog(REDIS_NOTICE, "timer event , id : %d", id);
	sds timerkey = sdsfromlonglong(id);
	void * lib = dictFetchValue(rsotimerdict, timerkey);
	if (lib) {
		rso * rsao = (rso *)dlsym(lib, "rsao");
		if (rsao->onTimer) {
			int ret = rsao->onTimer();
			if (ret == 1) {
				return rsao->hasTimer;
			}
		}
	}
	return -1;
}

/* rsoCommand */
void rsoCommand(redisClient *c) {
	redisLog(REDIS_NOTICE, "%d, argv1 : %s, arg2 : %s", c->argc, c->argv[1]->ptr, c->argv[2]->ptr);
	sds cmdname = sdsnew((char*)c->argv[1]->ptr);
	void * lib = dictFetchValue(rsodict, cmdname);
	if (lib) {
		int len = 0;
		char * reply = rsoExec(lib, (char *)c->argv[2]->ptr, &len);
		if (reply == NULL) {
			sdsfree(cmdname);
			addReply(c, shared.err);
			return;
		}
		addReplyStatusLength(c,reply,len);
		free(reply);
		sdsfree(cmdname);
		return;
	}
	/* maybe find at remote redis */
	sds remoteaddress = (sds)dictFetchValue(remotersodict, cmdname);
	if (remoteaddress == NULL) {
		addReply(c, shared.err);
	} else {
		addReplyStatusLength(c,remoteaddress,sdslen(remoteaddress));
	}
	sdsfree(cmdname);
}

/* rsoReloadCommand */
void rsoReloadCommand(redisClient *c) {
	int ret = rsoReload();
	if (ret == REDIS_OK) {
		addReply(c, shared.ok);
	} else {
		addReply(c, shared.err);
	}
}

void rsoInit() {
	dictIterator *di = dictGetIterator(rsodict);
	dictEntry *de = NULL;
	while((de = dictNext(di)) != NULL) {
		void * solib = dictGetVal(de);
		InitSo soInit = (InitSo)dlsym(solib, "OnInit");
		soInit();  
		rso * rsao = (rso *)dlsym(solib, "rsao");
		redisLog(REDIS_NOTICE, "setup timer %d", rsao->hasTimer);
		/* set timer */
		if (rsao->hasTimer > 0) {
			long long timerid = aeCreateTimeEvent(server.el, rsao->hasTimer, rsoTimer, NULL, NULL);
			redisLog(REDIS_NOTICE, "timer %d", timerid);
			if(timerid == AE_ERR) {
				redisLog(REDIS_NOTICE, "setup timer error");
			} else {
				if (rsotimerdict == NULL) {
					rsotimerdict = dictCreate(&rsoTimerDictType,NULL);
				}
				sds timerkey = sdsfromlonglong(timerid);
				dictAdd(rsotimerdict, timerkey, solib);
			}
		}
	}
	dictReleaseIterator(di);
}

char * rsoExec(void * lib, char * msg, int * len) {
	rso * rsao = (rso *)dlsym(lib, "rsao");
	if (rsao->onExec) {
		return rsao->onExec(msg, len);
	}
	return NULL;
}

void rsoExit() {
	if (rsodict) {
		dictRelease(rsodict);
		rsodict = NULL;
	}
	if (remotersodict) {
		dictRelease(remotersodict);
		remotersodict = NULL;
	}
	if (rsotimerdict) {
		dictRelease(rsotimerdict);
		rsotimerdict = NULL;
	}
}

/* reload config */
int loadRsoServerConfigFromString(char *config);
int loadRsoConfig(char *filename);
/* reload */
int rsoReload() {
	rsoExit();
	int ret = loadRsoConfig(server.configfile);
	if (ret == REDIS_OK) {
		rsoInit();
	}
	return ret;
}

/* read config */
int loadRsoConfig(char *filename) {
	sds config = sdsempty();
	char buf[REDIS_CONFIGLINE_MAX+1];
	/* Load the file content */
	if (filename) {
		FILE *fp;
		if (filename[0] == '-' && filename[1] == '\0') {
			fp = stdin;
		} else {
			if ((fp = fopen(filename,"r")) == NULL) {
				redisLog(REDIS_WARNING,
					"Fatal error, can't open config file '%s'", filename);
				return REDIS_ERR;
			}
		}
		while(fgets(buf,REDIS_CONFIGLINE_MAX+1,fp) != NULL)
			config = sdscat(config,buf);
		if (fp != stdin) fclose(fp);
	}
	int ret = loadRsoServerConfigFromString(config);
	sdsfree(config);
	return ret;
}

int loadRsoServerConfigFromString(char *config) {
    char *err = NULL;
    int linenum = 0, totlines, i;
    int slaveof_linenum = 0;
    sds *lines;
    lines = sdssplitlen(config,strlen(config),"\n",1,&totlines);
    for (i = 0; i < totlines; i++) {
        sds *argv;
        int argc;
        linenum = i+1;
        lines[i] = sdstrim(lines[i]," \t\r\n");
        /* Skip comments and blank lines */
        if (lines[i][0] == '#' || lines[i][0] == '\0') continue;
        /* Split into arguments */
        argv = sdssplitargs(lines[i],&argc);
        if (argv == NULL) {
            err = "Unbalanced quotes in configuration line";
            goto loaderr;
        }
        /* Skip this line if the resulting command vector is empty. */
        if (argc == 0) {
            sdsfreesplitres(argv,argc);
            continue;
        }
        sdstolower(argv[0]);
        /* Execute config directives */
		if (!strcasecmp(argv[0],"include") && argc == 2) {
            loadRsoConfig(argv[1]);
		} else if (!strcasecmp(argv[0],"loadrso") && argc == 3) {
			loadrso(argv[1], argv[2]);
		} else if (!strcasecmp(argv[0],"loadremoterso") && argc == 3) {
			loadremoterso(argv[1], argv[2]);
		}
        sdsfreesplitres(argv,argc);
    }
    sdsfreesplitres(lines,totlines);
    return REDIS_OK;

loaderr:
    fprintf(stderr, "\n*** FATAL CONFIG FILE ERROR ***\n");
    fprintf(stderr, "Reading the configuration file, at line %d\n", linenum);
    fprintf(stderr, ">>> '%s'\n", lines[i]);
    fprintf(stderr, "%s\n", err);
	sdsfreesplitres(lines,totlines);
    return REDIS_ERR;
}
