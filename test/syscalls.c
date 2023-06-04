#include <sys/stat.h>

void *_sbrk(int incr) { (void) incr; return (void *) -1; }
int _fstat(int fd, struct stat *st) { (void) fd, (void) st; return -1; }
int _stat(const char *path, struct stat *st) { (void) path, (void) st; return -1; }
int _open(const char *path) { (void) path; return -1; }
int _close(int fd) { (void) fd; return -1; }
int _isatty(int fd) { (void) fd; return 1; }
int _lseek(int fd, int ptr, int dir) { (void) fd, (void) ptr, (void) dir; return 0; }
void _exit(int status) { (void) status; for (;;) asm volatile("BKPT #0"); }
void _kill(int pid, int sig) { (void) pid, (void) sig; }
int _getpid(void) { return -1; }
int _write(int fd, char *ptr, int len) { (void) fd, (void) ptr, (void) len; return -1; }
int _read(int fd, char *ptr, int len) { (void) fd, (void) ptr, (void) len; return -1; }
int _link(const char *a, const char *b) { (void) a, (void) b; return -1; }
int _unlink(const char *a) { (void) a; return -1; }
int mkdir(const char *path, mode_t mode) { (void) path, (void) mode; return -1; }
void _init(void) {}
