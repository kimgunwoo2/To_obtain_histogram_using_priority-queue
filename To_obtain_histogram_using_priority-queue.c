#define _CRT_SECURE_NO_WARNINGS
#include <pthread.h>
#include <stdlib.h>
#include <semaphore.h>
#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <assert.h>
#include <sys/time.h>
sem_t sem_bounded;
sem_t sem_histogram;
sem_t sem_full;
sem_t sem_count;
sem_t sem_empty;
int data_amount=0;
int histogram[256] = { 0, }; // save histogram data
int wait_thread_count=0;
int status=0;
int boundedSize;
int data_count=0;
float average=0;
float priority_time[5]={0,};
float priority_count[5]={0,};
typedef struct file_info{
    char file_name[20];
    int priority;
    struct timeval in_time;
}file_info;

typedef struct _heap{
    int numOfData;
    file_info* f_in;
}Heap;
Heap heap;
void HeapInit(Heap *ph){
    ph->numOfData=0;
    ph->f_in=(file_info*)malloc(sizeof(file_info)*(boundedSize+1));
}

int getParentidx(int idx){
    return idx/2;
}
int getLChildidx(int idx){
    return idx*2;
}
int getRChildidx(int idx){
    return getLChildidx(idx)+1;
}

int getHipriChildidx(Heap * ph, int idx){
    if(getLChildidx(idx)>ph->numOfData)
        return 0;
    else if (getLChildidx(idx)==ph->numOfData)
        return getLChildidx(idx);
    else{
        if(ph->f_in[getLChildidx(idx)].priority>ph->f_in[getRChildidx(idx)].priority)
            return getRChildidx(idx);
        else
            return getLChildidx(idx);        
    }
}
void Hinsert(Heap* ph,file_info f_in){
    int idx=ph->numOfData+1;
    file_info temp =f_in;
    while(idx!=1){
        if(temp.priority<ph->f_in[getParentidx(idx)].priority){
            ph->f_in[idx]=ph->f_in[getParentidx(idx)];
            idx=getParentidx(idx);
        }
        else 
            break;
    }
    ph->f_in[idx]=temp;
    ph->numOfData+=1;
}
file_info HDelete(Heap* ph){
    file_info ret_info=ph->f_in[1];
    file_info lasttemp=ph->f_in[ph->numOfData];
    
    int parentidx=1;
    int childidx;

    while(childidx=getHipriChildidx(ph,parentidx))
    {
        if(lasttemp.priority<=ph->f_in[childidx].priority)
            break;
        ph->f_in[parentidx]=ph->f_in[childidx];
        parentidx=childidx;    
    }
    ph->f_in[parentidx]=lasttemp;
    ph->numOfData-=1;
    return ret_info;
        
}
                      
