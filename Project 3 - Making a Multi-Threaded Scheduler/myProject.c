#define NR_SLICES 3	// Initial assumption.
#define PHASE_TYPES 2		// I/O, and CPU.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include <pthread.h>
#include <unistd.h>


int jobId = 0;
int runningQueueIsEmpty;



typedef enum {READY, RUNNING, BLOCKED, COMPLETE} job_state;
typedef enum {CPU, IO} phase_type;

pthread_mutex_t lock_runningQueue;
pthread_mutex_t lock_ioQueue = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t lock_finishedQueue = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t runningQueueIsNotEmptyCondition = PTHREAD_COND_INITIALIZER;

struct threadName {
	 char * threadName;
	int threadId;
};

void setThreadName(struct threadName * threadName, char *nameToSet, int number){
	char numberToString[10];
	int i = 0;
	sprintf(numberToString, "%d", number);
	threadName->threadName = (char *)malloc(sizeof(nameToSet) + strlen(nameToSet) + 2);
	for(i = 0; i < strlen(nameToSet); i++){
		threadName->threadName[i] = nameToSet[i];
	}
	threadName->threadName[i] = numberToString[0];
	threadName->threadName[i+1] = '\0';
}

char *  threadToString(struct threadName threadName){
	return threadName.threadName;
}


struct job {	
	int jobNumber;
	job_state jobState;
	phase_type phaseTypeToExecute;
	int jobNumberToExecute;	// get time to run at this index.
	int is_completed;
	int cpuSliceCount;
	int ioSliceCount;
	int lastJobExecuted;
	int phases_and_times[PHASE_TYPES][NR_SLICES];
};

void jobToString(struct job *job){
	printf("\nJob Number =  %d \n",job->jobNumber);
	printf("\nJob Completed? = %d\n", job->is_completed);
	printf("\nJob  State = %d\n", job->jobState);
}


struct node {
	struct job *job;
	struct node *next;
};

struct Queue {
	struct node * front;
	struct node * rear;
	int size;
	char *name;
} *runningQueue, *ioQueue, *finishedQueue;



void printStatus(pthread_mutex_t  lock){
	
	printf("\n-----------------------------\n");
	printf("\nStatus Report\n");
	printf("Finished Queue has: %d jobs\n", getQueueSize(finishedQueue, lock));
	printf("Ready Queue hjas %d jobs\n", getQueueSize(runningQueue, lock));
	printf("IO Queue has %d jobs\n", getQueueSize(ioQueue, lock));
	printf("---------------------------------\n\n");
}



struct job*  createJob(){
	int i = 0;
	struct job* job  = (struct job*)malloc(sizeof(struct job));
	job->jobNumber =  ++jobId;
	job->jobState = READY;
	job->is_completed = 0;
	// the first job to run.	
	for(i  = 0; i < NR_SLICES; i++){
		job->phases_and_times[CPU][i] =  20 * (i + 1);
		job->phases_and_times[IO][i] = 25 * (i + 1);
	}
	printf("NBUMBER OF SLICKES = %D\n", i);
	job->phaseTypeToExecute = CPU;	// the slice to start running.
	job->cpuSliceCount = 0;
	job->ioSliceCount = 0;
	return job;	
}

int checkIfPhaseTypeToExecuteIsIO(struct job *job){
	return job->phaseTypeToExecute == IO ? 1 : 0;
}

void enqueue(struct Queue *queue, struct job * j, pthread_mutex_t lock, struct threadName threadName){
	//struct job *j = (struct job*)malloc(sizeof(struct job));
	int tid;
	pthread_mutex_lock(&lock);	// lock depending on the type of lock variable.	
	//printStatus();
	struct node *n = (struct node *)malloc(sizeof(struct node));
	tid =1;
//	tid = gettid();
	printf("Thread %s has acquired the %s queue...\n", threadToString(threadName), queue->name);
	n->job = j;
	n->next = NULL;
	if(queue-> front == NULL){
		queue->front = n;		
		queue->rear = queue->front;
	}
	else{
		queue->rear->next = n;
		queue->rear = queue->rear->next;
	}
	queue->size = queue->size + 1;
	printf("Thread %s has realeased  the  %s queue...\n", threadToString(threadName), queue->name);
	pthread_mutex_unlock(&lock);
}

struct node * dequeue(struct Queue *queue, pthread_mutex_t lock, struct threadName * threadName){
	pthread_mutex_lock(&lock);
	//printStatus();
		if(queue->front == NULL){
		printf("it is NULL\n");
				return NULL;
		}
		
