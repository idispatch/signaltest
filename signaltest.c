#include <errno.h>
#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <pthread.h>

#define NUM_THREADS 4

pthread_t threads[NUM_THREADS];

// waits for signals specified in signals array of length length
static int wait_signals(const int signals[], size_t length){
    int rc;
    int sig;
    sigset_t s;
    sigemptyset(&s);
    for(;length > 0; length--) {
        sigaddset(&s, signals[length - 1]);
    }

    sig = SIGHUP;
    while(sig == SIGHUP){
        rc = sigwait(&s, &sig);
        if(rc == 0) {
            fprintf(stdout, "%s: signal=%d in thread=%d\n", __FUNCTION__, sig, (int)pthread_self());
            fflush(stdout);
        } else {
            fprintf(stderr, "%s: thread=%d, error: %s (%d)\n", __FUNCTION__, (int)pthread_self(), strerror(errno), errno);
            break;
        }
    }
    return rc;
}

// dummy signal handler
static void signal_handler(int signo, siginfo_t *info, void *context) {
    char message[200];
    // no floating point in snpritnf format string - so it is safe to call
    snprintf(message, sizeof(message), "signal_handler: received signal=%d (thread=%d)\n", signo, (int)pthread_self());
    write(1, message, strlen(message));
}

// creates the signal mask for the calling thread
// blocks all signals on the current thread except signals in allowed_signals array of given length
static int setup_signal_mask_for_current_thread(const int allowed_signals[], size_t length) {
    int rc;
    sigset_t signal_set;
    sigfillset(&signal_set);
    for(;length > 0; --length) {
        sigdelset(&signal_set, allowed_signals[length - 1]);
    }
    rc = pthread_sigmask(SIG_SETMASK, &signal_set, NULL);
    if(rc != 0) {
        fprintf(stderr, "%s: error: %s (%d)\n", __FUNCTION__, strerror(errno), errno);
    }
    return rc;
}

// initializes the process signal handlers and main thread signal mask
static int setup_signals() {
    int rc;
    struct sigaction sa;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_SIGINFO;
    sa.sa_handler = NULL;
    sa.sa_sigaction = signal_handler;

    const int signals[] = {SIGQUIT, SIGTERM, SIGHUP, SIGINT, SIGPIPE, SIGUSR1};
    size_t length = sizeof(signals)/sizeof(signals[0]);
    for(;length > 0; length--) {
        rc = sigaction(signals[length - 1], &sa, NULL);
        if(rc != 0) {
            fprintf(stderr, "%s: failed to initialize signal %d: %s (%d)\n", __FUNCTION__, signals[length - 1], strerror(errno), errno);
            return rc;
        }
    }

    const int allowed_signals[] = {SIGQUIT, SIGTERM, SIGHUP, SIGINT};
    rc = setup_signal_mask_for_current_thread(allowed_signals, sizeof(allowed_signals)/sizeof(allowed_signals[0]));
    if(rc != 0) {
        return rc;
    }

    fprintf(stdout, "%s: thread %d set up signal mask\n", __FUNCTION__, (int)pthread_self());
    fflush(stdout);
    return 0;
}

// test thread that is allowed to intercept only SIGUSR1 signal
static void* thread_test(void * arg) {
    int rc;
    fprintf(stdout, "%s: thread started: %d\n", __FUNCTION__, (int)pthread_self());
    fflush(stdout);

    const int allowed_signals[] = {SIGUSR1};
    rc = setup_signal_mask_for_current_thread(allowed_signals, sizeof(allowed_signals)/sizeof(allowed_signals[0]));
    if(rc != 0) {
        pthread_exit((void*)rc);
        return (void*)rc;
    }

    // Wait for any signal
#if 0
    const int signals[] = {SIGUSR1};
    wait_signals(signals, 1);
#else
    fd_set fds;
    FD_ZERO(&fds);
    rc = select(1, &fds, &fds, &fds, NULL);
    if(rc == -1) {
        switch(errno) {
            case EINTR:
                break;
            default:
                fprintf(stderr, "%s: select error: %s (%d)\n", __FUNCTION__, strerror(errno), errno);
                break;
        }
    }
#endif
    fprintf(stdout, "%s: thread exiting: %d\n", __FUNCTION__, (int)pthread_self());
    fflush(stdout);
    pthread_exit(0);
    return NULL;
}

// start all threads
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

// stop all threads
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
    // this signals are allowed to be intercepted by the main thread
    const int signals[] = {SIGQUIT,SIGTERM,SIGHUP,SIGINT};
    if(setup_signals() != 0) {
        return 1;
    }

    printf("Test started (main thread=%d)\n", (int)pthread_self());

    start_threads(); // start all threads

    // main thread now will wait for Ctrl-C (SIGINT) or SIGTERM or SIGQUIT
    // it will ignore SIGHUP signal
    wait_signals(signals, sizeof(signals)/sizeof(signals[0]));

    stop_threads(); // stop all threads

    return 0;
}
