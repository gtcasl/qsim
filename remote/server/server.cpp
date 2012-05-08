#include <iostream>
#include <sstream>
#include <vector>

#include <stdio.h>
#include <qsim.h>
#include <qsim-load.h>
#include <pthread.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <stdint.h>

#include <qsim-net.h>

Qsim::OSDomain *osd;

std::vector<int> tid_vec;
std::vector<Qsim::OSDomain::cpu_mode> mode_vec;
std::vector<Qsim::OSDomain::cpu_prot> prot_vec;
std::vector<bool> idle_vec;

class ServerThread;
ServerThread *st;

// Terrible performance-wise, but effective and simple.
//pthread_mutex_t runlock = PTHREAD_MUTEX_INITIALIZER;

pthread_mutex_t active_connections_lock = PTHREAD_MUTEX_INITIALIZER;
unsigned active_connections(0), cpus;

using namespace QsimNet;

class CallbackAdaptor {
public:
  CallbackAdaptor(unsigned n) : app_running(false) 
    { runmap = new ServerThread *[n](); }

  ~CallbackAdaptor() { delete [] runmap; }

  void start(unsigned i, ServerThread *t) { runmap[i] = t; }
  void finish(unsigned i) { runmap[i] = NULL; }

  int atomic_cb(int c);
  void inst_cb(int c, uint64_t v, uint64_t p, uint8_t l, const uint8_t *b,
               enum inst_type t);
  int int_cb(int c, uint8_t v);
  void mem_cb(int c, uint64_t v, uint64_t p, uint8_t s, int t);
  int magic_cb(int c, uint64_t a);
  void io_cb(int c, uint64_t p, uint8_t s, int t, uint32_t v);
  void reg_cb(int c, int r, uint8_t s, int t);
  void app_end_cb(int c);
  void app_start_cb(int c);

  bool app_running;
  
private:
  ServerThread **runmap;
} *_cba;

class ServerThread {
public:
  ServerThread() : active(false),   atomic_cb_f(false), inst_cb_f(false), 
                   int_cb_f(false), mem_cb_f(false),    magic_cb_f(false), 
                   io_cb_f(false),  app_end_cb_f(false), reg_cb_f(false)
  {
    pthread_mutex_init(&lock, NULL);
  }

  ~ServerThread() {
    if (active) senddata(sock, "XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX", 32);
    pthread_mutex_lock(&lock);
    if (active) close(sock.fd);
    pthread_mutex_unlock(&lock);
  }

  pthread_t pthread;

  bool try_thread(int fd) {
    void *serverthread_main(void*);

    pthread_mutex_lock(&lock);
    if (active) {
      pthread_mutex_unlock(&lock);
      return false;
    }
    active = true;
    sock.fd = fd;
    pthread_mutex_unlock(&lock);

    pthread_create(&pthread, NULL, serverthread_main, (void*)this);

    return true;
  }

  void main_loop() {
    try {
      SockBinStream sbs(&sock);
      sbs << '.';
      while (true) {
        char c;
        sbs >> c;

        switch (c) {
        case 'r': { uint16_t i; uint32_t n;
                    sbs >> i >> n; 
                    _cba->start(i, this);
                    //pthread_mutex_lock(&runlock);
                    n = osd->run(i, n);
                    //pthread_mutex_unlock(&runlock);
                    _cba->finish(i);
                    sbs << '.';
                    sbs << n;
                  }
                  break;
        case 't': { osd->timer_interrupt();
                    sbs << '.';
                  }
                  break;
        case 'i': { uint16_t i; uint8_t vec;
                    sbs >> i >> vec;
                    osd->interrupt(i, vec);
                    sbs << '.';
                  } 
                  break;
        case 'b': { uint16_t i;
                    sbs >> i;
                    sbs << (osd->booted(i)?'T':'F');
                  }
                  break;
        case 's': { char d;
                    sbs >> d;
                    switch (d) {
                    case 'a': atomic_cb_f  = true; break;
                    case 'i': inst_cb_f    = true; break;
                    case 'v': int_cb_f     = true; break;
                    case 'm': mem_cb_f     = true; break;
                    case 'g': magic_cb_f   = true; break;
                    case 'o': io_cb_f      = true; break;
                    case 'e': app_end_cb_f = true; break;
                    case 'r': reg_cb_f     = true; break;
                    default: throw SockBinStreamError();
                    }
                    sbs << '.';
                  } 
                  break;
        case 'u': { char d;
                    sbs >> d;
                    switch (d) {
                    case 'a': atomic_cb_f  = false; break;
                    case 'i': inst_cb_f    = false; break;
                    case 'v': int_cb_f     = false; break;
                    case 'm': mem_cb_f     = false; break;
                    case 'g': magic_cb_f   = false; break;
                    case 'o': io_cb_f      = false; break;
                    case 'e': app_end_cb_f = false; break;
                    case 'r': reg_cb_f     = false; break;
                    default: throw SockBinStreamError();
                    }
                    sbs << '.';
                  }
                  break;
        case 'n': { uint16_t n = cpus;
                    sbs << n;
                  }
                  break;

        default: pthread_mutex_lock(&lock);
                 goto cleanup;
        }
      }
    } catch (SockBinStreamError e) {
      std::cout << "Caught SockBinStreamError!\n";
      pthread_mutex_lock(&lock);
    }

  cleanup:
    if (active) { close(sock.fd); }
    active = false;
    pthread_mutex_lock(&active_connections_lock);
    --active_connections;
    pthread_mutex_unlock(&active_connections_lock);
    pthread_mutex_unlock(&lock);
  }

