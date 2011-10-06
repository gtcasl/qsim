/*****************************************************************************\
* Qemu Simulation Framework (qsim)                                            *
* Qsim is a modified version of the Qemu emulator (www.qemu.org), coupled     *
* a C++ API, for the use of computer architecture researchers.                *
*                                                                             *
* This work is licensed under the terms of the GNU GPL, version 2. See the    *
* COPYING file in the top-level directory.                                    *
\*****************************************************************************/

#include <climits>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <signal.h>

#include <fstream>
#include <sstream>
#include <string>
#include <list>
#include <vector>
#include <map>
#include <set>

#include <readline/readline.h> //for rl_get_screen_size
#include <readline/history.h>

#include <distorm.h>

#include <qsim-regs.h>
#include <qsim.h>

#include "util.h"
#include "qdb.h"
#include "banner.h"

const size_t LINE_BUF_SIZE = 80;

using std::list;          using std::string;   using std::ostringstream;
using std::istringstream; using std::ifstream; using std::map;
using std::set;           using std::vector;

using Qsim::OSDomain;

const unsigned DUMP_COLS = 16  ; // Number of columns in a memory dump.
const uint64_t MAX_RUN   = 1000; // Longst time we can keep an atomic 
                                 // operation waiting.

// Global variables used throughout QDB
unsigned    n_cpus;
const char* kernel_filename;
unsigned    barriers_per_interrupt = 1;
unsigned    countdown_to_interrupt = 1;

map<string, uint64_t>                 global_symbol_table;
map<uint64_t, map<string, uint64_t> > symbol_tables;       //Indexed by CR3
OSDomain                              *cd;
vector<CPUThread*>                    thread_objs;

pthread_mutex_t                       prof_lock = PTHREAD_MUTEX_INITIALIZER;
map<string, uint64_t>                 prof_inst_counts;
set<int>                              prof_tids;
bool                                  prof_all_tids;

uint32_t                              prof_mean_ibs = 1000000;
uint32_t                              prof_countdown = 0;

// Global variables local to this file.
static uint64_t *rip_vec;
bool app_flag = false;

int yyparse(void);

extern "C" {
  uint64_t symbol_lookup(const char* symbol) {
    map<string, uint64_t>::iterator sym_it;

    // First, try the global (kernel) symbol table.
    sym_it = global_symbol_table.find(string(symbol));
    if (sym_it != global_symbol_table.end()) return sym_it->second;

    // Next, try the symbol tables in order until one is found.
    map<uint64_t, map<string, uint64_t> >::iterator tab_it;
    for (tab_it=symbol_tables.begin(); tab_it!=symbol_tables.end(); tab_it++) {
      sym_it = tab_it->second.find(string(symbol));
      if (sym_it != tab_it->second.end()) return sym_it->second;
    }

    printf("Symbol lookup of \"%s\" failed.\n", symbol);
    return 0xdeadbeef;
  }

  int register_lookup(const char* regname) {
    static struct { 
      const char* s; int i; 
    } reg_table[] = {
      { "%rax", QSIM_RAX }, { "%rcx", QSIM_RCX }, { "%rdx", QSIM_RDX }, 
      { "%rbx", QSIM_RBX }, { "%rsp", QSIM_RSP }, { "%rbp", QSIM_RBP },
      { "%rsi", QSIM_RSI }, { "%rdi", QSIM_RDI }, { "%es",  QSIM_ES  },
      { "%cs",  QSIM_CS  }, { "%ss",  QSIM_SS  }, { "%ds",  QSIM_DS  },
      { "%fs",  QSIM_FS  }, { "%gs",  QSIM_GS  }, { "%rip", QSIM_RIP },
      { "%cr0", QSIM_CR0 }, { "%cr2", QSIM_CR2 }, { "%cr3", QSIM_CR3 },
      { "%rflags", QSIM_RFLAGS }, {NULL,0}
    };

    for (int i = 0; reg_table[i].s != NULL; i++)
      if (!strncasecmp(regname, reg_table[i].s, 4))  return reg_table[i].i;

    return -1;
  }
};

