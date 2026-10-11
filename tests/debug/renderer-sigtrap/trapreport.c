// Preloaded into the browser and its engine processes: on SIGTRAP, writes the
// faulting thread's stack to stderr as module+offset frames, then dies of the
// same signal so the browser sees the same termination status.
#define _GNU_SOURCE
#include <execinfo.h>
#include <link.h>
#include <signal.h>
#include <string.h>
#include <sys/syscall.h>
#include <ucontext.h>
#include <unistd.h>

static void put(const char *text) { write(2, text, strlen(text)); }

static void hex(unsigned long value)
{
    char buffer[19] = "0x";
    int length = 2;
    char digits[16];
    int count = 0;
    do {
        digits[count++] = "0123456789abcdef"[value & 15];
        value >>= 4;
    } while (value);
    while (count)
        buffer[length++] = digits[--count];
    write(2, buffer, length);
}

static void dec(long value)
{
    char digits[24];
    int count = 0;
    do {
        digits[count++] = '0' + value % 10;
        value /= 10;
    } while (value);
    char buffer[24];
    int length = 0;
    while (count)
        buffer[length++] = digits[--count];
    write(2, buffer, length);
}

static unsigned long wanted;

static int locate(struct dl_phdr_info *info, size_t size, void *data)
{
    (void)size;
    (void)data;
    for (int i = 0; i < info->dlpi_phnum; i++) {
        const ElfW(Phdr) *segment = &info->dlpi_phdr[i];
        if (segment->p_type != PT_LOAD)
            continue;
        unsigned long start = info->dlpi_addr + segment->p_vaddr;
        if (wanted >= start && wanted < start + segment->p_memsz) {
            put(info->dlpi_name[0] ? info->dlpi_name : "(main)");
            put("+");
            hex(wanted - info->dlpi_addr);
            return 1;
        }
    }
    return 0;
}

static void frame(int number, unsigned long address)
{
    put("trapreport #");
    dec(number);
    put(" ");
    hex(address);
    put(" ");
    wanted = address;
    if (!dl_iterate_phdr(locate, 0))
        put("?");
    put("\n");
}

static void report(int signal, siginfo_t *info, void *context)
{
    (void)info;
    ucontext_t *state = context;
    put("trapreport: signal ");
    dec(signal);
    put(" in pid ");
    dec(getpid());
    put(" tid ");
    dec(syscall(SYS_gettid));
    put("\n");
    frame(0, state->uc_mcontext.gregs[REG_RIP]);
    void *frames[64];
    int count = backtrace(frames, 64);
    for (int i = 0; i < count; i++)
        frame(i + 1, (unsigned long)frames[i]);
    put("trapreport: end\n");
    struct sigaction plain = {0};
    plain.sa_handler = SIG_DFL;
    sigaction(signal, &plain, 0);
    raise(signal);
}

__attribute__((constructor)) static void install(void)
{
    void *warm[1];
    backtrace(warm, 1); // loads libgcc_s before any sandbox closes the filesystem
    struct sigaction action = {0};
    action.sa_sigaction = report;
    action.sa_flags = SA_SIGINFO | SA_ONSTACK;
    sigaction(SIGTRAP, &action, 0);
}
