#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/loader.h"
#include "userprog/gdt.h"
#include "threads/flags.h"
#include "intrinsic.h"

#include "filesys/filesys.h"
#include "filesys/file.h"
#include "threads/palloc.h"
#include "threads/vaddr.h"
#include "userprog/process.h"
#include "threads/synch.h"

void syscall_entry (void);
void syscall_handler (struct intr_frame *);
void check_address(void *addr);
void halt(void);
void exit(int status);
bool create (const char *file , unsigned initial_size);
bool remove (const char *file);
pid_t fork (const char *thread_name);
int exec (const char *cmd_line);
int wait (pid_t pid);
int open (const char *file);
void close (int fd);
int filesize (int fd);
int read (int fd, void *buffer, unsigned size);
int write (int fd, const void *buffer, unsigned size);
void seek (int fd, unsigned position);
unsigned tell (int fd);
static struct file *find_file_by_fd(int fd);
int add_file_to_fdt(struct file *file);
void remove_file_from_fdt(int fd);

/* System call.
 *
 * Previously system call services was handled by the interrupt handler
 * (e.g. int 0x80 in linux). However, in x86-64, the manufacturer supplies
 * efficient path for requesting the system call, the `syscall` instruction.
 *
 * The syscall instruction works by reading the values from the the Model
 * Specific Register (MSR). For the details, see the manual. */

#define MSR_STAR 0xc0000081         /* Segment selector msr */
#define MSR_LSTAR 0xc0000082        /* Long mode SYSCALL target */
#define MSR_SYSCALL_MASK 0xc0000084 /* Mask for the eflags */

// ? : gitbook(open)에 0, 1 
const int STDIN = 1;
const int STDOUT = 2;

void
syscall_init (void) {
	write_msr(MSR_STAR, ((uint64_t)SEL_UCSEG - 0x10) << 48  |
			((uint64_t)SEL_KCSEG) << 32);
	write_msr(MSR_LSTAR, (uint64_t) syscall_entry);

	/* The interrupt service rountine should not serve any interrupts
	 * until the syscall_entry swaps the userland stack to the kernel
	 * mode stack. Therefore, we masked the FLAG_FL. */
	write_msr(MSR_SYSCALL_MASK,
			FLAG_IF | FLAG_TF | FLAG_DF | FLAG_IOPL | FLAG_AC | FLAG_NT);
	
	lock_init(&file_rw_lock); // lock_init을 통해 초기화
}

// ! helper function
void check_address(void *addr) {
	struct thread *t = thread_current();
	/* --- Project 2: User memory access --- */
	// if (!is_user_vaddr(addr)||addr == NULL) 
	//-> 이 경우는 유저 주소 영역 내에서도 할당되지 않는 공간 가리키는 것을 체크하지 않음. 그래서 
	// pml4_get_page를 추가해줘야!
	if (!(is_user_vaddr(addr))||addr == NULL||
	pml4_get_page(t->pml4, addr)== NULL)
	{
		exit(-1);
	}
}

static struct file *find_file_by_fd(int fd)
{
    struct thread *cur = thread_current();
    if (fd < 0 || fd >= FDCOUNT_LIMIT)
    {
        return NULL;
    }
    return cur->file_descriptor_table[fd];
}

int add_file_to_fdt(struct file *file)
{
    struct thread *cur = thread_current();
    struct file **fdt = cur->file_descriptor_table;

    // Find open spot from the front
    // fd 위치가 제한 범위 넘지않고, fd table의 인덱스 위치와 일치한다면
    while (cur->fd_idx < FDCOUNT_LIMIT && fdt[cur->fd_idx])
    {
        cur->fd_idx++;
    }

    // error - fd table full
    if (cur->fd_idx >= FDCOUNT_LIMIT)
        return -1;

    fdt[cur->fd_idx] = file;
    return cur->fd_idx;
}

void remove_file_from_fdt(int fd)
{
	// printf ("im in remove_file_from_fdt! (start)\n"); // debugging
    struct thread *cur = thread_current();

    // error : invalid fd
    if (fd < 0 || fd >= FDCOUNT_LIMIT)
        return;

    cur->file_descriptor_table[fd] = NULL;
	// printf ("im in remove_file_from_fdt! (last)\n"); // debugging
}

// ! ---


/* The main system call interface */
void
syscall_handler (struct intr_frame *f) {
	// TODO: Your implementation goes here.
	/*
	1. 시스템콜이 레지스터 RAX에 저장한 시스템콜 넘버에 따라 각기 다른 작업 수행
	2. 리턴값이 있으면 RAX에 저장
	3. 인자는 rdi, rsi, rdx, r10, r8, r9 순으로 전달됨
	*/
	switch (f->R.rax)
	{
	case SYS_HALT:
		halt();	// 인자와 리턴값이 모두 없음
		break;
	case SYS_EXIT:
		exit(f->R.rdi); // 인자가 1개
		break;
	case SYS_FORK: ; // ! 세미콜론 추가: 없으면 컴파일 에러-> 이유?
		struct thread *curr = thread_current();
		memcpy(&curr->parent_if, f, sizeof(struct intr_frame)); // ! 수정 인자로 받은 if를 curr의 parent_if로 복사
		f->R.rax = fork(f->R.rdi); // 인자가 1개고 리턴값이 있음
		break;
	case SYS_EXEC:
		if (exec(f->R.rdi) == -1)
			exit(-1);
		break;
	case SYS_WAIT:
		f->R.rax = wait(f->R.rdi);
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
	// case SYS_DUP2:
	// 	f->R.rax = dup2(f->R.rdi, f->R.rsi);
	// 	break;
	default:
		exit(-1);
		break;
	}
}

