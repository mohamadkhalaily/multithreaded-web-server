#include "segel.h"
#include "request.h"
#include <pthread.h>
#include "queue.h"
#include "stat_thread.h"
#define MAX_SCHED_NAME 7

// Function prototypes
void scheduleNextRequest(int queue_size, int connfd, char* sched_name, Request request);
void *workThread(void *arg);
void getargs(int *port, int argc, char *argv[], int *total_threads, char *sched_name, int *queue_size);

// Global variables
pthread_mutex_t queue_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t normal_cond = PTHREAD_COND_INITIALIZER;
pthread_cond_t block_cond = PTHREAD_COND_INITIALIZER;
int current_working_num_threads = 0;
Queue request_queue;

int main(int argc, char *argv[]) {
    int listenfd, connfd, port, clientlen;
    int total_thread_num, queue_size;
    struct sockaddr_in clientaddr;
    char sched_name[MAX_SCHED_NAME]; // 7 because random is biggest string;
    Request req;

    getargs(&port, argc, argv, &total_thread_num, sched_name, &queue_size);
    request_queue = createQueue(queue_size, copyRequest, destroyRequest);

    pthread_t *threads = malloc(sizeof(pthread_t) * total_thread_num);
    StatThread *sts = malloc(sizeof(struct statThread_t) * total_thread_num);
    for (int i = 0; i < total_thread_num; ++i) {
        sts[i] = createStatThread(i);
        pthread_create(&threads[i], NULL, workThread, sts[i]);
    }

    listenfd = Open_listenfd(port);
    while (1) {
        clientlen = sizeof(clientaddr);
        connfd = Accept(listenfd, (SA *) &clientaddr, (socklen_t *) &clientlen);
        Time arrive_time = malloc(sizeof(*arrive_time));
        gettimeofday(arrive_time, NULL);

        req = createRequest(connfd, arrive_time);

        pthread_mutex_lock(&queue_lock);
        scheduleNextRequest(queue_size, connfd, sched_name, req);
        pthread_cond_signal(&normal_cond);
        pthread_mutex_unlock(&queue_lock);
    }

    free(threads);
    free(sts);
    return 0;
}

void scheduleNextRequest(int queue_size, int connfd, char* sched_name, Request request) {
    Request tmp_request;
    if (current_working_num_threads + getSizeQueue(request_queue) < queue_size) {
        addElement(request_queue, request);
        return;
    }
    switch (*sched_name) {
        case 'r':
            if (!strcmp(sched_name, "random")) {
                if (isEmptyQueue(request_queue)) {
                    destroyReq(request,connfd);
                    return;
                }
                Queue deleted_vals = createQueue(queue_size, copyRequest, destroyRequest);
                Queue old_queue = request_queue;
                request_queue = removeHalfElementsRandomly(request_queue, deleted_vals);
                freeQueue(old_queue);
                addElement(request_queue, request);
                while (!isEmptyQueue(deleted_vals)) {
                    tmp_request = topElement(deleted_vals);
                    Close(getFdRequest(tmp_request));
                    dequeElement(deleted_vals);
                    destroyRequest(tmp_request);
                }
                freeQueue(deleted_vals);
                return;
            }
            break;

        case 'd':
            if (!strcmp(sched_name, "dh")) {
                if (isEmptyQueue(request_queue)) {
                    destroyReq(request,connfd);
                    return;
                }
                tmp_request = topElement(request_queue);
                Close(getFdRequest(tmp_request));
                dequeElement(request_queue);
                destroyRequest(tmp_request);
                addElement(request_queue, request);
                return;
            }
            if (!strcmp(sched_name, "dt")) {
                destroyReq(request,connfd);
                return;
            }
            break;

        case 'b':
            if (!strcmp(sched_name, "block")) {
                while (getSizeQueue(request_queue) + current_working_num_threads >= queue_size) {
                    pthread_cond_wait(&block_cond, &queue_lock);
                }
                addElement(request_queue, request);
                return;
            }
            break;
    }

}

void destroyReq(Request req,int connfd)
{
    destroyRequest(req);
    Close(connfd);
}

void *workThread(void *arg) {
    StatThread st = (StatThread) arg;
    Time received_time = malloc(sizeof(struct timeval));
    Request request;
    while(1) {
        pthread_mutex_lock(&queue_lock);
        while (isEmptyQueue(request_queue)) {
            pthread_cond_wait(&normal_cond, &queue_lock);
        }
        gettimeofday(received_time, NULL);
        request = topElement(request_queue);
        dequeElement(request_queue);

        increaseThreadCount(st);
        setDispatchRequest(request, received_time);
        requestSetThread(request, st);
        ++current_working_num_threads;
        pthread_mutex_unlock(&queue_lock);

        if (request != NULL) {
            requestHandle(request);
            Close(getFdRequest(request));
            destroyRequest(request);
        }

        pthread_mutex_lock(&queue_lock);
        --current_working_num_threads;
        pthread_cond_signal(&block_cond);
        pthread_mutex_unlock(&queue_lock);
    }
    return NULL;
}

// Parse command-line arguments
void getargs(int *port, int argc, char *argv[], int *total_threads, char *sched_name, int *queue_size) {
    if (argc != 5) {
        fprintf(stderr, "Usage: %s <port> <threads> <queue_size> <schedalg>\n", argv[0]);
        exit(1);
    }
    *port = atoi(argv[1]);
    *total_threads = atoi(argv[2]);
    *queue_size = atoi(argv[3]);
    strcpy(sched_name, argv[4]);
}