  bool get_cb_rval() {
    SockBinStream sbs(&sock);
    char c;
    sbs >> c;
    if (c == 'T') return true;
    else return false;
  }

  int atomic_cb(int cpu_id) {
    if (!active || !atomic_cb_f) return 1;

    SockBinStream sbs(&sock);
    uint16_t i = cpu_id;
    sbs << 'a' << i;

    return get_cb_rval();
  }

  void inst_cb(int cpu_id, 
               uint64_t vaddr, uint64_t paddr, uint8_t len, 
               const uint8_t *bytes, enum inst_type type) 
  {
    SockBinStream sbs(&sock);

    int tid = osd->get_tid(cpu_id);
    Qsim::OSDomain::cpu_mode mode = osd->get_mode(cpu_id);
    Qsim::OSDomain::cpu_prot prot = osd->get_prot(cpu_id);
    bool idle = osd->idle(cpu_id);

    if (tid  != tid_vec[cpu_id] ) {
      tid_vec[cpu_id]  = tid; 
      sbs << 'u' << 'T' << (uint16_t)cpu_id << (uint16_t)tid;
    }

    if (mode != mode_vec[cpu_id]) {
      mode_vec[cpu_id] = mode;
      sbs << 'u' << 'M' << (uint16_t)cpu_id << (int8_t)mode;
    }

    if (prot != prot_vec[cpu_id]) {
      prot_vec[cpu_id] = prot;
      sbs << 'u' << 'P' << (uint16_t)cpu_id << (int8_t)prot; 
    }

    if (idle != idle_vec[cpu_id]) {
      idle_vec[cpu_id] = prot;
      sbs << 'u' << 'I' << (uint16_t)cpu_id << (int8_t)idle;
    }

    if (!inst_cb_f) return;

    uint8_t type_b(type);
    uint16_t i(cpu_id);
    sbs << 'i' << i << vaddr << paddr << len << type_b;
    for (int i = 0; i < len; i++) sbs << bytes[i];
  }

  int int_cb(int cpu_id, uint8_t vec) {
    if (!int_cb_f) return 0;

    SockBinStream sbs(&sock);
    uint16_t i = cpu_id;
    sbs << 'v' << i << vec;
    return get_cb_rval();
  }

  void mem_cb(int cpu_id, uint64_t vaddr, uint64_t paddr, uint8_t size, 
              int type) 
  {
    if (!mem_cb_f) return;

    SockBinStream sbs(&sock);
    uint16_t i(cpu_id);
    uint8_t t(type);
    sbs << 'm' << i << vaddr << paddr << size << t;
  }

  int magic_cb(int cpu_id, uint64_t rax) {
    if (!magic_cb_f) return 0;

    SockBinStream sbs(&sock);
    uint16_t i(cpu_id);
    sbs << 'g' << i << rax;
    return get_cb_rval();
  }

  void io_cb(int cpu_id, uint64_t port, uint8_t size, int type, uint32_t val) {
    if (!io_cb_f) return;

    SockBinStream sbs(&sock);
    uint16_t i(cpu_id); 
    uint8_t t(type);
    sbs << 'o' << i << port << size << t << val;
  }

  void reg_cb(int cpu_id, int reg, uint8_t size, int type) {
    if (!reg_cb_f) return;

    SockBinStream sbs(&sock);
    uint16_t i(cpu_id);
    uint32_t r(reg);
    uint8_t  s(size), t(type);
    sbs << 'r' << i << r << s << t;
  }

  void app_end_cb(int cpu_id) {
    if (!app_end_cb_f) return;

    SockBinStream sbs(&sock);
    uint16_t i(cpu_id);
    sbs << 'e' << i;
  }

private:
  bool active, atomic_cb_f, inst_cb_f, int_cb_f, mem_cb_f, magic_cb_f, io_cb_f,
       app_end_cb_f, reg_cb_f;
  pthread_mutex_t lock;
  pthread_cond_t wait_on_connection;
  QsimNet::SockHandle sock;
};