void halt (void) {
	power_off();
}

void exit (int status) {
	struct thread *curr = thread_current ();
	curr->exit_status = status;						// thread의 "종료" 상태를 갱신
	printf ("%s: exit(%d)\n", thread_name (), status);
	thread_exit();
}

bool create (const char *file, unsigned initial_size) {
	check_address (file);
	return filesys_create (file, initial_size);
}

bool remove (const char *file) {
	check_address (file);
	return filesys_remove (file);
}

int exec (const char *cmd_line) {
	check_address (cmd_line);

	char *fn_copy = palloc_get_page (PAL_ZERO);			// 새 페이지 할당 받기
	// if (fn_copy == NULL) return -1;						// 할당되지 않은 경우 -1
	if (fn_copy == NULL) // ! 수정
		// return -1; // ? return -1 (=> 이건 f.rax를 -1로 설정하는 것) 과 exit(-1)의 차이
		exit(-1);
	
	strlcpy (fn_copy, cmd_line, strlen(cmd_line) + 1);	// 왜 +1? -> null(\0)문자까지 (이유: strlcpy에서 src에서 dst로 값을 size길이 만큼 복사, size는 문자열 끝의 NULL까지 포함한 길이를 넣어줘야 함)
	
	if (process_exec (fn_copy) == -1) return -1;		// 실행

	NOT_REACHED ();
	return 0;
}

int wait (pid_t pid) {
	return process_wait (pid);
}

pid_t fork (const char *thread_name)
{
	struct thread *t =  thread_current ();
	return process_fork(thread_name, &t->parent_if);
}

// fd 0은 표준 입력, 1은 표준 출력
int open(const char *file)
{
    check_address(file);
	lock_acquire(&file_rw_lock);
    struct file *fileobj = filesys_open(file);

    if (fileobj == NULL)
    {
        return -1;
    }
    // fd table에 file추가
    int fd = add_file_to_fdt(fileobj);

    if (fd == -1)     // fd table 가득 찼을경우
    {
        file_close(fileobj);
    }
	lock_release(&file_rw_lock); // ! 수정 -> release
    return fd;
}

void close(int fd)
{
	// printf ("im in close! (start)\n"); // debugging
	if (fd <= 1) return; // stdin, stdout을 제외하고 모두 닫혀있는 경우는 그냥 return

    struct file *fileobj = find_file_by_fd(fd);
	// printf ("im looking for file (%d : %d)\n", fd, fileobj); // debugging
    if (fileobj == NULL)
    {
        return;
    }
    remove_file_from_fdt(fd); // fd table에 접근하여 fd를 NULL처리

	// printf ("im in close! (last)\n"); // debugging
}

int filesize(int fd)
{
    struct file *fileobj = find_file_by_fd(fd);
    if (fileobj == NULL)
    {
        return -1;
    }
    return file_length(fileobj);
}

int read(int fd, void *buffer, unsigned size) {
	// 유효한 주소인지부터 체크
	check_address(buffer); // 버퍼 시작 주소 체크
	// check_address(buffer + size -1); // 버퍼 끝 주소도 유저 영역 내에 있는지 체크
	unsigned char *buf = buffer;
	int read_count;
	
	struct file *fileobj = find_file_by_fd(fd);

	if (fileobj == NULL) {
		return -1;
	}

	if (fileobj == STDIN)
	{
		char key;
		for (int read_count = 0; read_count < size; read_count++) {
			key  = input_getc(); // size만큼 키보드에서 한 문자씩 입력받음
			*buf++ = key;        // 입력받은 key값을 buf의 주소를 증가시켜가며 넣어줌
			if (key == '\0') {   // 개행 문자를 만날 경우 break
				break;
			}
		}
	}

	else if (fileobj == STDOUT)
	{
		return -1;
	}

	else {
		lock_acquire(&file_rw_lock);
		read_count = file_read(fileobj, buffer, size); // 파일 읽어들일 동안만 lock 걸어준다.
		lock_release(&file_rw_lock);

	}
	return read_count;
}

int write (int fd, const void *buffer, unsigned size) {
	check_address(buffer);
	struct file *fileobj = find_file_by_fd(fd);
	int read_count;
	
	if (fileobj == NULL)
		return -1;
	
	if (fileobj == STDOUT)
	{
		putbuf(buffer, size);
		read_count = size;
	}	
	
	// else if (fd == STDIN) { // read와 반대로 STDIN일 경우 -1 반환
	else if (fileobj == STDIN)
	{
		return -1;
	}

	else {
		lock_acquire(&file_rw_lock);
		read_count = file_write(fileobj, buffer, size);
		lock_release(&file_rw_lock);
	}
	return read_count; // ! 추가
}

void seek(int fd, unsigned position) {
	struct file *fileobj = find_file_by_fd(fd);
	if (fd < 2)
		return;

	file_seek(fileobj, position); // ! file.c에 제공된 file_seek 사용하여 구현
}

/* 파일의 시작점부터 현재 위치까지의 offset을 반환 */
unsigned tell(int fd) {
	struct file *fileobj = find_file_by_fd(fd);
	if (fd < 2)
		return;
	return file_tell(fileobj);
}