string get_nearest_symbol_below(uint64_t addr, uint64_t cr3) {
  string current = "[no symbol]";
  int    number_to_beat = INT_MAX;

  // First try the local symbol table.
  map<string, uint64_t>::iterator i;
  for (i = symbol_tables[cr3].begin(); i != symbol_tables[cr3].end(); i++) {
    if (addr - i->second >= INT_MAX || i->second > addr) continue;
    int difference = addr - i->second;
    if (difference < number_to_beat) {
      number_to_beat = difference;
      current = i->first;
    }
  }

  // Then check the global symbol table.
  for (i = global_symbol_table.begin(); i != global_symbol_table.end(); i++) {
    if (addr - i->second >= INT_MAX || i->second > addr) continue;
    int difference = addr - i->second;
    if (difference < number_to_beat) {
      number_to_beat = difference;
      current = i->first;
    }
  }

  return current;
}

// Instruction callback for better %rip value tracking (get_reg only gives us
// %rip from the last basic block).
void rip_inst_cb(int cpu, uint64_t v, uint64_t p, uint8_t s, const uint8_t* b,
                 enum inst_type t) 
{
  // rip_vec is just a lump of RAM accessed like a C array, meaning that 
  // parallel accesses to different indices are just fine.
  rip_vec[cpu] = v;
}

// Use the thread-unsafe rand(), but avoid modulo bias.
static uint32_t _rand(uint32_t max) {
  uint32_t max_allowable = RAND_MAX - (RAND_MAX%max);
  uint32_t result;

  while ( (result = rand()%max) > max_allowable);

  return result;
}

// Instruction callback for profiling.
void prof_inst_cb(int cpu, uint64_t v, uint64_t p, uint8_t s, const uint8_t* b,
                  enum inst_type t)
{
  // Random statistical sampling.
  if (prof_countdown != 0) {
    prof_countdown--;
    return;
  }
  prof_countdown = _rand(prof_mean_ibs * 2);
  //printf("Set new sample timeout to %u.\n", prof_countdown);

  uint64_t cr3 = cd->get_reg(cpu, QSIM_CR3);
  uint16_t tid = cd->get_tid(cpu);

  // Our values for %rip still must be updated.
  rip_inst_cb(cpu, v, p, s, b, t);

  pthread_mutex_lock(&prof_lock);
  if (prof_all_tids || prof_tids.find(tid) != prof_tids.end()) {
    string symbol = get_nearest_symbol_below(v, cr3);
    prof_inst_counts[symbol]++;
  }
  pthread_mutex_unlock(&prof_lock);
}

void prof_all() {
  pthread_mutex_lock(&prof_lock);
  prof_all_tids = true;
  pthread_mutex_unlock(&prof_lock);
  cd->set_inst_cb(prof_inst_cb);
}

void prof_on(int tid) {
  pthread_mutex_lock(&prof_lock);
  if (!prof_all_tids) prof_tids.insert(tid);
  if (prof_tids.size() == 1) cd->set_inst_cb(prof_inst_cb);
  pthread_mutex_unlock(&prof_lock);
}

void prof_off(int tid) {
  bool clear_inst_cb = false;

  pthread_mutex_lock(&prof_lock);
  prof_tids.erase(tid);
  if (prof_tids.size() == 0 && !prof_all_tids) {
    clear_inst_cb = true;
    prof_inst_counts.clear();
  }
  pthread_mutex_unlock(&prof_lock);

  if (clear_inst_cb) cd->set_inst_cb(rip_inst_cb);
}

void prof_off() {
  pthread_mutex_lock(&prof_lock);
  prof_all_tids = false;
  prof_tids.clear();
  prof_inst_counts.clear();
  pthread_mutex_unlock(&prof_lock);
  cd->set_inst_cb(rip_inst_cb);
}