int CallbackAdaptor::atomic_cb(int c) {
  if (runmap[c]) runmap[c]->atomic_cb(c);
}

void CallbackAdaptor::inst_cb
  (int c,uint64_t v,uint64_t p,uint8_t l,const uint8_t *b,enum inst_type t)
{
  if (runmap[c]) runmap[c]->inst_cb(c, v, p, l, b, t);
}

int CallbackAdaptor::int_cb(int c, uint8_t v) {
  if (runmap[c]) runmap[c]->int_cb(c, v);
}

void CallbackAdaptor::mem_cb(int c, uint64_t v, uint64_t p, uint8_t s, int t) {
  if (runmap[c]) runmap[c]->mem_cb(c, v, p, s, t);
}

int CallbackAdaptor::magic_cb(int c, uint64_t a) {
  if (runmap[c]) runmap[c]->magic_cb(c, a);
}

void CallbackAdaptor::io_cb(int c, uint64_t p, uint8_t s, int t, uint32_t v) {
  if (runmap[c]) runmap[c]->io_cb(c, p, s, t, v);
}

void CallbackAdaptor::reg_cb(int c, int r, uint8_t s, int t) {
  if (runmap[c]) runmap[c]->reg_cb(c, r, s, t);
}

void CallbackAdaptor::app_end_cb(int c) {
  if (runmap[c]) runmap[c]->app_end_cb(c);
}

void CallbackAdaptor::app_start_cb(int c) {
  app_running = true;
}

void *serverthread_main(void *arg_vp) {
  ServerThread *arg = (ServerThread*)arg_vp;
  arg->main_loop();
};

template <typename T> 
void arg_convert(T& d, const char* s, unsigned i, unsigned n, const T& def) 
{
  if (i < n) {
    std::istringstream iss(s);
    iss >> d;
  } else {
    d = def;
  }
}

int listen_socket;

void handle_sigint(int) { 
  std::cerr << "QSim server terminating";

  // Cleanup goes here!
  close(listen_socket);
  delete []st;
  delete osd;

  for (unsigned i = 0; i < 10; i++) { std::cerr << '.'; sleep(1); }
  std::cerr << '\n';
  exit(0);
}

void init_sighandler() {
  struct sigaction sa; 
  memset(&sa, 0, sizeof(sa));
  sa.sa_handler = handle_sigint;
  sigaction(SIGINT, &sa, NULL);
}

int main(int argc, char** argv) {
  std::string kernel;
  unsigned ramsz;
  char *port;

  if (argc != 4) {
    std::cout << "Usage: \n  "
              << argv[0] << " <port> <state file> <benchmark>\n";
    return 1;
  }

  port = argv[1];

  // Create the OSDomain
  std::cout << "Loading state...\n";
  osd = new Qsim::OSDomain(argv[2]);
  cpus = osd->get_n();
  osd->connect_console(std::cout);

  // Load the benchmark
  std::cout << "Loading benchmark...\n";
  Qsim::load_file(*osd, argv[3]);

  std::cout << "QSim server ready.\n";

  _cba = new CallbackAdaptor(cpus);

  // Instantiate the server thread objects
  st = new ServerThread[cpus]();

  // Create CPU state vectors
  for (unsigned i = 0; i < cpus; i++) {
    tid_vec.push_back(0);
    mode_vec.push_back(Qsim::OSDomain::cpu_mode(0));
    prot_vec.push_back(Qsim::OSDomain::cpu_prot(0));
    idle_vec.push_back(false);
  }

  // Setup all callbacks.
  osd->set_atomic_cb(_cba, &CallbackAdaptor::atomic_cb);
  osd->set_inst_cb(_cba, &CallbackAdaptor::inst_cb);
  osd->set_int_cb(_cba, &CallbackAdaptor::int_cb);
  osd->set_mem_cb(_cba, &CallbackAdaptor::mem_cb);
  osd->set_magic_cb(_cba, &CallbackAdaptor::magic_cb);
  osd->set_io_cb(_cba, &CallbackAdaptor::io_cb);
  osd->set_reg_cb(_cba, &CallbackAdaptor::reg_cb);
  osd->set_app_end_cb(_cba, &CallbackAdaptor::app_end_cb);

  listen_socket = create_listen_socket(port, cpus);
  init_sighandler();

  while (true) {
    int fd = next_connection(listen_socket);
    pthread_mutex_lock(&active_connections_lock);
 
    // Only allow as many connections as there are QEMU CPUs.
    if (active_connections == cpus) {
      // Refuse connection.
      pthread_mutex_unlock(&active_connections_lock);
      raw_senddata(fd, "x", 1);
      close(fd); 
      continue;
    }
    active_connections++;
    pthread_mutex_unlock(&active_connections_lock);
    { unsigned i = 0; while (!st[i++].try_thread(fd)); }
  }

  return 0;
}