void *worker()
{
   struct timeval start,end,out_time,in_time;
   int res;
   float tvalue;
   res=gettimeofday(&start,NULL);
   assert(res==0);
   file_info fileNumber;
   int data_file;
   unsigned char buf[256]={0,};
   int bufsize;
   int histogram_data[256]={0,};
   while (1)
   {
      sem_wait(&sem_count);
      wait_thread_count++;
      sem_post(&sem_count);
      sem_wait(&sem_empty);
      sem_wait(&sem_count);
      wait_thread_count--;
      sem_post(&sem_count);
      if(status==1){
        break;
    }
      
      sem_wait(&sem_bounded);
      fileNumber = HDelete(&heap);
      res=gettimeofday(&out_time,NULL);
      assert(res==0);
      in_time=fileNumber.in_time;
      priority_time[fileNumber.priority]+=out_time.tv_sec-in_time.tv_sec+(out_time.tv_usec-in_time.tv_usec)/1000000.0;
      priority_count[fileNumber.priority]++;
      printf("wait time %.4f\t",out_time.tv_sec-in_time.tv_sec+(out_time.tv_usec-in_time.tv_usec)/1000000.0);
      printf("file name %s priority %d\n",fileNumber.file_name,fileNumber.priority);
      sem_post(&sem_bounded); 
      sem_post(&sem_full);
      memset(histogram_data,0,sizeof(histogram_data));
      data_file=open(fileNumber.file_name,O_RDONLY);
      while((bufsize=read(data_file,buf,64))>0){
        for(int i=0; i<bufsize; i++){
            histogram_data[buf[i]]++;    
        }
        memset(buf,0,256);
     }
     close(data_file);
     usleep(10000);
     sem_wait(&sem_histogram);
    for(int i=0; i<256; i++){
        histogram[i]+=histogram_data[i];
    }
    data_count++;
     if(data_count==data_amount){
        status=1;
        printf("end\n");
     }
     sem_post(&sem_histogram);
    
   }
   res=gettimeofday(&end,NULL);
   assert(res==0);
   tvalue=end.tv_sec-start.tv_sec+(end.tv_usec-start.tv_usec)/1000000.0;
   printf("thread worked time : %.4f ms\n",tvalue);
   average+=tvalue;
}
int main(int argc, char* argv[])
{
   int res;
   struct timeval start,end;
   pthread_t *thread_t; // size of thread
   int threadSize = 1;
   int his_file;
   int i=0;
   float tvalue;
   file_info *f_in;
   char line[256];
   char temp[256];
   FILE* rf;
   char *tok;
   res=gettimeofday(&start,NULL);
   assert(res==0);
   if(argc==4){
    boundedSize = atoi(argv[3]);
      threadSize = atoi(argv[2]);
   }else if(argc==3){
    threadSize = atoi(argv[2]);
    boundedSize = 1;
   }else if(argc==2){
    threadSize = 1;
    boundedSize = 1;
   }else{
    printf("error please again\n");
    return 0;
    }    
   HeapInit(&heap);
   rf=fopen(argv[1],"r");
   if(rf==NULL){
    printf("no file\n");
    return 0;
    }
  
  fgets(temp,sizeof(temp),rf);
  tok=strtok(temp,"\n");
  data_amount=atoi(temp);
  f_in=malloc(sizeof(file_info)*data_amount);
   while(!feof(rf)){
    fgets(line,sizeof(line),rf);
       tok=strtok(line,"\n");
    tok=strtok(line," ");
    strcpy(f_in[i].file_name,line);
    f_in[i].priority=atoi(strtok(NULL," "));
    i++;
    if(i==data_amount)
        break;
   }
  
   
   sem_init(&sem_count, 0, 1);
   sem_init(&sem_bounded, 0, 1);
   sem_init(&sem_histogram, 0, 1);
   sem_init(&sem_full, 0, boundedSize);
   sem_init(&sem_empty, 0, 0);
   thread_t = (pthread_t *)malloc(threadSize* sizeof(pthread_t));

   for (i = 0; i< threadSize; i++)
   {
      if (pthread_create(&thread_t[i], NULL, worker, NULL) < 0)
      {
         perror("thread create error:");
         exit(0);
      }
   }

  
   for (int i = 0; i < data_amount; i++) 
   {
      sem_wait(&sem_full);
      sem_wait(&sem_bounded);
      res=gettimeofday(&f_in[i].in_time,NULL);
      assert(res==0);
      Hinsert(&heap,f_in[i]);
      sem_post(&sem_bounded);
      sem_post(&sem_empty);
   }
   while(1){
    if(status==1&&wait_thread_count==threadSize){
        for(i=0; i<threadSize; i++){
            sem_post(&sem_empty);        
        }
    break;
    }
   }
   int sum;
   for(int i=0; i<5; i++){
    printf("\n %d priority waiting average time  %.6f\n",i,priority_time[i]/priority_count[i]);
    sum=priority_count[i]+sum;
   }
   printf("file count %d\n",sum);
    
   his_file=open("histogram.bin",O_RDWR);
   lseek(his_file,(off_t)0,SEEK_SET);
   write(his_file,histogram,1024);
   close(his_file);
   res=gettimeofday(&end,NULL);
   assert(res==0);
   average=average/threadSize;
   printf("total thread worked time Average : %.4f ms\n",average);
   tvalue=end.tv_sec-start.tv_sec+(end.tv_usec-start.tv_usec)/1000000.0;
   printf("program total time %.4f ms\n",tvalue);

   return 0;
} 