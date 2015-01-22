#include<stdio.h>
#include<dirent.h>
#include<ctype.h>
#include<vector>
#include<map>
#include<cstring>
#include<stdlib.h>
#include<ctime>
#include<time.h>
#include<sys/time.h>
#include<algorithm>
#include<unistd.h>
#include<stdarg.h>
#include "curControl.h"


using namespace std;

int clearLine;
/*
read process id,name from /proc/[pid]/stat
*/
void getProcInfo(FILE* file,char *array)
{
	int pid;
	fscanf(file,"%d %s",&pid,array);	
}

/*
 read from /proc/[pid]/io
*/
void getIOAmount(FILE *file, unsigned long long *readBytes, unsigned long long *writeBytes,long long * updateTime)
{
	char tmp[64];
	int i=0;
	for(i=0;i<4;++i)
	{
		fscanf(file,"%s %llu",tmp,readBytes);	
		//printf("%s %llu\n",tmp,*readBytes);
	}


	fscanf(file,"%s %llu",tmp,readBytes);	
			//printf("%s %llu\n",tmp,*readBytes);
	fscanf(file,"%s %llu",tmp,writeBytes);	
			//printf("%s %llu\n",tmp,*writeBytes);

	struct timeval tv;
	gettimeofday(&tv,NULL);
	*updateTime=tv.tv_sec*1000+tv.tv_usec/1000;
} 


/*
 get all directory of processes from /proc/
*/
vector<int>listDir(map<int,char> pidMap)
{
	DIR *dir;
	struct dirent *ent;
	vector<int> procId;
	if ((dir = opendir ("/proc/")) != NULL) {
	  /* print all the files and directories within directory */
	  while ((ent = readdir (dir)) != NULL) {
	    if(isdigit(ent->d_name[0]))
	   	{
			//char * buffer=(char *)malloc(strlen(ent->d_name)+1);
			//strcpy(buffer,ent->d_name);
			//printf ("%s\n", buffer);
			int tpid=atoi(ent->d_name);
			procId.push_back(tpid);
			pidMap.insert(std::pair<int,char>(tpid,'0'));
		}
	  }
	  closedir (dir);
	} else {
	  /* could not open directory */

		printf("cannot open /proc/");
	}
	return procId;
}


struct procIoSt
{
	unsigned long long readBytes;
	unsigned long long writeBytes;
	unsigned long long lastUptime;
};

struct pidIOThr
{
	int pid;
	unsigned long long ioThr;
};

bool cmp(const struct pidIOThr * arg1,const struct pidIOThr * arg2)
{
	if(arg1->ioThr==arg2->ioThr)
		return false;

	return arg1->ioThr>arg2->ioThr?true:false;
}

char * itoa(int val, char *array)
{
	sprintf(array,"%d",val);
	return array;
}

/**
concatenate char arrays
para:
	paraCount,number of char arrays to be concatenated
	...,char arrays
return value:
	char arrays generated by concatenation of all char arrays	
caution:
	length limitition is 512 bytes
**/
char * strConcat(int paraCount,...)
{
	va_list ap;
	va_start(ap,paraCount);
	char * res=(char *)malloc(sizeof(char)*512);
	int cur=0;
	for(int i=0;i<paraCount;++i)
	{
		char * tp=va_arg(ap,char *);
		strcpy(res+cur,tp);
		cur+=strlen(tp);
	}
	res[cur]='\0';
	va_end(ap);
	return res;
}

/**
get process name
para:
	name,process name
	pid,process id
description:
	if process does not exist, process
	name will be "(NULL)"
**/
void getProcessName(char * name,int pid)
{
	char tmp[64];
	itoa(pid,tmp);
	char pName[64];
	strcpy(pName,"(NULL)");
	char * procName=strConcat(3,"/proc/",tmp,"/stat");
	FILE *file=fopen(procName,"r");
	if(file!=NULL)
	{
		getProcInfo(file,pName);
		fclose(file);
		free(procName);
	}
	strcpy(name,pName);
}

