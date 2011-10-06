#include <qsim.h>
#include <iostream>
#include <vector>

#include "qsim-client.h"

using std::cout;

struct CbTester {
  int atomic(int cpu) {
    return 1;
  }
  void inst(int cpu, uint64_t va, uint64_t pa, uint8_t l, const uint8_t *b) {
    static unsigned count = 0;
    if (count++ == 10000) count = 0; else return;
    cout << "Instruction, CPU " << cpu << " va=0x" << std::hex << va << ": ";
    for (unsigned i = 0; i < l; i++) 
      cout << std::hex << std::setw(0) << std::setfill('0') << (b[i]&0xff) 
           << ' ';
    cout << '\n';
  }
  void mem(int cpu, uint64_t va, uint64_t pa, uint8_t s, int t) {
    static unsigned count = 0; 
    if (count++ == 10000) count = 0; else return;
    cout << "Memory op, CPU " << cpu << " va=0x" << std::hex << va << " size="
         << (s&0xff) << ' ' << (t?'W':'R') << '\n';
  }
} cbtest;

int main(int argc, char **argv) {
  if (argc != 3) {
    cout << "Usage: " << argv[0] << " <server> <port>\n";
    return 1;
  }

  Qsim::Client *qc = new Qsim::Client(client_socket(argv[1], argv[2]));

  qc->set_atomic_cb(&cbtest, &CbTester::atomic);
  qc->set_inst_cb(&cbtest, &CbTester::inst);
  qc->set_mem_cb (&cbtest, &CbTester::mem );

  cout << "Server has " << qc->get_n() << " CPUs.\n";
  for (unsigned i = 0; i < qc->get_n(); i++) {
    cout << "  CPU " << i << " is " << (qc->booted(i)?"booted.\n":"offline.\n");
    cout << "  Running TID " << qc->get_tid(i) << '\n';
    cout << "  " << ((qc->get_prot(i))?"Kernel mode.\n":"User mode.\n");
    cout << "  " << ((qc->get_mode(i))?"Protected mode.\n":"Real mode.\n");
  }

  unsigned *inst_countdown = new unsigned[sizeof(unsigned)*qc->get_n()];

  for (unsigned i = 0; i < 10000; i++) {
    for (unsigned j = 0; j < qc->get_n(); j++)
      inst_countdown[j] = (qc->booted(j)?10000000:0);

    bool running;
    do {
      for (unsigned j = 0; j < qc->get_n(); j++)
        inst_countdown[j] -= qc->run(j, inst_countdown[j]);
    
      running = false;
      for (unsigned j = 0; j < qc->get_n(); j++) 
        if (inst_countdown[j] != 0) running = true;
    } while(running);

    qc->timer_interrupt();
  }

  delete inst_countdown;
  delete qc;

  return 0;
}
