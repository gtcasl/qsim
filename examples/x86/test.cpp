/*****************************************************************************\
* Qemu Simulation Framework (qsim)                                            *
* Qsim is a modified version of the Qemu emulator (www.qemu.org), coupled     *
* a C++ API, for the use of computer architecture researchers.                *
*                                                                             *
* This work is licensed under the terms of the GNU GPL, version 2. See the    *
* COPYING file in the top-level directory.                                    *
\*****************************************************************************/
#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <map>
#include <vector>

#include <stdint.h>
#include <stdlib.h>

#include <qsim.h>
#include <mgzd.h>

//#include "../statesaver.h"

#include "distorm.h"

using Qsim::OSDomain; using Qsim::Queue;   using Qsim::QueueItem;
using std::cout;      using std::map;      using std::istringstream;
using std::ofstream;  using std::ifstream; using std::pair;
using std::string;    using std::vector;

bool app_started = true;

OSDomain *cd;
int atomic_count = 0;

map <uint64_t, string> sysmap;
string cur_sym;

bool tracing = false;
uint64_t cur_inst_addr;

ofstream tout; // Trace out.

void read_sys_map(const char* mapfile) {
  ifstream sysmap_stream;
  string   t;
  uint64_t addr;
  string   sym;
  
  sysmap_stream.open(mapfile);

  for (;;) {
    sysmap_stream >> std::hex >> addr >> t >> sym;
    if (!sysmap_stream) break;
    sysmap[addr] = sym;
    //cout << "Added 0x" << std::hex << addr << ':' << sym << " to sysmap.\n";
  }

  sysmap_stream.close();
}

int count_insts(Queue &q) {
  int i = 0;
  while (!q.empty()) { if (q.front().type == QueueItem::INST) i++; q.pop(); }
  return i;
}

void trace_insts(int cpu_id, Queue &q) {
  while (!q.empty()) {

    if (q.front().type == QueueItem::INST) {

      uint64_t vaddr = q.front().data.inst.vaddr;
      uint64_t paddr = q.front().data.inst.paddr;
      uint8_t  len   = q.front().data.inst.len  ;
      uint8_t *bytes = q.front().data.inst.bytes;

      cout << std::hex << std::setfill('0') << "CPU " << cpu_id << ": Inst: 0x"
           << std::setw(8) << vaddr << "(0x" << std::setw(8) << paddr << "): ";
    
      _DecodedInst inst[15];
      unsigned int shouldBeOne;
      distorm_decode(0, bytes, len,
                     // This is not entirely correct. It uses the current mode
                     // to determine how to interpret what is already in the
                     // queue.
                     (cd->get_mode(cpu_id) == OSDomain::MODE_PROT)?
                       Decode64Bits:Decode16Bits, 
                     inst, 15, &shouldBeOne);

      if (shouldBeOne == 0) cout << "[Decoding Error] ";
      else                  cout << inst[0].mnemonic.p << ' ' 
                                 << inst[0].operands.p;
      cout << '\n';
    } else if (q.front().type == QueueItem::INTR) {
      cout << std::hex << std::setfill('0') << "CPU " << cpu_id << ": Int 0x"
           << std::setw(2) << q.front().data.intr.vec << '\n';
    }

    q.pop();
  }
}

void boring_inst_cb(int cpu_id, uint64_t vaddr, uint64_t paddr, uint8_t len, const uint8_t *bytes, enum inst_type type) 
{
  if (len == 1 && *bytes == 0xf4) cd->timer_interrupt();
}

void trace_inst_cb(int cpu_id, 
		   uint64_t vaddr,
		   uint64_t paddr,
		   uint8_t len,
		   const uint8_t *bytes,
                   enum inst_type type)
{
  if (len == 1 && *bytes == 0xf4) cd->timer_interrupt();

  static string prev_sym;

  cur_inst_addr = vaddr;

  uint32_t offset;

