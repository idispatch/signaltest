#include <errno.h>
#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <pthread.h>

#define NUM_THREADS 4

pthread_t threads[NUM_THREADS];


static int wait_signals(const int signals[], size_t length){
    int rc;
    int sig;
    sigset_t s;
    sigemptyset(&s);
    for(;length > 0; length--) {
        sigaddset(&s, signals[length - 1]);
    }
    while(1){
        rc = sigwait(&s, &sig);
        if(rc == 0) {
            fprintf(stdout, "%s: signal=%d in thread=%d\n", __FUNCTION__, sig, (int)pthread_self());
            fflush(stdout);
        } else {
            fprintf(stderr, "%s: thread=%d, error: %s (%d)\n", __FUNCTION__, (int)pthread_self(), strerror(errno), errno);
            break;
        }
        if(sig==SIGHUP)
            continue;
        else
            break;
    }
    return rc;
}

static int setup_signals() {
    int rc;
    struct sigaction sa;
    sigset_t s;

    sigfillset(&s);
    sigdelset(&s, SIGHUP);
    sigdelset(&s, SIGINT);
    sigdelset(&s, SIGTERM);
    sigdelset(&s, SIGQUIT);
    sigdelset(&s, SIGHUP);

    rc = pthread_sigmask(SIG_SETMASK, &s, NULL);
    if(rc != 0) {
        fprintf(stderr, "%s: error: %s (%d)\n", __FUNCTION__, strerror(errno), errno);
        return rc;
    }

    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sa.sa_handler = SIG_IGN;
    sa.sa_sigaction = NULL;

    rc = sigaction(SIGINT, &sa, NULL);
    if(rc != 0) {
        fprintf(stderr, "%s: error: %s (%d)\n", __FUNCTION__, strerror(errno), errno);
        return rc;
    }

    rc = sigaction(SIGTERM, &sa, NULL);
    if(rc != 0) {
        fprintf(stderr, "%s: error: %s (%d)\n", __FUNCTION__, strerror(errno), errno);
        return rc;
    }

    rc = sigaction(SIGQUIT, &sa, NULL);
    if(rc != 0) {
        fprintf(stderr, "%s: error: %s (%d)\n", __FUNCTION__, strerror(errno), errno);
        return rc;
    }

    rc = sigaction(SIGHUP, &sa, NULL);
    if(rc != 0) {
        fprintf(stderr, "%s: error: %s (%d)\n", __FUNCTION__, strerror(errno), errno);
        return rc;
    }

    fprintf(stdout, "%s: thread %d set up signal mask\n", __FUNCTION__, (int)pthread_self());
    fflush(stdout);
    return 0;
}

static int block_all_signals_except_SIGUSR1() {
    int rc;
    sigset_t set;
    sigfillset(&set);
    sigdelset(&set, SIGUSR1);

    rc = pthread_sigmask(SIG_SETMASK, &set, NULL);
    if(rc != 0) {
        fprintf(stderr, "%s: error: %s (%d)\n", __FUNCTION__, strerror(errno), errno);
        return rc;
    }

    struct sigaction sa;
    sa.sa_mask = 0;
    sa.sa_flags = 0;
    sa.sa_handler = SIG_IGN;
    sa.sa_sigaction = NULL;

    rc = sigaction(SIGUSR1, &sa, NULL);
    if(rc != 0) {
        fprintf(stderr, "%s: error: %s (%d)\n", __FUNCTION__, strerror(errno), errno);
        return rc;
    }

    fprintf(stdout, "%s: thread %d set up signal mask\n", __FUNCTION__, (int)pthread_self());
    fflush(stdout);
    return 0;
}

static void* thread_test(void * arg) {
    const int signals[] = {SIGUSR1};
    fprintf(stdout, "%s: thread started: %d\n", __FUNCTION__, (int)pthread_self());
    fflush(stdout);

    block_all_signals_except_SIGUSR1();

    wait_signals(signals, 1);

    fprintf(stdout, "%s: thread exiting: %d\n", __FUNCTION__, (int)pthread_self());
    fflush(stdout);
    pthread_exit(0);
    return NULL;
}

static void start_threads() {
    fprintf(stdout, "%s: starting %d threads\n", __FUNCTION__, NUM_THREADS);
    fflush(stdout);
    int i;
    int rc;
    for(i = 0; i < NUM_THREADS; ++i) {
        rc = pthread_create(&threads[i], NULL, thread_test, NULL);
        if(rc != 0) {
            fprintf(stderr, "%s: error: %s (%d)\n", __FUNCTION__, strerror(errno), errno);
        }
    }
    fprintf(stdout, "%s: started %d threads\n", __FUNCTION__, NUM_THREADS);
    for(i = 0; i < NUM_THREADS; ++i) {
        fprintf(stdout, "%s: thread[%d]=%d\n", __FUNCTION__, i, (int)threads[i]);
    }
    fflush(stdout);
}

static void stop_threads() {
    int i;
    int rc;
    void *rv;
    fprintf(stdout, "%s: stoping %d threads\n", __FUNCTION__, NUM_THREADS);
    fflush(stdout);
    for(i = 0; i < NUM_THREADS; ++i) {
        fprintf(stdout, "%s: sending signal %d to thread %d\n", __FUNCTION__, SIGUSR1, (int)threads[i]);
        fflush(stdout);
        rc  =pthread_kill(threads[i], SIGUSR1);
        if(rc != 0) {
            fprintf(stderr, "%s: error: %s (%d)\n", __FUNCTION__, strerror(errno), errno);
        } else {
            fprintf(stderr, "%s: signal %d sent to thread %d\n", __FUNCTION__, SIGUSR1, (int)threads[i]);
        }

        rc = pthread_join(threads[i], &rv);
        if(rc != 0) {
            fprintf(stderr, "%s: error: %s (%d)\n", __FUNCTION__, strerror(errno), errno);
        }
    }
    fprintf(stdout, "%s: stoped %d threads\n", __FUNCTION__, NUM_THREADS);
    fflush(stdout);
}



int main(int argc, char * argv[]) {
    const int signals[] = {SIGQUIT,SIGTERM,SIGHUP,SIGINT};
    if(setup_signals() != 0) {
        return 1;
    }

    printf("Test started (main thread=%d)\n", (int)pthread_self());

    start_threads();
    wait_signals(signals, sizeof(signals)/sizeof(signals[0]));
    stop_threads();

    fflush(stdout);
    return 0;
}
