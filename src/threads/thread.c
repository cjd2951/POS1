#include "threads/thread.h"
#include <debug.h>
#include <stddef.h>
#include <random.h>
#include <stdio.h>
#include <string.h>
#include "threads/flags.h"
#include "threads/interrupt.h"
#include "threads/intr-stubs.h"
#include "threads/palloc.h"
#include "threads/switch.h"
#include "threads/synch.h"
#include "threads/vaddr.h"
#include "threads/fixed-point.h"
#include "thread.h"

#ifdef USERPROG
#include "userprog/process.h"
#endif

/* Random value for struct thread's `magic' member.
   Used to detect stack overflow.  See the big comment at the top
   of thread.h for details. */
#define THREAD_MAGIC 0xcd6abf4b

/* List of processes in THREAD_READY state, that is, processes
   that are ready to run but not actually running. */
static struct list ready_list;

/* List of all processes.  Processes are added to this list
   when they are first scheduled and removed when they exit. */
static struct list all_list;

// Jed new code
// add a list struct sleep_list, used to hold sleeping threads
//static struct list sleep_list;

/* Idle thread. */
static struct thread *idle_thread;

/* Initial thread, the thread running init.c:main(). */
static struct thread *initial_thread;

/* Lock used by allocate_tid(). */
static struct lock tid_lock;

/* Stack frame for kernel_thread(). */
struct kernel_thread_frame 
  {
    void *eip;                  /* Return address. */
    thread_func *function;      /* Function to call. */
    void *aux;                  /* Auxiliary data for function. */
  };

/* Statistics. */
static long long idle_ticks;    /* # of timer ticks spent idle. */
static long long kernel_ticks;  /* # of timer ticks in kernel threads. */
static long long user_ticks;    /* # of timer ticks in user programs. */

/* Scheduling. */
#define TIME_SLICE 4            /* # of timer ticks to give each thread. */
static unsigned thread_ticks;   /* # of timer ticks since last yield. */

/* If false (default), use round-robin scheduler.
   If true, use multi-level feedback queue scheduler.
   Controlled by kernel command-line option "-o mlfqs". */
bool thread_mlfqs;


// Jed new code
// added static preempt_if_higher_pri method, takes a pointer to a thread struct
static void preempt_if_higher_pri(struct thread *);

// Jed new code
// added static is_highest_priority, returns true if A priority is higher than B priority
static bool is_highest_priority(void);

static void kernel_thread (thread_func *, void *aux);

static void idle (void *aux UNUSED);
static struct thread *running_thread (void);
static struct thread *next_thread_to_run (void);
static void init_thread (struct thread *, const char *name, int priority);
static bool is_thread (struct thread *) UNUSED;
static void *alloc_frame (struct thread *, size_t size);
static void schedule (void);
void thread_schedule_tail (struct thread *prev);
static tid_t allocate_tid (void);
int load_average;

/* Initializes the threading system by transforming the code
   that's currently running into a thread.  This can't work in
   general and it is possible in this case only because loader.S
   was careful to put the bottom of the stack at a page boundary.

   Also initializes the run queue and the tid lock.

   After calling this function, be sure to initialize the page
   allocator before trying to create any threads with
   thread_create().

   It is not safe to call thread_current() until this function
   finishes. */
void
thread_init (void) 
{
  ASSERT (intr_get_level () == INTR_OFF);

  lock_init (&tid_lock);
  list_init (&ready_list);
  list_init (&all_list);
  // Jed new code 
  // initialize list sleep list calling list_init and passing the address of sleep_list
  list_init(&sleep_list);

  /* Set up a thread structure for the running thread. */
  initial_thread = running_thread ();
  init_thread (initial_thread, "main", PRI_DEFAULT);
  initial_thread->status = THREAD_RUNNING;
  initial_thread->tid = allocate_tid ();
  load_average = 0;
}

/* Starts preemptive thread scheduling by enabling interrupts.
   Also creates the idle thread. */
void
thread_start (void) 
{
  /* Create the idle thread. */
  struct semaphore idle_started;
  sema_init (&idle_started, 0);
  thread_create ("idle", PRI_MIN, idle, &idle_started);

  /* Start preemptive thread scheduling. */
  intr_enable ();

  /* Wait for the idle thread to initialize idle_thread. */
  sema_down (&idle_started);
}