void print(vector<struct pidIOThr *> readThr,vector<struct pidIOThr *> writeThr)
{
	int toprN=5;
	int topwN=5;
	if(readThr.size()<toprN)
		toprN=readThr.size();

	if(writeThr.size()<topwN)
		topwN=writeThr.size();

	for(int i=0;i<clearLine;++i)
	{
		curMoveUp();
		eraseLine();
	}
	clearLine=0;

	printf("Read\n");
	clearLine++;
	for(int i=0;i<toprN;++i)
	{
		char pName[64];
		getProcessName(pName,readThr[i]->pid);
		printf("%8d\t%16s%12llu kb/s\n",readThr[i]->pid,pName,readThr[i]->ioThr);
		clearLine++;
	}

	printf("Write\n");
	clearLine++;
	for(int i=0;i<topwN;++i)
	{
		char pName[64];
		getProcessName(pName,writeThr[i]->pid);
		//printf("pid:%d%s write:%llu b/s\n",writeThr[i]->pid,pName,writeThr[i]->ioThr);
		printf("%8d\t%16s%12llu kb/s\n",writeThr[i]->pid,pName,writeThr[i]->ioThr);
		clearLine++;
	}
}


void uidInfo()
{
	if(getuid()!=0)
	{
		printf("Not root user,only display processes owned by current user\n");	
	}
}

int main()
{
	uidInfo();
	int loopCount=5;
	int readTop=5;
	int writeTop=5;
	
	clearLine=0;

	map<int,char> procIdMap;
	map<int,struct procIoSt*> pidToIoSt;
	vector<struct pidIOThr *> readThr;
	vector<struct pidIOThr *> writeThr;

	char descPath[128];
	strcpy(descPath,"/proc/");
	printf("%8s\t%16s%17s\n","PID","PROCESS NAME","IO RATE");
	while(true)
	{
		procIdMap.clear();	
		readThr.clear();
		writeThr.clear();
		vector<int> procId=listDir(procIdMap);
		vector<int> pidToDel;
		for(map<int,struct procIoSt*>::iterator it=pidToIoSt.begin();
			it!=pidToIoSt.end();++it)
		{
			if(procIdMap.find(it->first)!=procIdMap.end())
				pidToDel.push_back(it->first);
		}
		for(int i=0;i<pidToDel.size();++i)
			pidToIoSt.erase(pidToDel[i]);

		
		for(int i=0;i<procId.size();++i)
		{
			char pidCharArray[32];
			itoa(procId[i],pidCharArray);
			strcpy(descPath+6,pidCharArray);
			strcpy(descPath+6+strlen(pidCharArray),"/io");
			descPath[6+3+strlen(pidCharArray)]='\0';
			FILE *file=fopen(descPath,"r");
			if(file==NULL)
			{
				continue;
			}
			unsigned long long readBytes,writeBytes;
			long long updateTime;
			getIOAmount(file,&readBytes,&writeBytes,&updateTime);
			fclose(file);
			if(pidToIoSt.find(procId[i])!=pidToIoSt.end())
			{
				struct procIoSt * tp=pidToIoSt[procId[i]];

				struct pidIOThr * tRead=(struct pidIOThr*)malloc(sizeof(struct pidIOThr));
				struct pidIOThr * tWrite=(struct pidIOThr*)malloc(sizeof(struct pidIOThr));
				tRead->pid=procId[i];
				tRead->ioThr=(readBytes-tp->readBytes)/(updateTime-tp->lastUptime);

				tWrite->pid=procId[i];
				tWrite->ioThr=(writeBytes-tp->writeBytes)/(updateTime-tp->lastUptime);

				readThr.push_back(tRead);
				writeThr.push_back(tWrite);

				tp->lastUptime=updateTime;
				tp->readBytes=readBytes;
				tp->writeBytes=writeBytes;
			}
			else
			{
				struct procIoSt *tp=(struct procIoSt *)malloc(sizeof(struct procIoSt));
				tp->lastUptime=updateTime;
				tp->readBytes=readBytes;
				tp->writeBytes=writeBytes;
				pidToIoSt.insert(std::pair<int,struct procIoSt*>(procId[i],tp));
			}
		}

		sort(readThr.begin(),readThr.end(),cmp);
		sort(writeThr.begin(),writeThr.end(),cmp);

		print(readThr,writeThr);
		sleep(1);

	}

	return 0;
}