void disas(uint64_t vaddr, uint64_t size) {
  uint8_t      *buf   = new uint8_t[size];
  _DecodedInst *insts = new _DecodedInst[size];
  unsigned int n_decoded;

  // Copy the instructions from QSIM ram.
  for (uint64_t i = 0; i < size; i++) cd->mem_rd(buf[i], vaddr+i);

  // Disassemble them.
  distorm_decode(0, buf, size, Decode32Bits, insts, size, &n_decoded);

  // Pretty-print the results.
  for (uint64_t i = 0; i < n_decoded; i++) {
    printf(
      "%08llx: %s %s\n", 
      (unsigned long long)(vaddr + insts[i].offset), 
      insts[i].mnemonic.p, insts[i].operands.p
    );
  }

  delete[] buf;
  delete[] insts;
}

void mem_dump_row(uint64_t paddr, uint64_t size) {
  typedef unsigned long long ull;
  printf("%08llx: ", (ull)paddr);
  for (unsigned i = 0; i < size; i++) {
    uint8_t val;
    cd->mem_rd(val, paddr+i);
    printf("%02x ", (unsigned)val);
  }

  if (size < DUMP_COLS) 
    for (unsigned i = size; i < DUMP_COLS; i++) printf("   ");

  for (unsigned i = 0; i < size; i++) {
    uint8_t val;
    cd->mem_rd(val, paddr+i);
    if (isprint(val)) putc(val, stdout);
    else              putc('.',  stdout);
  }

  putc('\n', stdout);
}

void mem_dump(uint64_t paddr, uint64_t size) {
  while (size > 0) {
    uint64_t row_size = DUMP_COLS>size?size:DUMP_COLS;
    mem_dump_row(paddr, row_size);
    paddr += row_size;
    size  -= row_size;
  }
}

// Print status of CPU i.
void cpu_stat(unsigned i) {
  typedef unsigned long long ull;
  ull rax = cd->get_reg(i, QSIM_RAX), rcx = cd->get_reg(i, QSIM_RCX), 
      rdx = cd->get_reg(i, QSIM_RDX), rbx = cd->get_reg(i, QSIM_RBX), 
      rsp = cd->get_reg(i, QSIM_RSP), rbp = cd->get_reg(i, QSIM_RBP), 
      rip = rip_vec[i],               cr3 = cd->get_reg(i, QSIM_CR3);
  int tid = cd->get_tid(i);
  const char* sym = get_nearest_symbol_below(rip, cr3).c_str();

  const char *idle_str = cd->idle(i)?"(idle)":"";

  printf(" rax=%08llx  rcx=%08llx  rdx=%08llx  rbx=%08llx\n"
         " rip=%08llx  rsp=%08llx  rbp=%08llx  cr3=%08llx\n"
         " TID=%d %s\n Nearest symbol: %s\n", 
	 rax, rcx, rdx, rbx, rip, rsp, rbp, cr3, tid, idle_str, sym);
}

// Print profiling report.
void do_report() {
  // Print the contents of prof_inst_counts.
  map<string, uint64_t>::iterator i;
  pthread_mutex_lock(&prof_lock);
  puts(" Dynamic Insts     Symbol\n"
       "---------------   -----------------------------");
  for (i = prof_inst_counts.begin(); i != prof_inst_counts.end(); i++) {
    printf("%15llu   %s\n", i->second, i->first.c_str());
  }
  pthread_mutex_unlock(&prof_lock);
}

void symbol_add(bool global, uint64_t cr3, string symbol, uint64_t value) {
  if (global) global_symbol_table[symbol] = value;
  else        symbol_tables[cr3][symbol]  = value;
}