/* Called by the timer interrupt handler at each timer tick.
   Thus, this function runs in an external interrupt context. */
void
thread_tick (void) 
{
  struct thread *t = thread_current ();

  /* Update statistics. */
  if (t == idle_thread)
    idle_ticks++;
#ifdef USERPROG
  else if (t->pagedir != NULL)
    user_ticks++;
#endif
  else
    kernel_ticks++;

  // Jed new code
  // decrement sleeping threads tick counts
  // create a pointer to a list_elem struct
  struct list_elem *e;
  
  // iterate through the sleep list
  for (e = list_begin(&sleep_list); e != list_end(&sleep_list); e = list_next (e)) {
    
    // create a struct thread pointer that points to the entry in the sleep list
    struct thread *t = list_entry (e, struct thread, sleepelem);
    
    // decrement sleeping threads tick count
    t->sleep_ticks--;
  }

  // wake up all threads that have slept long enough, put them on the ready list
  // iterate through the sleep list
  for (e = list_begin(&sleep_list); e != list_end(&sleep_list); e = list_next (e)) {
     
    // create a struct thread pointer that points to the entry in the sleep list
    struct thread *t = list_entry (e, struct thread, sleepelem);
       
      // if the sleep ticks of the thread is equal to 0, unblock thread and remove from sleep list
      if (t-> sleep_ticks == 0) {
        thread_unblock(t);
        list_remove(e);
    }
  }

  /* Enforce preemption. */
  if (++thread_ticks >= TIME_SLICE)
    intr_yield_on_return ();



}

/* Prints thread statistics. */
void
thread_print_stats (void) 
{
  printf ("Thread: %lld idle ticks, %lld kernel ticks, %lld user ticks\n",
          idle_ticks, kernel_ticks, user_ticks);
}

/* Creates a new kernel thread named NAME with the given initial
   PRIORITY, which executes FUNCTION passing AUX as the argument,
   and adds it to the ready queue.  Returns the thread identifier
   for the new thread, or TID_ERROR if creation fails.

   If thread_start() has been called, then the new thread may be
   scheduled before thread_create() returns.  It could even exit
   before thread_create() returns.  Contrariwise, the original
   thread may run for any amount of time before the new thread is
   scheduled.  Use a semaphore or some other form of
   synchronization if you need to ensure ordering.

   The code provided sets the new thread's `priority' member to
   PRIORITY, but no actual priority scheduling is implemented.
   Priority scheduling is the goal of Problem 1-3. */
tid_t
thread_create (const char *name, int priority,
               thread_func *function, void *aux) 
{
  struct thread *t;
  struct kernel_thread_frame *kf;
  struct switch_entry_frame *ef;
  struct switch_threads_frame *sf;
  tid_t tid;

  ASSERT (function != NULL);

  /* Allocate thread. */
  t = palloc_get_page (PAL_ZERO);
  if (t == NULL)
    return TID_ERROR;

  /* Initialize thread. */
  init_thread (t, name, priority);
  tid = t->tid = allocate_tid ();

  /* Stack frame for kernel_thread(). */
  kf = alloc_frame (t, sizeof *kf);
  kf->eip = NULL;
  kf->function = function;
  kf->aux = aux;

  /* Stack frame for switch_entry(). */
  ef = alloc_frame (t, sizeof *ef);
  ef->eip = (void (*) (void)) kernel_thread;

  /* Stack frame for switch_threads(). */
  sf = alloc_frame (t, sizeof *sf);
  sf->eip = switch_entry;
  sf->ebp = 0;

  /* Add to run queue. */
  thread_unblock (t);

  if(thread_mlfqs){
    recalculate_priority(t);
  }

  // Jed new code
  // call to preempt_if_higher_pri
  preempt_if_higher_pri(t);

  return tid;
}

// Jed new code
// this method will preempt the currently running thread
// if the given thread's priority is higher
static void preempt_if_higher_pri (struct thread *t) {
  if (thread_get_priority() < thread_get_highest_priority(t)) {
    thread_yield();
  }
}

