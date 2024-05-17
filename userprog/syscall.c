#include "userprog/syscall.h"

#include <stdio.h>
#include <syscall-nr.h>

#include "intrinsic.h"
#include "threads/flags.h"
#include "threads/interrupt.h"
#include "threads/loader.h"
#include "threads/thread.h"
#include "userprog/gdt.h"

/** #Project 2: System Call */
#include <string.h>

#include "filesys/file.h"
#include "filesys/filesys.h"
#include "threads/mmu.h"
#include "threads/palloc.h"
#include "userprog/process.h"
/** -----------------------  */

void syscall_entry(void);
void syscall_handler(struct intr_frame *);

/** #Project 2: System Call */
struct lock filesys_lock;  // 파일 읽기/쓰기 용 lock

/* System call.
 *
 * Previously system call services was handled by the interrupt handler
 * (e.g. int 0x80 in linux). However, in x86-64, the manufacturer supplies
 * efficient path for requesting the system call, the `syscall` instruction.
 *
 * The syscall instruction works by reading the values from the the Model
 * Specific Register (MSR). For the details, see the manual. */

#define MSR_STAR         0xc0000081 /* Segment selector msr */
#define MSR_LSTAR        0xc0000082 /* Long mode SYSCALL target */
#define MSR_SYSCALL_MASK 0xc0000084 /* Mask for the eflags */

void syscall_init(void) {
    write_msr(MSR_STAR, ((uint64_t)SEL_UCSEG - 0x10) << 48 | ((uint64_t)SEL_KCSEG) << 32);
    write_msr(MSR_LSTAR, (uint64_t)syscall_entry);

    /* The interrupt service rountine should not serve any interrupts
     * until the syscall_entry swaps the userland stack to the kernel
     * mode stack. Therefore, we masked the FLAG_FL. */
    write_msr(MSR_SYSCALL_MASK, FLAG_IF | FLAG_TF | FLAG_DF | FLAG_IOPL | FLAG_AC | FLAG_NT);

    /** #Project 2: System Call - read & write 용 lock 초기화 */
    lock_init(&filesys_lock);
}

/* The main system call interface */
/** #Project 2: System Call - 시스템 콜 핸들러 */
void syscall_handler(struct intr_frame *f UNUSED) {
    // TODO: Your implementation goes here.
    int sys_number = f->R.rax;

    // Argument 순서
    // %rdi %rsi %rdx %r10 %r8 %r9

    switch (sys_number) {
        case SYS_HALT:
            halt();
            break;
        case SYS_EXIT:
            exit(f->R.rdi);
            break;
        case SYS_FORK:
            f->R.rax = fork(f->R.rdi);
            break;
        case SYS_EXEC:
            f->R.rax = exec(f->R.rdi);
            break;
        case SYS_WAIT:
            f->R.rax = process_wait(f->R.rdi);
            break;
        case SYS_CREATE:
            f->R.rax = create(f->R.rdi, f->R.rsi);
            break;
        case SYS_REMOVE:
            f->R.rax = remove(f->R.rdi);
            break;
        case SYS_OPEN:
            f->R.rax = open(f->R.rdi);
            break;
        case SYS_FILESIZE:
            f->R.rax = filesize(f->R.rdi);
            break;
        case SYS_READ:
            f->R.rax = read(f->R.rdi, f->R.rsi, f->R.rdx);
            break;
        case SYS_WRITE:
            f->R.rax = write(f->R.rdi, f->R.rsi, f->R.rdx);
            break;
        case SYS_SEEK:
            seek(f->R.rdi, f->R.rsi);
            break;
        case SYS_TELL:
            f->R.rax = tell(f->R.rdi);
            break;
        case SYS_CLOSE:
            close(f->R.rdi);
            break;
        case SYS_DUP2:
            f->R.rax = dup2(f->R.rdi, f->R.rsi);
            break;
        case SYS_MMAP:
            f->R.rax = mmap(f->R.rdi, f->R.rsi, f->R.rdx, f->R.r10, f->R.r8);
            break;
        case SYS_MUNMAP:
            munmap(f->R.rdi);
            break;
        default:
            exit(-1);
    }
}

#ifndef VM
/** #Project 2: System Call */
void check_address(void *addr) {
    if (is_kernel_vaddr(addr) || addr == NULL || pml4_get_page(curr->pml4, addr) == NULL)
        exit(-1);
}
#else
/** #Project 3: Anonymous Page */
struct page *check_address(void *addr) {
    thread_t *curr = thread_current();

    if (is_kernel_vaddr(addr) || addr == NULL)
        exit(-1);

    return spt_find_page(&curr->spt, addr);
}
#endif

/** #Project 2: System Call - Halt */
void halt(void) {
    power_off();
}

/** #Project 2: System Call - Exit */
void exit(int status) {
    thread_t *curr = thread_current();
    curr->exit_status = status;

    /** #Project 2: Process Termination Messages */
    printf("%s: exit(%d)\n", curr->name, curr->exit_status);

    thread_exit();
}