		printf("\n Thread %s is dequeuing from the Queue %s\n",  threadName->threadName, queue->name);
		struct node *temp  = queue->front;
		queue->front = queue->front->next;
		temp->next = NULL;
		queue->size = queue->size - 1;
 		printf("The size of the %s queue is: %d\n", queue->name, queue->size);
	pthread_mutex_unlock(&lock);
	return temp;
}

int getTimeToCompletion(struct job *job){
//	return 20;	
	if(job->is_completed)
		return -1;
	int index = job->phaseTypeToExecute == CPU ? job->cpuSliceCount : job->ioSliceCount;
	printf("retrieve the value at: %d\n\n", index);
	return job->phases_and_times[job->phaseTypeToExecute][index];
}

int checkIfJobHasFinished(struct job *job){
	return job->is_completed;
}

void updateJobStatus(struct job *job){
	
		if(job->phaseTypeToExecute == CPU){
		++job->cpuSliceCount;	 // this is zero based indexed.
		
	}
	else{
		++job->ioSliceCount;
		
	}
	job->phaseTypeToExecute = job->phaseTypeToExecute == CPU ? IO : CPU;
	if((job->cpuSliceCount) == NR_SLICES && (job->ioSliceCount)== NR_SLICES)
	{
		job->is_completed = 1;
	}

}


int getQueueSize(struct Queue *queue, pthread_mutex_t  queueLock){
	int  size = 0;
	pthread_mutex_lock(&queueLock);
	size = queue->size;
	pthread_mutex_unlock(&queueLock);
	return size;
}

void *cpuThreadDelegate(void *args){
	struct threadName  * threadName = (struct threadName *)args;
	struct node *node = NULL;
	struct job * jobToRun;
	int currentThreadId = 0;
	char ch[10];
	while(1){
		if(getQueueSize(runningQueue, lock_runningQueue) == 0){
			printf("running queue is empty\n");
			printf("\nenter something\n");
			scanf("%s", ch);
			continue;
		}
	// Run the thread delegate all  the time....
	printf("%s has gained control of the CPU....\n", threadToString(*threadName));	
	
//	printStatus();
	// improve this.
//	if(runningQueue->size == 0){
		// we need to wait until a job has been added to the queue/
//		pthread_cond_wait(&runningQueueIsNotEmptyCondition, &lock_runningQueue);
//	}
	printf(" %s is running.....\n", threadToString(*threadName));
		// keep waiting until there is stuff in the queue.
	node = dequeue(runningQueue, lock_runningQueue, (void *)args);
	jobToRun = node->job;
	// run the job.
	int timeToCompletion = getTimeToCompletion(jobToRun);
	// run the job.
	// thread should be consuming CUP  cycles.
	
	//currentThreadId = gettid();
	printf("Thread %s has a job to run...\n",  threadToString(*threadName));
	jobToRun->jobState = RUNNING;
	while(timeToCompletion--);
	updateJobStatus(jobToRun);
	printf("Thread %s has finished the slice of the job at ready queue...\n", threadToString(*threadName));
	// check what to do with this job
	if(checkIfJobHasFinished(jobToRun)){
		// ADD IT TO THE FINISHED Queue
		jobToRun->jobState = COMPLETE;
		enqueue(finishedQueue, jobToRun, lock_runningQueue,*threadName);	
	}
	else{
		// check if IO
		
		printf("Thread: %s realized that the job is incomplete\n", threadToString(*threadName));
		if(checkIfPhaseTypeToExecuteIsIO(jobToRun)){
		// add to the IO Queue.
		jobToRun->jobState = BLOCKED;
		printf("Thread %s added the io Phase to the  IO queue", threadToString(*threadName));
			enqueue(ioQueue,jobToRun, lock_ioQueue, *threadName);				
		}
		else{
			// this has CPU phase.
			jobToRun->jobState = READY;
			printf("Thread %s added the CPU Phase to the running queue", threadToString(*threadName));
			enqueue(runningQueue,jobToRun, lock_runningQueue, *threadName);
		}
	}
	printf("Thread %s has finished running, and running Size = %d and ioQueueSize = %d!\n",threadToString(*threadName), runningQueue->size, 
					ioQueue->size);
 }
	
}