// Jed new code
// this method will preempt the currently running thread
// if a thread in the ready queue has a higher priority
void preempt_if_not_highest_pri() {
  struct list_elem *e = list_max(&ready_list, lesser_priority, NULL);
  struct thread *t = list_entry(e, struct thread, elem);
  preempt_if_higher_pri(t);
}

// Jed new code
// this method returns the higher priority, either the
// donated priority or initial priority
int thread_get_highest_priority (struct thread *t) {
  if (t->priority_donated > t->priority)
    return t->priority_donated;
  return t->priority;
}

// Jed new code
// this method puts the current thread in the sleep 
// list and sets the sleep ticks
void sleep_list_add(struct thread *t) {
  list_push_back(&sleep_list, &t->sleepelem);
}

/* Puts the current thread to sleep.  It will not be scheduled
   again until awoken by thread_unblock().

   This function must be called with interrupts turned off.  It
   is usually a better idea to use one of the synchronization
   primitives in synch.h. */
void
thread_block (void) 
{
  ASSERT (!intr_context ());
  ASSERT (intr_get_level () == INTR_OFF);

  thread_current ()->status = THREAD_BLOCKED;
  schedule ();
}

/* Transitions a blocked thread T to the ready-to-run state.
   This is an error if T is not blocked.  (Use thread_yield() to
   make the running thread ready.)

   This function does not preempt the running thread.  This can
   be important: if the caller had disabled interrupts itself,
   it may expect that it can atomically unblock a thread and
   update other data. */
void
thread_unblock (struct thread *t) 
{
  enum intr_level old_level;

  ASSERT (is_thread (t));

  old_level = intr_disable ();
  ASSERT (t->status == THREAD_BLOCKED);
	//list_push_back (&ready_list, &t->elem);
  //NEW CODE
  list_insert_ordered(&ready_list, &t->elem, sort_by_priority, NULL);
  t->status = THREAD_READY;
  intr_set_level (old_level);
}

/* Returns the name of the running thread. */
const char *
thread_name (void) 
{
  return thread_current ()->name;
}

/* Returns the running thread.
   This is running_thread() plus a couple of sanity checks.
   See the big comment at the top of thread.h for details. */
struct thread *
thread_current (void) 
{
  struct thread *t = running_thread ();
  
  /* Make sure T is really a thread.
     If either of these assertions fire, then your thread may
     have overflowed its stack.  Each thread has less than 4 kB
     of stack, so a few big automatic arrays or moderate
     recursion can cause stack overflow. */
  ASSERT (is_thread (t));
  ASSERT (t->status == THREAD_RUNNING);

  return t;
}

/* Returns the running thread's tid. */
tid_t
thread_tid (void) 
{
  return thread_current ()->tid;
}

/* Deschedules the current thread and destroys it.  Never
   returns to the caller. */
void
thread_exit (void) 
{
  ASSERT (!intr_context ());

#ifdef USERPROG
  process_exit ();
#endif

  /* Remove thread from all threads list, set our status to dying,
     and schedule another process.  That process will destroy us
     when it calls thread_schedule_tail(). */
  intr_disable ();
  list_remove (&thread_current()->allelem);
  thread_current ()->status = THREAD_DYING;
  schedule ();
  NOT_REACHED ();
}

/* Yields the CPU.  The current thread is not put to sleep and
   may be scheduled again immediately at the scheduler's whim. */
void
thread_yield (void)
{
  struct thread *cur = thread_current ();
  enum intr_level old_level;
  
  ASSERT (!intr_context ());

  old_level = intr_disable ();
  if (cur != idle_thread) 
    //list_insert_ordered(&ready_list, &cur->elem, sort_by_priority, NULL);
    list_push_front (&ready_list, &cur->elem);
  cur->status = THREAD_READY;
  schedule ();
  intr_set_level (old_level);
}

/* Invoke function 'func' on all threads, passing along 'aux'.
   This function must be called with interrupts off. */
void
thread_foreach (thread_action_func *func, void *aux)
{
  struct list_elem *e;

  ASSERT (intr_get_level () == INTR_OFF);

  for (e = list_begin (&all_list); e != list_end (&all_list);
       e = list_next (e))
    {
      struct thread *t = list_entry (e, struct thread, allelem);
      func (t, aux);
    }
}

