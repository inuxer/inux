#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <assert.h>
#include <sys/types.h>

#include "thread-control.h"

#define DBGPRINTF_DEBUG printf
#define DBGPRINTF_ERROR printf
#define ASSERT  assert

typedef void*(*thread_task_func)(void* arg);

/*Task need be executed by thread*/
struct _thread_task_t
{
    int taskid;                     /*Task id.*/
    thread_task_func task_func;     /*Task function and it's arg*/
    void* task_arg;
};

typedef struct _thread_task_t thread_task_t;

/*Thread's status.*/
typedef enum
{
    ethread_status_unknown = 0,
    ethread_status_idle ,
    ethread_status_running ,
    ethread_status_terminel ,
    ethread_status_cannotuse ,
}thread_status_e;

/*Thread's parameter.*/
struct _thread_data_t
{
    int thread_id;
    pthread_t pid;
    thread_status_e status;

    thread_task_t thread_task;
    THREAD_CONTROL thread_control;
};
typedef struct _thread_data_t thread_data_t;

/*Thread-pool's parameter*/
struct _thread_pool_t
{
    thread_data_t* thread_data_set;
    int num_thread;
    int taskid_base;

    pthread_mutex_t thread_pool_lock;
};
typedef struct _thread_pool_t thread_pool_t;

thread_pool_t g_thread_pool;

/*Set thread's status*/
int 
thread_pool_setthreadsstatus(thread_data_t* thread_data, thread_status_e status)
{
    thread_pool_t* thread_pool = &g_thread_pool;
    pthread_mutex_lock(&(thread_pool->thread_pool_lock));

    thread_data->status = status;

    pthread_mutex_unlock(&(thread_pool->thread_pool_lock));

    return 0;
}


/*Thread's function.*/
void* 
thread_pool_func(void* arg)
{
    sleep(1);   //Wait pthread_t count.

    thread_data_t* thread_data = (thread_data_t*)arg;
    DBGPRINTF_DEBUG("Thread start run. Thread_id = %d, pid = 0x%x . \n", 
            thread_data->thread_id, (unsigned int)thread_data->pid); 

    /*
     Continue to wait the task, then based on new task_func and task_arg to perform this task.
      */
    while(1)
    {
        thread_control_wait(thread_data->thread_control);
        
        //Need to lock? Yes.
        thread_pool_setthreadsstatus(thread_data, ethread_status_running);
        DBGPRINTF_DEBUG("Task start. taskid = %d .\n", thread_data->thread_task.taskid);

        thread_data->thread_task.task_func(thread_data->thread_task.task_arg);

        DBGPRINTF_DEBUG("Task end. taskid = %d .\n", thread_data->thread_task.taskid);
        //Need to lock?Yes.
        thread_pool_setthreadsstatus(thread_data, ethread_status_idle);
    }

    DBGPRINTF_DEBUG("Thread end run. Thread_id = %d, pid = 0x%x . \n", 
            thread_data->thread_id, (unsigned int)thread_data->pid); 
}


int 
thread_task_init(thread_task_t* thread_task)
{

    thread_task->taskid             = -1;
    thread_task->task_func          = NULL;
    thread_task->task_arg           = NULL;

    return 0;
}


int 
thread_data_init(thread_data_t* thread_data)
{
    thread_data->thread_id = -1;
    thread_data->pid = 0x0;
    thread_data->status = ethread_status_unknown ,

    thread_task_init(&(thread_data->thread_task));
    thread_control_init(&(thread_data->thread_control));

    return 0;
}


int 
thread_pool_create(int num_thread)
{
    ASSERT(num_thread > 0 && num_thread <= 10*1024);
    thread_pool_t* thread_pool = &g_thread_pool;
    
    int i = 0;
    thread_pool->thread_data_set = (thread_data_t*)malloc(sizeof(thread_data_t) * num_thread);
    ASSERT(thread_pool->thread_data_set != NULL);
    thread_pool->num_thread = num_thread;
    thread_pool->taskid_base = -1;
    pthread_mutex_init(&(thread_pool->thread_pool_lock), NULL);

    for(i=0; i<num_thread; i++)
    {
        thread_data_t* thread_data = thread_pool->thread_data_set+i;
        thread_data_init(thread_data);
        thread_data->thread_id = i;
        thread_data->status = ethread_status_idle;

        /* pthread_create set to detached. */
        pthread_attr_t attr;
        pthread_attr_init(&attr);
        pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
        int ret = pthread_create(&(thread_data->pid), &attr, thread_pool_func, thread_data);
        if(ret != 0)
        {
            DBGPRINTF_DEBUG("pthread_create error[%d].\n", i);
            break;
        }
    }

    sleep(2);

    return 0;
}