void load_symbols(string filename, bool global, uint64_t cr3) {
  ifstream symbol_map;
  string   type;
  uint64_t addr;
  string   sym;

  // Open the symbol map file.
  symbol_map.open(filename.c_str());
  if (!symbol_map) {
    printf("Failed to open file \"%s\".\n", filename.c_str());
    return;
  }

  // Read the symbols.
  for (;;) {
    uint64_t addr;
    string type;
    string sym;
    char   c;

    for (;;) {
      c = symbol_map.peek();
      if ( (c>='0' && c<='9') || (c>='a' && c<='f') || !symbol_map) break;
      symbol_map >> type >> sym >> std::ws;
    };

    symbol_map >> std::hex >> addr >> type >> sym >> std::ws;

    if (!symbol_map) break;

    symbol_add(global, cr3, sym, addr);
  }

  // Close the symbol map file.
  symbol_map.close();

  // Tell the user we've finished.
  printf("Loaded symbols from file \"%s\".\n", filename.c_str());
}

void clear_symbols(bool global, uint64_t cr3) {
  if (global) global_symbol_table.clear();
  else         symbol_tables[cr3].clear();
}

static char line_buf[LINE_BUF_SIZE];

Paginate::Paginate() {
  //No initialization needed for the paginator.
}

void Paginate::operator()(list<string> &lines) {
  int rows, cols;
  rl_get_screen_size(&rows, &cols);
  list<string>::iterator i = lines.begin();
  int line = 0;
  while (i != lines.end()) {
    if (line >= (rows-1)) { pause(); line = 0; }
    puts(i->c_str());
    i++; line++;
  }
}

void Paginate::pause() {
  fputs("--Press [return] to continue--", stdout);
  fgets(line_buf, LINE_BUF_SIZE, stdin);
}

Paginate paginate;

void sigint_handler(int i) {
  printf("\n---Execution suspended---\n");
  for (unsigned i = 0; i < n_cpus; i++) {
    thread_objs[i]->ctrl_c();
  }
}

void setup_signals() {
  struct sigaction sa;

  sa.sa_handler = sigint_handler;
  sigemptyset (&sa.sa_mask);
  sa.sa_flags = 0;

  sigaction(SIGINT, &sa, NULL);
}

// NOTE: because it writes to the CPUThread directory, CPUThread::CPUThread()
// is NOT THREAD SAFE. Only make new CPUThreads from inside of the main thread,
// and only when no CPUs are running.
CPUThread::CPUThread(OSDomain *cd, unsigned int cpu_id): 
  cpu_id(cpu_id), countdown(0), tickcount(1000000), interval(1000000)
{
  // Add me to the CPUThread directory, needed by the callbacks.
  directory[cpu_id] = this;

  // Init the countdown mutex/condition.
  pthread_mutex_init(&countdown_mutex, NULL);
  pthread_cond_init (&wake_up_thread,  NULL);

  // Init thread, passing thread_main _this_ as its argument.
  pthread_create(&thread, NULL, thread_main, this);

  // Init the countdown mutex and con
}

CPUThread::~CPUThread() {
  pthread_cancel(thread);
}

void CPUThread::run(uint64_t insts) {
  // Add insts to countdown and signal 
  pthread_mutex_lock(&countdown_mutex);
  if (countdown == 0) pthread_cond_signal(&wake_up_thread);
  countdown += insts;
  pthread_mutex_unlock(&countdown_mutex);
}

void CPUThread::ctrl_c() {
  // Cancel running instructions.
  pthread_mutex_lock(&countdown_mutex);
  countdown = 0;
  pthread_mutex_unlock(&countdown_mutex);
}

void CPUThread::set_interval(uint64_t n) { interval = n; }

pthread_barrier_t CPUThread::before_int;
pthread_barrier_t CPUThread::after_int;
map<unsigned, CPUThread*> CPUThread::directory;