/* Sets the current thread's priority to NEW_PRIORITY. 
Sets the current thread’s priority to new priority. If the current thread no longer has
the highest priority, yields. */
void
thread_set_priority (int new_priority) 
{
  if(thread_mlfqs){
    return;
  }
  //thread_current ()->priority = new_priority;
//  struct list_elem *e = list_begin(&ready_list);//grab the front of the ready list
//  struct thread *t = list_entry(e, struct thread, elem);//convert to the thread struct
//  struct thread *cur = thread_current();//grab thread changing priority
//  cur->priority = new_priority;  //Set new priority
//  if(cur->priority <= t->priority)
//  {
//     thread_yield();
// }        

  thread_current ()->priority = new_priority;
  if (!is_highest_priority()) {
    thread_yield();
  }
}

// Jed new code
// this method will retrun whether or not the currently 
// running thread is still the highest priority
static bool is_highest_priority() {
  struct list_elem *e = list_max(&ready_list, lesser_priority, NULL);
  struct thread *t = list_entry(e, struct thread, elem);
  return thread_get_priority() >= thread_get_highest_priority (t);
}

/* Returns the current thread's priority. */
int
thread_get_priority (void) 
{
//  int temp_priority = thread_current ()->priority;
//  return temp_priority;
    // Jed new code 
    // return the highest priority 
    return thread_get_highest_priority(thread_current());
}

/* Sets the current thread's nice value to NICE. */
void
thread_set_nice (int new_nice)
{
  thread_current()->nice = new_nice;
  calculate_recent_cpu(thread_current(),NULL);
  recalculate_priority(thread_current());
  struct thread *front = list_entry(list_begin(&ready_list), struct thread, elem);
  if(!list_empty(&ready_list) && thread_current()->priority < front->priority){
    thread_yield();
  }
}

/* Returns the current thread's nice value. */
int
thread_get_nice (void) 
{
  int temp_nice = thread_current()->nice;
  return temp_nice;
}

/* Returns 100 times the system load average. */
int
thread_get_load_avg (void) 
{
  int temp_load_average = float_to_int_nearest_round(multi_float_and_int(load_average,100));
  return temp_load_average;
}

/* Returns 100 times the current thread's recent_cpu value. */
int
thread_get_recent_cpu (void) 
{
  int temp_recent_cpu = float_to_int_nearest_round(multi_float_and_int(thread_current()->recent_cpu,100));
  return temp_recent_cpu;
}


void calculate_recent_cpu(struct thread *cur_thread, void *aux UNUSED){
  if(cur_thread == idle_thread){
    return;
  }
  int partial1 = multi_float_and_int(load_average,2);
  int partial2 = add_float_and_int(multi_float_and_int(load_average,2), 1);
  int partial3 = divide_floats(partial1,partial2);
  int cur_t_cpu = cur_thread->recent_cpu;
  int partial4 = multi_floats(partial3,cur_t_cpu);
  int cur_t_nice = cur_thread->nice;
  int temp_recent_cpu = add_float_and_int(partial4,cur_t_nice);
  cur_thread->recent_cpu = temp_recent_cpu;
}

void recalculate_priority(struct thread *cur_thread){
  if(cur_thread == idle_thread){
    return;
  }
  int temp_priority = PRI_MAX - float_to_int_nearest_round(divide_float_and_int(cur_thread->recent_cpu,4)) - (cur_thread->nice * 2);
  cur_thread->priority = temp_priority;
  if(cur_thread->priority < PRI_MIN){
    cur_thread->priority = PRI_MIN;
  }
  if(cur_thread->priority > PRI_MAX){
    cur_thread->priority = PRI_MAX;
  }
}

void calculate_load_average(void){
  int ready_list_size = list_size(&ready_list);
  if(thread_current() != idle_thread){
    ready_list_size++;
  }
  int first_frac = int_to_float(59)/60;
  int second_frac = int_to_float(1)/60;
  int temp_load_average = add_floats(multi_floats(first_frac,load_average),second_frac * ready_list_size);
  load_average = temp_load_average;

}