void ioThreadDelegate(void *args){
	char ch[10];
	struct threadName  *threadName = (struct threadName *)args;
	while(1){
	
	if(getQueueSize(ioQueue, lock_ioQueue) == 0 && getQueueSize(runningQueue, lock_runningQueue)){
		printf("All jobs have been depleted");
		scanf("%s", ch);
	}
	
	
	if(getQueueSize(ioQueue, lock_ioQueue) == 0){
		printf("IOQueue is empty... keep waiting");
		scanf("%s", ch);
		//printStatus(lock_runningQueue);
		continue;
	}	
		while(ioQueue->size == 0){
			printf("IO Thread is waiting for the IO Queue to fill up\n");
		}
			struct job *jobToRun = (dequeue(ioQueue, lock_ioQueue, (void *)args))->job;
		int timeToCompletion =  getTimeToCompletion(jobToRun);
		jobToRun->jobState = RUNNING;
		while(timeToCompletion--);
		updateJobStatus(jobToRun);
		if(checkIfJobHasFinished(jobToRun)){
		// ADD IT TO THE FINISHED Queue
		printf("CPU Thread has realized that the job is complete...");
		printf("\n add it to the finished queue\n");
		jobToRun->jobState = COMPLETE;
		enqueue(finishedQueue, jobToRun, lock_runningQueue,*threadName);
		scanf("%s", ch);	
	}
	else{
		// check if IO
		printf("Thread: %s realized that the job is incomplete\n", threadToString(*threadName));
		
		if(checkIfPhaseTypeToExecuteIsIO(jobToRun)){
		// add to the IO Queue.
		
		jobToRun->jobState = BLOCKED;
		printf("Thread %s added the io Phase to the  IO queue", threadToString(*threadName));
			enqueue(ioQueue,jobToRun, lock_ioQueue, *threadName);				
		}
		else{
			// this has CPU phase.
			printf("Thread %s added the CPU Phase to the running queue", threadToString(*threadName));
			jobToRun->jobState = READY;
			enqueue(runningQueue,jobToRun, lock_runningQueue, *threadName);
		}
	}
	printf("Thread %s has finished running!\n", threadToString(*threadName));
	}
}

void *finishingThreadDelegate(void *args){
	// add a job to 0the queue in every 3 seconds or so.
	// monitor the finished queue.
	int tid;
	//printStatus();
	struct threadName * threadName = (struct threadName *)args;
	struct job * jobToRun = createJob();
	enqueue(runningQueue, jobToRun,lock_runningQueue,*threadName);
	printf("\nFinished Queue: %s added a job\n", threadToString(*threadName));
	// while(getQueueSize(finishedQueue, lock_finishedQueue)!=0){
	// 	printf("removing job");
	// 	dequeue(finishedQueue, lock_finishedQueue, (void *)&threadName);
	// }

}


void initQueues(){
	runningQueue = (struct Queue*)malloc(sizeof(struct Queue));
	runningQueue->name = (char *)malloc(sizeof(char) * strlen("Running Queue") + 1);
	strcpy(runningQueue->name, "Running Queue\0");
//	runningQueue->name[strlen("Running Queue") + 1] = "\0";
	runningQueue->size = 0;
	ioQueue = (struct Queue*)malloc(sizeof(struct Queue));
	
		ioQueue->name = (char *)malloc(sizeof(char) * strlen("IO Queue") + 2);
	strcpy(ioQueue->name, "IO Queue\0");
	ioQueue->size = 0;
	//ioQueue->name[strlen("IO Queue") + 1] = "\0";
	
	finishedQueue = (struct Queue*)malloc(sizeof(struct Queue));
	
		finishedQueue->name = (char *)malloc(sizeof(char) * strlen("Finished Queue") + 1);
	strcpy(finishedQueue->name, "Finished Queue\0");
	finishedQueue->size = 0;
	//finishedQueue->name[strlen("Finished Queue") + 1] = "\0";
}


void generateJobsForReadyQueue(pthread_t threads[], int numberOfFinishedThreads){
	
	int i = 0;
	struct threadName finishThreadNames[numberOfFinishedThreads];
	for(i = 0; i < numberOfFinishedThreads; i++){
		setThreadName(&finishThreadNames[i], "Finish Thread ", i);
		pthread_create(&threads[i], NULL, (void *)&finishingThreadDelegate, (void *)&finishThreadNames[i]);
	}		
	for(i = 0; i < numberOfFinishedThreads; i++){
		pthread_join(threads[i], NULL);
	}
}