  map<uint64_t, string>::iterator it = sysmap.upper_bound(vaddr); it--;
  if (it != sysmap.end()) {
    cur_sym = it->second;
    offset = 0;
    offset = vaddr - it->first;
  } else {
    cur_sym = "???";
  }

  uint64_t a, b, c;
  a = cd->get_reg(cpu_id, QSIM_RAX);
  b = cd->get_reg(cpu_id, QSIM_RBX);
  c = cd->get_reg(cpu_id, QSIM_RCX);

  if (/*it != sysmap.end()*/cur_sym != prev_sym) {
    bool kernel = cd->get_prot(cpu_id) == Qsim::OSDomain::PROT_KERN;
    prev_sym = cur_sym;
    tout << "CPU " << cpu_id << ": @" << cur_sym;
    if (offset) tout << " + " << std::dec << offset;
    tout << '(' << std::hex << std::setw(8) << a << ' ' << b << ' ' << c 
         << ") " << (kernel?"[KERNEL]\n":"[USER]\n");
  }

  tout << std::hex << std::setfill('0') << "CPU " << cpu_id << ": Inst: 0x"
       << std::setw(8) << vaddr << "(0x" << std::setw(8) << paddr << "): ";

  _DecodedInst inst[15];
  unsigned int shouldBeOne;
  distorm_decode(0, bytes, len,
                 // This is not entirely correct. It uses the current mode
                 // to determine how to interpret what is already in the
                 // queue.
                 (cd->get_mode(cpu_id) == OSDomain::MODE_PROT)?
                   Decode64Bits:Decode16Bits, 
                 inst, 15, &shouldBeOne);

  if (shouldBeOne == 0) tout << "[Decoding Error] ";
  else                  tout << inst[0].mnemonic.p << ' ' 
                             << inst[0].operands.p;
  tout << '\n';


#if 0
  if (!strcmp((char*)inst[0].mnemonic.p, "IRET")) {
    uint64_t sp = cd->get_reg(cpu_id, QSIM_RSP);
    tout << "^^^ IRET, ESP = 0x" << std::hex << sp << '\n';
  }
#endif
}

void simple_mem_cb(int c, uint64_t v, uint64_t p, uint8_t s, int t) {
  unsigned tid = cd->get_tid(c);
  if (v != 0xf4c60008 && (!tracing || tid != 1)) return;

  uint32_t d;
  cd->mem_rd(d, p);

  if (!tracing) {
    cur_sym    = "";
    uint32_t offset = 0 ;

    if ((cur_inst_addr&0xf0000000) == 0xc0000000) {
      map<uint64_t, string>::iterator i, last_i(0);
      for (i=sysmap.begin(); i!=sysmap.end() && cur_inst_addr>=i->first; i++) {
        last_i = i;
      }

      cur_sym    = last_i->second;
      offset = cur_inst_addr - last_i->first;
    }

    tout << '@' << cur_sym << std::dec; 
    if (offset) tout << " + " << offset << '\n';
    else        tout << '\n';
  }

  tout << std::hex << std::setfill('0') << "CPU " << c << ": Mem:  0x"
       << std::setw(8) << v << "(0x" << std::setw(8) << p << "): " 
       << (unsigned)s << " bytes " << (t?'W':'R') << ": 0x" << std::setw(8) 
       << d << '\n';
}

void app_end_cb(int cpu) {
  app_started = false;
}

struct int_cb_struct {
int my_int_cb(int cpu_id, uint8_t vec) {
  tout << "CPU " << cpu_id << ": Interrupt 0x" << std::hex 
       << std::setw(2) << (unsigned)vec << '\n';
  return 0;
}
int my_magic_cb(int cpu_id, uint64_t rax) {
  if (rax == 0xaaaaaaaa) { 
    app_started = true; 
    return 1; 
  }

  return 0;
}

} int_cb_obj;

int my_atomic_cb(int cpu_id) {
  atomic_count++;

  // Return nonzero to force immediate exit of current CPU.
  return 0;
}