/** #Project 2: System Call - Fork */
pid_t fork(const char *thread_name) {
    check_address(thread_name);

    return process_fork(thread_name, NULL);
}

/** #Project 2: System Call - Execute */
int exec(const char *cmd_line) {
    check_address(cmd_line);

    off_t size = strlen(cmd_line) + 1;
    char *cmd_copy = palloc_get_page(PAL_ZERO);

    if (cmd_copy == NULL)
        return -1;

    memcpy(cmd_copy, cmd_line, size);

    if (process_exec(cmd_copy) == -1)
        return -1;

    return 0;  // process_exec 성공시 리턴 값 없음 (do_iret)
}

/** #Project 2: System Call - Wait */
int wait(pid_t tid) {
    return process_wait(tid);
}

/** #Project 2: System Call - Create File */
bool create(const char *file, unsigned initial_size) {
    check_address(file);

    return filesys_create(file, initial_size);
}

/** #Project 2: System Call - Remove File */
bool remove(const char *file) {
    check_address(file);

    return filesys_remove(file);
}

/** #Project 2: System Call - Open File */
int open(const char *file) {
    check_address(file);

    struct file *newfile = filesys_open(file);

    if (newfile == NULL)
        return -1;

    int fd = process_add_file(newfile);

    if (fd == -1)
        file_close(newfile);

    return fd;
}

/** #Project 2: System Call - Get Filesize */
int filesize(int fd) {
    struct file *file = process_get_file(fd);

    if (file == NULL)
        return -1;

    return file_length(file);
}

/** #Project 2: System Call - Read File */
int read(int fd, void *buffer, unsigned length) {
    check_address(buffer);

    thread_t *curr = thread_current();
    struct file *file = process_get_file(fd);

    if (file == NULL || file == STDOUT || file == STDERR)  // 빈 파일, stdout, stderr를 읽으려고 할 경우
        return -1;

    if (file == STDIN) {  // stdin -> console로 직접 입력
        int i = 0;        // 쓰레기 값 return 방지
        char c;
        unsigned char *buf = buffer;

        for (; i < length; i++) {
            c = input_getc();
            *buf++ = c;
            if (c == '\0')
                break;
        }

        return i;
    }

    // 그 외의 경우
    off_t bytes = -1;

    lock_acquire(&filesys_lock);
    bytes = file_read(file, buffer, length);
    lock_release(&filesys_lock);

    return bytes;
}

/** #Project 2: System Call - Write File */
int write(int fd, const void *buffer, unsigned length) {
    check_address(buffer);

    thread_t *curr = thread_current();
    off_t bytes = -1;

    struct file *file = process_get_file(fd);

    if (file == STDIN || file == NULL)  // stdin에 쓰려고 할 경우
        return -1;

    if (file == STDOUT || file == STDERR) {  // 1(stdout) & 2(stderr) -> console로 출력
        putbuf(buffer, length);
        return length;
    }

    lock_acquire(&filesys_lock);
    bytes = file_write(file, buffer, length);
    lock_release(&filesys_lock);

    return bytes;
}

/** #Project 2: System Call - Change Read Position */
void seek(int fd, unsigned position) {
    struct file *file = process_get_file(fd);

    if (file == NULL || (file >= STDIN && file <= STDERR))
        return;

    file_seek(file, position);
}

/** #Project 2: System Call - Get Read Position */
int tell(int fd) {
    struct file *file = process_get_file(fd);

    if (file == NULL || (file >= STDIN && file <= STDERR))
        return -1;

    return file_tell(file);
}

/** #Project 2: System Call - Close File */
void close(int fd) {
    thread_t *curr = thread_current();
    struct file *file = process_get_file(fd);

    if (file == NULL)
        return;

    process_close_file(fd);

    if (file >= STDIN && file <= STDERR) {
        file = 0;
        return;
    }

    if (file->dup_count == 0)
        file_close(file);
    else
        file->dup_count--;
}

/** #Project 2: Extend File Descriptor (Extra) */
int dup2(int oldfd, int newfd) {
    struct file *oldfile = process_get_file(oldfd);
    struct file *newfile = process_get_file(newfd);

    if (oldfile == NULL)
        return -1;

    if (oldfd == newfd)
        return newfd;

    if (oldfile == newfile)
        return newfd;

    close(newfd);

    newfd = process_insert_file(newfd, oldfile);

    return newfd;
}

/** Project 3: Memory Mapped Files - Memory Mapping */
void *mmap(void *addr, size_t length, int writable, int fd, off_t offset) {
    // addr은 페이지 시작 주소여야 함. length가 음수거나 쓰기 불가능한 영역도 불가능.
    if (check_address(addr) || pg_round_down(addr) != addr || length <= 0 || writable == 0 || offset % PGSIZE != 0)
        return NULL;

    struct file *file = process_get_file(fd);

    if ((file >= STDIN && file <= STDERR) || file == NULL)
        return NULL;

    return do_mmap(addr, length, writable, file, offset);
}

/** Project 3: Memory Mapped Files - Memory Unmapping */
void munmap(void *addr) {
}