void initLocks(){
	
	if (pthread_mutex_init(&lock_runningQueue, NULL) != 0)
    {
        printf("\n mutex init failed\n");
    	exit(0);
	}
}


void initCPUThreads(pthread_t cpuThreads[], int numberOfCpuThreads){
	int i = 0;
	char num[10];
	char *result;
	struct threadName cpuThreadNames[numberOfCpuThreads];
	for(i = numberOfCpuThreads - 1; i >=0 ; i--){
		setThreadName(&cpuThreadNames[i],"CPU Thread ", i);
		result = threadToString(cpuThreadNames[i]);
		printf("Set the name of the thread to: %s \n", result);
		pthread_create(&cpuThreads[i], NULL, (void *)&cpuThreadDelegate, (void *)&cpuThreadNames[i]);
	}	
}



void initIOThreads(pthread_t ioThreads[], int numberOfIOThreads){
	int i = 0;
	char* result;
	struct threadName ioThreadNames[numberOfIOThreads];
	for(i = numberOfIOThreads - 1; i >=0 ; i--){
		setThreadName(&ioThreadNames[i],"IO Thread ", i);
		result =  threadToString(ioThreadNames[i]);
		printf("Set the name of the thread to: %s", result);
		pthread_create(&ioThreads[i], NULL, (void *)&ioThreadDelegate, (void *)&ioThreadNames[i]);
	}	
}

int main(){
	int num1 = 1, num2 = 2;
	int i = 0;
	char ch[10];
	pthread_t finishThreads[4];
	pthread_t cpuThreads[8];
	pthread_t ioThreads[2];
	struct threadName threadName;
	initQueues();
	initLocks();
	// join all of the ready queues.
	generateJobsForReadyQueue(finishThreads, 4);
	printf("Queue size = %d\n",  runningQueue->size);
	initCPUThreads(cpuThreads, 2);
	initIOThreads(ioThreads, 2);
	//printStatus();
	
	for(i = 0; i < 4; i++){
		pthread_join(finishThreads[i], NULL);	
	} 
	for(i = 0; i < 2; i++){
		pthread_join(cpuThreads[i], NULL);
	}
	for(i = 0; i < 2; i++){
		pthread_join(ioThreads[i], NULL);
	}
	/* while(runningQueue->size!=0){
		 		struct node *temp = dequeue(runningQueue, lock_runningQueue);
		printf("size = %d\n", runningQueue->size);
		printf("The value is: %d", temp->job->jobNumber);
	}
	*/
	//printStatus(lock_runningQueue);
	return 0;
	
	
/*
	struct job *job = createJob();
	while(!job->is_completed){
		printf("job type = %d \n", job->phaseTypeToExecute);
		printf("\njob needs time = %d	\n", getTimeToCompletion(job));
		updateJobStatus(job);
		printf("\nContinue? \n");
		scanf("%s", ch);
	}
	*/
	/*
	pthread_create(&threads[0], NULL, (void *)&finishingThreadDelegate, (void *)&num1);
	pthread_create(&threads[1], NULL, (void *)&finishingThreadDelegate, (void *)&num2);
	pthread_join(threads[0], NULL);
	pthread_join(threads[1], NULL);
	pthread_create(&threads[0], NULL, (void *)&cpuThreadDelegate, (void *)&num1);
	pthread_create(&threads[1], NULL, (void *)&cpuThreadDelegate, (void *)&num2);
	pthread_join(threads[0], NULL);
	pthread_join(threads[1], NULL);
	*/
// 	pthread_t finishingThreads[4];
//	 pthread_t runningThreads[];
//	 pthread_t[] 
/*
	pthread_t threads[2];
	
	printf("entre the number");
	int i = 0;
	runningQueue = (struct Queue *)malloc(sizeof(struct Queue));
	runningQueue->size = 0;
	for(i = 0; i < 10; i++){
		printf("\nEnqueued: %d\n", i);
		struct job *j = createJob();
		printf("\nThe job is: %d\n", j->jobNumber);
		enqueue(runningQueue, j);
	}
	printf("\n");
	printf("top most element is: \n");
	printf("%d\n", runningQueue->front->job->jobNumber);
	printf("The size of the queue is: %d\n", runningQueue->size);	
	while(runningQueue->size!=0){
		struct node *temp = dequeue(runningQueue);
		printf("The value is: %d", temp->job->jobNumber);
	}
	printf("queue is empty");
	*/
	
	return 0;
}