void recent_cpu_increment(void){
  thread_current()->recent_cpu = add_float_and_int(thread_current()->recent_cpu, 1);
}
void calculate_recent_for_all(void){
  thread_foreach(calculate_recent_cpu, NULL);
}
void recalc_current_prior(void){
  recalculate_priority(thread_current());
}

/* Idle thread.  Executes when no other thread is ready to run.

   The idle thread is initially put on the ready list by
   thread_start().  It will be scheduled once initially, at which
   point it initializes idle_thread, "up"s the semaphore passed
   to it to enable thread_start() to continue, and immediately
   blocks.  After that, the idle thread never appears in the
   ready list.  It is returned by next_thread_to_run() as a
   special case when the ready list is empty. */
static void
idle (void *idle_started_ UNUSED) 
{
  struct semaphore *idle_started = idle_started_;
  idle_thread = thread_current ();
  sema_up (idle_started);

  for (;;) 
    {
      /* Let someone else run. */
      intr_disable ();
      thread_block ();

      /* Re-enable interrupts and wait for the next one.

         The `sti' instruction disables interrupts until the
         completion of the next instruction, so these two
         instructions are executed atomically.  This atomicity is
         important; otherwise, an interrupt could be handled
         between re-enabling interrupts and waiting for the next
         one to occur, wasting as much as one clock tick worth of
         time.

         See [IA32-v2a] "HLT", [IA32-v2b] "STI", and [IA32-v3a]
         7.11.1 "HLT Instruction". */
      asm volatile ("sti; hlt" : : : "memory");
    }
}

/* Function used as the basis for a kernel thread. */
static void
kernel_thread (thread_func *function, void *aux) 
{
  ASSERT (function != NULL);

  intr_enable ();       /* The scheduler runs with interrupts off. */
  function (aux);       /* Execute the thread function. */
  thread_exit ();       /* If function() returns, kill the thread. */
}

/* Returns the running thread. */
struct thread *
running_thread (void) 
{
  uint32_t *esp;

  /* Copy the CPU's stack pointer into `esp', and then round that
     down to the start of a page.  Because `struct thread' is
     always at the beginning of a page and the stack pointer is
     somewhere in the middle, this locates the curent thread. */
  asm ("mov %%esp, %0" : "=g" (esp));
  return pg_round_down (esp);
}

/* Returns true if T appears to point to a valid thread. */
static bool
is_thread (struct thread *t)
{
  return t != NULL && t->magic == THREAD_MAGIC;
}

/* Does basic initialization of T as a blocked thread named
   NAME. */
static void
init_thread (struct thread *t, const char *name, int priority)
{
  enum intr_level old_level;

  ASSERT (t != NULL);
  ASSERT (PRI_MIN <= priority && priority <= PRI_MAX);
  ASSERT (name != NULL);

  memset (t, 0, sizeof *t);
  t->status = THREAD_BLOCKED;
  strlcpy (t->name, name, sizeof t->name);
  t->stack = (uint8_t *) t + PGSIZE;
  t->priority = priority;
  
  // Jed new code
  // start with priority donated as = -1
  t->priority_donated = PRI_NONE;
  
  // set wanting_lock to NULL
  t->wanting_lock = NULL;

  // initialize donate
  list_init(&t->donate_list);
  t->magic = THREAD_MAGIC;
  if(thread_mlfqs){
    t->nice = NICE_DEFAULT;
    if(t == initial_thread){
      t->recent_cpu = 0;
    }else{
      t->recent_cpu = thread_get_recent_cpu();
      t->nice = thread_current()->nice;
    }
  }

  old_level = intr_disable ();
  list_push_back (&all_list, &t->allelem);
  intr_set_level (old_level);
}

/* Allocates a SIZE-byte frame at the top of thread T's stack and
   returns a pointer to the frame's base. */
static void *
alloc_frame (struct thread *t, size_t size) 
{
  /* Stack data is always allocated in word-size units. */
  ASSERT (is_thread (t));
  ASSERT (size % sizeof (uint32_t) == 0);

  t->stack -= size;
  return t->stack;
}

