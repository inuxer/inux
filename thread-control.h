#define THREAD_CONTROL  void*

int thread_control_init(THREAD_CONTROL* thread_control);
int thread_control_deinit(THREAD_CONTROL* thread_control);
int thread_control_wait(THREAD_CONTROL thread_control);
int thread_control_start(THREAD_CONTROL thread_control);