void* 
test_func(void* arg)
{
    int t_sleep = (int)arg;
    DBGPRINTF_DEBUG("Test func. Sleep %d .\n", t_sleep);
    /*
    int a[2048*1024] = {0};
    int i = 0;
    for(i=0; i<2028*1024; i++)
    {
        a[i] = i*i;
    }
    DBGPRINTF_DEBUG("a[0]=%d. \n", a[0]);
    */
    sleep(t_sleep);

    DBGPRINTF_DEBUG("Test func finished. \n");

    return NULL;
}


/*Find out which thread is free.*/
int 
thread_pool_queryfree(thread_data_t** thread_data_found)
{
    *thread_data_found = NULL;
    thread_pool_t* thread_pool = &g_thread_pool;
    pthread_mutex_lock(&(thread_pool->thread_pool_lock));

    int i = 0;
    for(i=0; i<thread_pool->num_thread; i++)
    {
        thread_data_t* thread_data = thread_pool->thread_data_set+i;
        if(thread_data->status == ethread_status_idle)
        {
            *thread_data_found = thread_data;
            break;
        }
    }

    pthread_mutex_unlock(&(thread_pool->thread_pool_lock));

    return 0;
}

/*Get a new taskid.*/
int 
thread_pool_gettaskid(int* taskid)
{
    thread_pool_t* thread_pool = &g_thread_pool;
    pthread_mutex_lock(&(thread_pool->thread_pool_lock));

    thread_pool->taskid_base ++;
    *taskid = thread_pool->taskid_base;

    pthread_mutex_unlock(&(thread_pool->thread_pool_lock));

    return 0;
}

/*Add task to thread-pool.*/
int 
thread_pool_addtask(thread_task_func task_func, void* arg)
{
    /* Find a free thread. */
    thread_data_t* thread_data_found = NULL;
    thread_pool_queryfree(&thread_data_found);

    if(thread_data_found != NULL)
    {
        DBGPRINTF_DEBUG("Thread [%d] perferm this task.\n", thread_data_found->thread_id);

        /* Set task data. */
        thread_data_found->thread_task.task_func    = task_func;       
        thread_data_found->thread_task.task_arg     = arg;       
        thread_pool_gettaskid(&(thread_data_found->thread_task.taskid));

        /* Start the task. */
        thread_pool_setthreadsstatus(thread_data_found, ethread_status_running);
        thread_control_start(thread_data_found->thread_control);
        DBGPRINTF_DEBUG("Thread [%d] Add task[%d] finished.\n", 
                thread_data_found->thread_id, thread_data_found->thread_task.taskid);
    }
    else
    {
        DBGPRINTF_ERROR("Thread pool full. Task not added.\n");
    }

    return 0;
}




int 
main()
{
    thread_pool_create(10);
	
    thread_pool_addtask(test_func, (void*)(1<<0));
    thread_pool_addtask(test_func, (void*)(1<<1));
    thread_pool_addtask(test_func, (void*)(1<<2));
    thread_pool_addtask(test_func, (void*)(1<<3));
    thread_pool_addtask(test_func, (void*)(1<<4));
    thread_pool_addtask(test_func, (void*)(1<<5));
    thread_pool_addtask(test_func, (void*)(1<<6));
    thread_pool_addtask(test_func, (void*)(1<<7));

    sleep(6);
    thread_pool_addtask(test_func, (void*)(1<<0));
    thread_pool_addtask(test_func, (void*)(1<<1));
    thread_pool_addtask(test_func, (void*)(1<<2));
    thread_pool_addtask(test_func, (void*)(1<<3));
    thread_pool_addtask(test_func, (void*)(1<<4));
    thread_pool_addtask(test_func, (void*)(1<<5));
    thread_pool_addtask(test_func, (void*)(1<<6));
    thread_pool_addtask(test_func, (void*)(1<<7));

    sleep(100000);

    return 0;
}