/* Jed new code
returns true if a priority is less than b priority
*/
bool lesser_priority(const struct list_elem *a, const struct list_elem *b, void *aux UNUSED) {
  struct thread *t1 = list_entry(a, struct thread, elem);
  struct thread *t2 = list_entry(b, struct thread, elem);
  ASSERT(t1 != NULL);
  ASSERT(t2 != NULL);
  return thread_get_highest_priority(t1) < thread_get_highest_priority(t2);
}


/* Chooses and returns the next thread to be scheduled.  Should
   return a thread from the run queue, unless the run queue is
   empty.  (If the running thread can continue running, then it
   will be in the run queue.)  If the run queue is empty, return
   idle_thread. */


// Jed new code
static struct thread *
next_thread_to_run (void) 
{
  if (list_empty(&ready_list))
    return idle_thread;
//  }else {
    //next thread to run is on the front of the ready list
    //because the ready list is in priority order
//    return list_entry(list_pop_front(&ready_list), struct thread, elem);
  else {
    list_sort(&ready_list, lesser_priority, 0);
    return list_entry(list_pop_back(&ready_list), struct thread, elem);
  }
}
/* Completes a thread switch by activating the new thread's page
   tables, and, if the previous thread is dying, destroying it.

   At this function's invocation, we just switched from thread
   PREV, the new thread is already running, and interrupts are
   still disabled.  This function is normally invoked by
   thread_schedule() as its final action before returning, but
   the first time a thread is scheduled it is called by
   switch_entry() (see switch.S).

   It's not safe to call printf() until the thread switch is
   complete.  In practice that means that printf()s should be
   added at the end of the function.

   After this function and its caller returns, the thread switch
   is complete. */
void
thread_schedule_tail (struct thread *prev)
{
  struct thread *cur = running_thread ();
  
  ASSERT (intr_get_level () == INTR_OFF);

  /* Mark us as running. */
  cur->status = THREAD_RUNNING;

  /* Start new time slice. */
  thread_ticks = 0;

#ifdef USERPROG
  /* Activate the new address space. */
  process_activate ();
#endif

  /* If the thread we switched from is dying, destroy its struct
     thread.  This must happen late so that thread_exit() doesn't
     pull out the rug under itself.  (We don't free
     initial_thread because its memory was not obtained via
     palloc().) */
  if (prev != NULL && prev->status == THREAD_DYING && prev != initial_thread) 
    {
      ASSERT (prev != cur);
      palloc_free_page (prev);
    }
}

/* Schedules a new process.  At entry, interrupts must be off and
   the running process's state must have been changed from
   running to some other state.  This function finds another
   thread to run and switches to it.

   It's not safe to call printf() until thread_schedule_tail()
   has completed. */
static void
schedule (void) 
{
  struct thread *cur = running_thread ();
  struct thread *next = next_thread_to_run ();
  struct thread *prev = NULL;

  ASSERT (intr_get_level () == INTR_OFF);
  ASSERT (cur->status != THREAD_RUNNING);
  ASSERT (is_thread (next));

  if (cur != next)
    prev = switch_threads (cur, next);
  thread_schedule_tail (prev);
}

/* Returns a tid to use for a new thread. */
static tid_t
allocate_tid (void) 
{
  static tid_t next_tid = 1;
  tid_t tid;

  lock_acquire (&tid_lock);
  tid = next_tid++;
  lock_release (&tid_lock);

  return tid;
}

/** NEW CODE **/
//This is a sort function to be used with in-order insertion for priority
//returns 'true' if a > b, else returns 'false'
bool sort_by_priority (const struct list_elem *a, const struct list_elem *b, void *aux UNUSED)
{
  //convert the 2 list elements to threads using the macro defined in thread.h
  //compare, and return true or false
  struct thread *t1 = list_entry(a, struct thread, elem);
  struct thread *t2 = list_entry(b, struct thread, elem);
  if(t1->priority > t2->priority){
    return true;
  }
  else{
    return false;
  }
}


/* Offset of `stack' member within `struct thread'.
   Used by switch.S, which can't figure it out on its own. */
uint32_t thread_stack_ofs = offsetof (struct thread, stack);
