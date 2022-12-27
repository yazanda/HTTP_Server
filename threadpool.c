#include <stdlib.h>
#include <stdio.h>
#include "threadpool.h"

void addWork(threadpool*, work_t*);

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
    th_p->shutdown = th_p->dont_accept = 0;

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
    if(from_me->dont_accept == 1) return;
    pthread_mutex_unlock(&(from_me->qlock));

    work_t *work = (work_t*) calloc(1, sizeof(work_t));
    if(work == NULL){
        perror("error while allocating memory\n");
        return;
    }
    work->routine = dispatch_to_here;
    work->arg = arg;
    work->next = NULL;

    pthread_mutex_lock(&(from_me->qlock));
    if(from_me->dont_accept == 1){
        free(work);
        return;
    }
    addWork(from_me, work);
    if(pthread_cond_signal(&(from_me->q_not_empty)) != 0) {
        free(work);
        return;
    }
    pthread_mutex_unlock(&(from_me->qlock));
}
/**Do Work**/
void* do_work(void* p){
    threadpool *th_p = (threadpool*)p;
    while(1){
        pthread_mutex_lock(&(th_p->qlock));
        if(th_p->shutdown != 0){
            pthread_mutex_unlock(&(th_p->qlock));
            pthread_exit(NULL);
        }
        th_p->qsize--;
        work_t *temp = th_p->qhead;
        if (th_p->qsize == 0) {
            th_p->qhead = NULL;
            th_p->qtail = NULL;

            if (th_p->dont_accept != 0)
                pthread_cond_signal(&(th_p->q_empty));
        }
        else
            th_p->qhead = th_p->qhead->next;

        pthread_mutex_unlock(&(th_p->qlock));

        if (temp->routine(temp->arg) < 0)
            printf("Processing request failed\n");
        free(temp);
    }
    //pthread_exit(NULL);
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
//function adds a new work to thread pool's queue.
void addWork(threadpool* th_p, work_t* newWork){
    if(th_p->qsize == 0){
        th_p->qhead = newWork;
        th_p->qtail = newWork;
    } else {
        th_p->qtail->next = newWork;
        th_p->qtail = th_p->qtail->next;
    }
    th_p->qsize++;
}