#include <stdlib.h>
#include <stdio.h>
#include "threadpool.h"


/**Create ThreadPool**/
threadpool* create_threadpool(int num_threads_in_pool){
    if(!(num_threads_in_pool > 0 && num_threads_in_pool < MAXT_IN_POOL)){
        printf("number of threads invalid or passing the limit\n");
        return NULL;
    }
    threadpool *th_p = (threadpool*)calloc(1, sizeof(threadpool));
    if(th_p == NULL){
        perror("error while allocating memory\n");
        return NULL;
    }
    //initializing values.
    th_p->num_threads = num_threads_in_pool;
    th_p->qsize = 0;
    th_p->threads = (pthread_t*) calloc(num_threads_in_pool, sizeof(pthread_t));
    if(th_p->threads == NULL){
        perror("error while allocating memory\n");
        free(th_p);
        return NULL;
    }
    th_p->qhead = th_p->qtail = NULL;
    int mutex_val = pthread_mutex_init(&(th_p->qlock), NULL);
    if(mutex_val != 0){
        perror("mutex initializing failed\n");
        free(th_p->threads);
        free(th_p);
        return NULL;
    }
    int cond_val1 = pthread_cond_init(&(th_p->q_not_empty), NULL);
    if(cond_val1 != 0){
        perror("condition initializing failed\n");
        pthread_mutex_destroy(&(th_p->qlock));
        free(th_p->threads);
        free(th_p);
        return NULL;
    }
    int cond_val2 = pthread_cond_init(&(th_p->q_empty), NULL);
    if(cond_val2 != 0){
        perror("condition initializing failed\n");
        pthread_mutex_destroy(&(th_p->qlock));
        pthread_cond_destroy(&(th_p->q_empty));
        free(th_p->threads);
        free(th_p);
        return NULL;
    }
    th_p->shutdown = 0;
    th_p->dont_accept = 0;
    //create threads.
    int create_val;
    for (int i = 0; i < th_p->num_threads; i++) {
        create_val = pthread_create((th_p->threads)+i, NULL, do_work, (void *)th_p);
        if(create_val != 0){
            perror("condition initializing failed\n");
            pthread_mutex_destroy(&(th_p->qlock));
            pthread_cond_destroy(&(th_p->q_empty));
            pthread_cond_destroy(&(th_p->q_not_empty));
            free(th_p->threads);
            free(th_p);
            return NULL;
        }
    }
    return th_p;
}
/**Dispatch**/
void dispatch(threadpool* from_me, dispatch_fn dispatch_to_here, void *arg){
    if(from_me == NULL)
        return;
    pthread_mutex_lock(&(from_me->qlock));
    if(from_me->dont_accept == 1) return;// if thread_pool isn't accept so continue to the next request.
    pthread_mutex_unlock(&(from_me->qlock));
    //create a new work.
    work_t *work = (work_t*) calloc(1, sizeof(work_t));
    if(work == NULL){
        perror("error while allocating memory\n");
        return;
    }
    //initializing values.
    work->routine = dispatch_to_here;
    work->arg = arg;
    work->next = NULL;

    pthread_mutex_lock(&(from_me->qlock));
    if(from_me->dont_accept == 1){//go to the next request.
        free(work);
        return;
    }
    if(from_me->qsize == 0){//add work to the queue.
        from_me->qhead = work;
        from_me->qtail = work;
    } else {
        from_me->qtail->next = work;
        from_me->qtail = from_me->qtail->next;
    }
    from_me->qsize++;
    if(pthread_cond_signal(&(from_me->q_not_empty)) != 0) {//send signal that there are work to do.
        free(work);
        return;
    }
    pthread_mutex_unlock(&(from_me->qlock));
}
/**Do Work**/
void* do_work(void* p){
    threadpool *th_p = (threadpool*)p;
    pthread_mutex_lock(&(th_p->qlock));
    while(1){
        if(th_p->dont_accept != 0){
            pthread_mutex_unlock(&(th_p->qlock));
            pthread_exit(NULL);
        }
        if(th_p->qsize == 0) {//wait until there are work to do.
            pthread_cond_wait(&th_p->q_not_empty, &th_p->qlock);
        }
        if(th_p->shutdown != 0){//check shutdown another time.
            pthread_mutex_unlock(&th_p->qlock);
            pthread_exit(NULL);
        }
        work_t *temp = th_p->qhead;
        if(temp == NULL){//continue until there are a work to do.
            pthread_mutex_unlock(&th_p->qlock);
            continue;
        }
        th_p->qhead = temp->next;
        th_p->qsize--;
        if(th_p->qsize == 0 && th_p->dont_accept != 0){//send signal that there are no works.
            pthread_cond_signal(&(th_p->q_empty));
        }
        if (temp->routine(temp->arg) < 0)//dispatch function.
            printf("ERROR in Request.\n");
        printf("Work Done.\n");
        free(temp);
        pthread_mutex_unlock(&(th_p->qlock));
    }
}
/**Destroy ThreadPool**/
void destroy_threadpool(threadpool* destroyme){
    if(destroyme == NULL)
        return;
    pthread_mutex_lock(&(destroyme->qlock));
    destroyme->dont_accept = 1;
    while (destroyme->qsize)
        pthread_cond_wait(&(destroyme->q_empty),&(destroyme->qlock));
    destroyme->shutdown = 1;
    pthread_cond_broadcast(&(destroyme->q_not_empty));
    pthread_mutex_unlock(&(destroyme->qlock));
    void *status;
    for (int i = 0; i < destroyme->num_threads; i++)
        pthread_join(destroyme->threads[i], &status);
    
    pthread_mutex_destroy(&(destroyme->qlock));
    pthread_cond_destroy(&(destroyme->q_empty));
    pthread_cond_destroy(&(destroyme->q_not_empty));
    free(destroyme->threads);
    free(destroyme);
}