#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#define N 10000000

int a[1<<25];
int cnt = 0;
int rss , prev_rss , vss, prev_vss;
int *arg1;
char *arg2;
pid_t _pid;

void check_change() {
 printf("vss = %d rss = %d\n",vss,rss);
 if(abs(vss - prev_vss) > 0) {
  printf("chnage in vss = %d\n", abs(vss - prev_vss));
 }
 if(abs(rss - prev_rss) > 0) {
  printf("chnage in rss = %d\n", abs(rss - prev_rss));
 } 
 prev_vss = vss; prev_rss = rss;
}

void recurse() {
 if(cnt == 64) {
  arg1[0] = (int)(_pid); 
  syscall(333,arg1,arg2);
  vss = arg1[1];
  rss = arg1[2];
  check_change();
  return;
 }else {
  cnt++;
  int x[1<<12];
  recurse();
 }
}

int main() {
 
 // setup...
 arg1 = malloc(5*sizeof(int));
 arg2 = malloc(sizeof(char)*1024);
 _pid = getpid();
 printf("pid of this process = %d\n",_pid);
 //...


 // first observatrion
 arg1[0] = (int)(_pid);
 long res = syscall(333,arg1,&arg2);
 vss = prev_vss = arg1[1];
 rss = prev_rss = arg1[2];
 printf("without anything\n");
 printf("vss = %d rss = %d\n",vss,rss);
 printf("----------------------\n");
 // -------------------------------------------------------------------------


 // changes
 unsigned int *ptr = malloc(1<<21); // 2 MB 
 printf("ptr = %p\n",ptr);
 //

 
 // second observatrion
 printf("with malloc of 2 MB\n");
 arg1[0] = (int)(_pid); 
 syscall(333,arg1,&arg2);
 vss = arg1[1];
 rss = arg1[2];
 check_change();
 printf("----------------------\n");
 //.............................................................................


 // changes
 int i = 0;
 printf("address of i = %p\n",&i);
 while(i < ( 512 * (1<<8) ) ) {
  ptr[i] = 1;
  i++;
 }

 // third observatrion
 printf("512 KB access of the above 2MB\n");
 arg1[0] = (int)(_pid); 
 syscall(333,arg1,arg2);
 vss = arg1[1];
 rss = arg1[2];
 check_change();
 printf("----------------------\n");


 // changes
 free(ptr);
 //

 // fourth observatrion
 printf("After free of the array\n");
 arg1[0] = (int)(_pid); 
 syscall(333,arg1,&arg2);
 vss = arg1[1];
 rss = arg1[2];
 check_change();
 printf("----------------------\n");
 

 // check for the global data a[]

 i = 0;
 while(i < ( 8 * (1<<20) ) ) {
  a[i] = 1;
  i++;
 }
 // fifth observatrion
 printf("After access the global array\n");
 
 
 arg1[0] = (int)(_pid); 
 syscall(333,arg1,&arg2);
 vss = arg1[1];
 rss = arg1[2];
 check_change();
 printf("----------------------\n"); 


 printf("Recursion\n");
 recurse(arg1,&arg2,_pid);

 // generic test
 for(i = 0; i<30 ;i++) {
  calloc(1ULL<<i,1);
  printf("after calloc = [ %lld B] \n",1ULL<<i);
  arg1[0] = (int)(_pid); 
  syscall(333,arg1,&arg2);
  vss = arg1[1];
  rss = arg1[2];
  check_change();
  printf("------------------\n");
 }
 return 0;
}