void queue_print_contents(Queue *q) {
  while (!q->empty()) {
    QueueItem &item = q->front();
    switch (item.type) {
    case QueueItem::INST: 
    tout << "Inst: ";
    for (unsigned i = 0; i < item.data.inst.len; i++) 
      tout << std::hex << std::setw(2) << (unsigned)item.data.inst.bytes[i] << ' ';
    tout << '\n';
     break;
    case QueueItem::MEM:  tout << "Mem:  ";
       tout << std::hex << std::setw(8) << item.data.mem.vaddr << ' '
	    << std::dec << (unsigned)item.data.mem.size << '\n';
    break;
    case QueueItem::INTR: std::cerr << "IN\n"; tout << "Int:  " << std::hex << std::setw(2) << item.data.intr.vec << '\n'; break;
    }
  q->pop();
  }
}

int main(int argc, char** argv) {
  unsigned n_cpus = 3;
  vector<Queue*>   queues;
  vector<unsigned> counts;
 
  if (argc == 2) {
    istringstream s(argv[1]);
    s >> n_cpus;
    if (!s) {
      cout << "Usage:\n  " << argv[0] << " [n_cpus]\n";
      exit(1);
    }
  }

  read_sys_map("System.map");
  //read_sys_map("merge.qsim.map");

  // Create a coherence domain with n CPUs, booting linux/bzImage.
  cd = new OSDomain(n_cpus, "linux/bzImage"/*"BOOTED_STATE"*/);
  n_cpus = cd->get_n();
  app_started = true;

  tout.open("PADDR_TRACE.restore");

  // Attach 1 queue to each of these CPUs, firing timer interrupts when HLT
  // instructions are encountered.

  cd->connect_console(std::cout);
  cd->set_magic_cb(&int_cb_obj, &int_cb_struct::my_magic_cb);
  cd->set_app_end_cb(app_end_cb);

  // Fast forward
  //do {
  for (unsigned k = 0; k < 370; k++) {
    for (unsigned i = 0; i < 100; i++) {
      for (unsigned j = 0; j < n_cpus; j++) {
        if (cd->runnable(j)) cd->run(j, 10000);
      }
      //if (app_started) break; 
    }
    //if (app_started) break;
    cd->timer_interrupt();
    //} while (!app_started);
  }
    //Qsim::save_state(*cd, "BOOTED_STATE");

    
  // Set a callback for atomic memory operations, for all CPUs.
  //cd->set_atomic_cb(my_atomic_cb  );
  cd->set_inst_cb  (trace_inst_cb/*boring_inst_cb*/);
  cd->set_int_cb   (&int_cb_obj, &int_cb_struct::my_int_cb  );
  cd->set_mem_cb   (simple_mem_cb );

  // Filter on user mode code from the init process:
  //for (unsigned i = 0; i < n_cpus; i++) {
  //  queues.push_back(new Queue(*cd, i));
  //  queues[i]->set_filt(true, false, true, true, 1);
  //  counts.push_back(0);
  //}
  
  // The main loop. We're responsible for calling the timer interrupt every 
  // 10ms, which means we're simulating exactly 100MIPS.
  //while (app_started) {
  for (unsigned k = 0; k < 1000; k++) {
    for (int j = 0; j < 10; j++) {
      // Run the CPUs in order; we're striving for determinism now.
      for (int i = 0; i < n_cpus; i++) {
	unsigned ran_for = cd->run(i, ((k==25 && j==3)?6391:10000)); //1
        uint64_t last_rip = cd->get_reg(i, QSIM_RIP);
        uint16_t last_tid = cd->get_tid(i);
        bool     kernel   = cd->get_prot(i) == OSDomain::PROT_KERN;
      }
      //cout << ".\n";
    }

    cd->timer_interrupt();
    tout << "---Timer Interrupt---\n"; 
  }

  tout.close();

  return 0;
}