void *CPUThread::thread_main(void *arg) {
  CPUThread *this_ = (CPUThread*)arg;

  class {public: bool operator()(CPUThread *this_) {
    bool running = true;

    pthread_mutex_lock(&(this_->countdown_mutex));
    if (this_->countdown == 0) running = false;
    pthread_mutex_unlock(&(this_->countdown_mutex));
    if (this_->tickcount == 0) running = false;
 
    return running;
  }} check_running;

  for (;;) {
    // This thread and asynchronous calls to ctrl_c() are the only things that 
    // can decrease countdown. run() will increase me and wake me up when 
    // there's work to do. As long as there's never any work to do, I'll just 
    // wait here (until the thread is canceled.)
    pthread_mutex_lock(&(this_->countdown_mutex));
    while (this_->countdown == 0) 
      pthread_cond_wait(&(this_->wake_up_thread), &(this_->countdown_mutex));
    pthread_mutex_unlock(&(this_->countdown_mutex));

    if (cd->booted(this_->cpu_id)) do {
      // Run CPU until next timer interrupt or the countdown expires, whichever
      // comes first.
      uint64_t tickcount = this_->tickcount;
      pthread_mutex_lock(&(this_->countdown_mutex));
      uint64_t countdown = this_->countdown;
      pthread_mutex_unlock(&(this_->countdown_mutex));
      uint64_t run_for = (tickcount<countdown)?tickcount:countdown;

      fflush(stdout);
      uint64_t r = cd->run(this_->cpu_id, run_for>MAX_RUN?MAX_RUN:run_for);

      this_->tickcount -= r;
      pthread_mutex_lock(&(this_->countdown_mutex));
      // Make sure we weren't cancelled with ctrl-c while running.
      if (this_->countdown != 0) this_->countdown -= r;
      pthread_mutex_unlock(&(this_->countdown_mutex));
    } while (check_running(this_));

    if (this_->tickcount == 0 || !cd->booted(this_->cpu_id)) {
      // Barrier 1: before the interrupt
      pthread_barrier_wait(&before_int);

      // One CPU may fire a timer interrupt between the barriers.
      if (this_->cpu_id == 0) {
        if (--countdown_to_interrupt == 0) {
          cd->timer_interrupt();
	  countdown_to_interrupt = barriers_per_interrupt;
        }
        if (app_flag && !prof_all_tids) prof_all();
      } 

      // Reset interval.
      this_->tickcount = (this_->interval)/barriers_per_interrupt;

      // Barrier 2: after the interrupt
      pthread_barrier_wait(&after_int);
    }
  }
}

// Always profile the application.
void app_start_cb(int cpu_id) {
  puts("Application started: profiling on.");
  app_flag = true;
}

void app_end_cb  (int cpu_id) {
  if (prof_all_tids) do_report();
  app_flag = false;
}

int main(int argc, char **argv) {
  // Print banner message
  puts(banner);

  // Parse commandline arguments.
  if (argc != 3) {
    printf("Usage:\n  %s <number of CPUs> <kernel image file>\n", argv[0]);
    return 1;
  }

  {
    istringstream n_cpus_sstream(argv[1]);
    n_cpus_sstream >> n_cpus;
    if (n_cpus < 1) {
      puts("Number of CPUs must be at least 1.");
      return 1;
    }
  }

  kernel_filename = argv[2];

  // Set up SIGINT handler.
  setup_signals();

  // Make GNU Readline provide history
  using_history();

  // Allocate the vector where we cache values of RIP
  rip_vec = new uint64_t[n_cpus];

  // Initialize our OSDomain.
  cd = new OSDomain(n_cpus, kernel_filename, 3072);
  cd->set_inst_cb  (rip_inst_cb               );
  cd->set_app_start_cb(app_start_cb);
  cd->set_app_end_cb  (app_end_cb);
  cd->connect_console(std::cout);

  // Initialize the barriers.
  pthread_barrier_init(&CPUThread::before_int, NULL, n_cpus);
  pthread_barrier_init(&CPUThread::after_int,  NULL, n_cpus);

  // Initialize the CPU threads.
  thread_objs.reserve(n_cpus);
  for (unsigned i = 0; i < n_cpus; i++) {
    thread_objs[i] = new CPUThread(cd, i);
  }

  // Main event loop. Parser's got us covered.
  while(yyparse());

  // Clean up.
  //for (unsigned i = 0; i < n_cpus; i++) delete(thread_objs[i]);
  //delete(cd);
  delete[] rip_vec;

  // Exit.
  return 0;
}
