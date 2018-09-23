/*****************************************************************************\
* Qemu Simulation Framework (qsim)                                            *
* Qsim is a modified version of the Qemu emulator (www.qemu.org), coupled     *
* a C++ API, for the use of computer architecture researchers.                *
*                                                                             *
* This work is licensed under the terms of the GNU GPL, version 2. See the    *
* COPYING file in the top-level directory.                                    *
\*****************************************************************************/
#include <string>
#include <sstream>
#include <iostream>
#include <fstream>
#include <vector>
#include <queue>

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "qsim.h"
#include "mgzd.h"
#include "qsim-vm.h"
#include "qsim-x86-regs.h"

using namespace Qsim;

using std::queue;        using std::vector;        using std::ostream;
using std::stringstream; using std::istringstream; using std::ostringstream;
using std::string;       using std::cerr;          using std::vector;
using std::ofstream;     using std::ifstream;      using std::istream;

// This way autoconf can easily determine that the QSim library is present.
extern "C" {
  void qsim_present() {}
};

template <typename T> static inline void read_header_field(FILE*    f, 
                                                           uint64_t offset, 
                                                           T&       field) 
{
  fseek(f, offset, SEEK_SET);
  size_t ret = fread(&field, sizeof(T), 1, f);
  if (ret == 0)
	  fprintf(stderr, "Read failed\n");
}

__attribute__((unused))
static inline void read_data_chunk(FILE*    f, 
                                   uint64_t offset,
                                   uint8_t* ptr, 
                                   size_t   size) 
{ 
  fseek(f, offset, SEEK_SET);
  size_t ret = fread(ptr, size, 1, f);
  if (ret == 0)
    fprintf(stderr, "Read failed\n");
}

// Find the libqemu-qsim.so library.
string get_qemu_lib(string cpu_type = "x86") {
  string outstr;

  string suffix;
  if (cpu_type == "a64")
    suffix = "/lib/libqemu-qsim-a64.so";
  else
    suffix = "/lib/libqemu-qsim-x86.so";
  const char *qsim_prefix = getenv("QSIM_PREFIX");

  if (!qsim_prefix) qsim_prefix = "/usr/local";

  return string(qsim_prefix) + (suffix);
}

// Simple zero-run compression for state files. We could use libz, but avoiding
// dependencies is the name of the game.
//
// Format: Each zero byte is followed by a 16-bit little endian runlength for
// additional zeros.
void zrun_compress_read(std::istream &f, void *data, size_t n) {
  uint8_t *d((uint8_t*)data);
  const uint8_t *end(d + n);

  while (d < end && f.good()) {
    uint8_t next = f.get();
    *(d++) = next;
    if (next == '\0') {
      uint16_t runlen = uint8_t(f.get()) | (uint8_t(f.get())<<8);
      for (unsigned i = 0; i < runlen && n; ++i) { *(d++) = '\0'; }
    }
  }

  if (d != end) {
    std::cerr << "Zero-run decoding of input failed.\n";
    exit(1);
  }
}

void zrun_compress_write(std::ostream &f, const void *data, size_t n) {
  const uint8_t *d((const uint8_t *)data),
                *end(d + n);

  while (d < end) {
    char next = *(d++);
    f.put(next);
    if (next == '\0') {
      uint16_t count = 0;
      while (d < end && *d == '\0') {
        ++d;
        if (count == 0xffff) {
          f.put(0xff); f.put(0xff); f.put(0x00);
          count = 0;
        } else {
          ++count;
        }
      }
      f.put(count & 0xff); f.put(count>>8);
    }
  }

  if (d != end) {
    std::cerr << "Zero-run encoding of output failed.\n";
    exit(1);
  }
}

// Put the vtable for Cpu here.
Qsim::Cpu::~Cpu() {}

void Qsim::QemuCpu::load_linux(const char* bzImage) {
  // Open bzImage
  FILE *f = fopen(bzImage, "r");
  if (!f) {
    cerr << "Could not open Linux kernel at " << bzImage << ".\n";
    exit(1);
  }

  // Close bzImage
  fclose(f);
}

void Qsim::QemuCpu::load_and_grab_pointers(const char* libfile) {
  // Load the library file                                           
  qemu_lib = Mgzd::open(libfile);
  
  //Get the symbols           
  Mgzd::sym(qemu_init,            qemu_lib, "qemu_init"           );
  Mgzd::sym(qemu_run,             qemu_lib, "run"                 );
  Mgzd::sym(qemu_run_cpu,         qemu_lib, "run_cpu"             );
  Mgzd::sym(qemu_interrupt,       qemu_lib, "interrupt"           );
  Mgzd::sym(qemu_set_atomic_cb,   qemu_lib, "set_atomic_cb"       );
  Mgzd::sym(qemu_set_inst_cb,     qemu_lib, "set_inst_cb"         );
  Mgzd::sym(qemu_set_int_cb,      qemu_lib, "set_int_cb"          );
  Mgzd::sym(qemu_set_mem_cb,      qemu_lib, "set_mem_cb"          );
  Mgzd::sym(qemu_set_magic_cb,    qemu_lib, "set_magic_cb"        );
  Mgzd::sym(qemu_set_io_cb,       qemu_lib, "set_io_cb"           );
  Mgzd::sym(qemu_set_reg_cb,      qemu_lib, "set_reg_cb"          );
  Mgzd::sym(qemu_set_trans_cb,    qemu_lib, "set_trans_cb"        );
  Mgzd::sym(qemu_set_gen_cbs,     qemu_lib, "set_gen_cbs"         );
  Mgzd::sym(qemu_set_sys_cbs,     qemu_lib, "set_sys_cbs"         );
  Mgzd::sym(qemu_get_reg,         qemu_lib, "get_reg"             );
  Mgzd::sym(qemu_set_reg,         qemu_lib, "set_reg"             );
  Mgzd::sym(qemu_mem_rd,          qemu_lib, "mem_rd"              );
  Mgzd::sym(qemu_mem_wr,          qemu_lib, "mem_wr"              );
  Mgzd::sym(qemu_mem_rd_virt,     qemu_lib, "mem_rd_virt"         );
  Mgzd::sym(qemu_mem_wr_virt,     qemu_lib, "mem_wr_virt"         );
  Mgzd::sym(qsim_savevm_state,    qemu_lib, "qsim_savevm_state"   );
  Mgzd::sym(qsim_loadvm_state,    qemu_lib, "qsim_loadvm_state"   );
}

const char** get_qemu_args(const char* kernel, int ram_size, int n_cpus, const string& cpu_type, qsim_mode mode)
{
  string qsim_prefix(getenv("QSIM_PREFIX"));
  qsim_prefix += "/";
  stringstream ncpus_ss;
  ncpus_ss << n_cpus;
  char* ncpus = strdup(ncpus_ss.str().c_str());
  stringstream ramsize_ss;
  ramsize_ss << ram_size;
  char* ramsize = strdup(ramsize_ss.str().c_str());
  string kernel_s(kernel);
  string image_prefix;

  if (cpu_type == "arm32")
    image_prefix = "arm";
  else if (cpu_type == "a64")
    image_prefix = "arm64";
  else if (cpu_type == "x86")
    image_prefix = "x86_64";

  string kernel_path_s = qsim_prefix + "images/" + image_prefix + "_images/vmlinuz";
  string initrd_path_s = qsim_prefix + "images/" + image_prefix + "_images/initrd.img";
  string disk_path_s   = qsim_prefix + "images/" + image_prefix + "_images/" +
                                                  image_prefix + "disk.img";

  char* kernel_path = strdup(kernel_path_s.c_str());
  char* initrd_path = strdup(initrd_path_s.c_str());
  char* disk_path   = strdup(disk_path_s.c_str());

  static const char *argv_interactive_a32[] = {
    "qemu", "-monitor", "/dev/null",
    "-m", ramsize, "-M", "vexpress-a9",
    "-kernel", kernel_path,
    "-initrd", initrd_path,
    "-sd", disk_path,
    "-append", "root=/dev/mmcblk0p2 lpj=34920500",
    NULL
  };

  string a64_img_options_s = "file=" + disk_path_s + ",id=coreimg,cache=unsafe,if=none";
  char* a64_img_options = strdup(a64_img_options_s.c_str());
  static const char *argv_interactive_a64[] = {
    "qemu", 
    "-m", ramsize, "-M", "virt",
    "-cpu", "cortex-a57",
    "-global", "virtio-blk-device.scsi=off",
    "-device", "virtio-scsi-device,id=scsi",
    "-drive",  a64_img_options,
    "-device", "scsi-hd,drive=coreimg",
    "-netdev", "user,id=unet",
    "-device", "virtio-net-device,netdev=unet",
    "-kernel", kernel_path,
    "-initrd", initrd_path,
    "-append", "root=/dev/sda2 nowatchdog rcupdate.rcu_cpu_stall_suppress=1",
    "-nographic",
    "-redir", "tcp:2222::22",
    "-smp", ncpus,
    (mode == QSIM_KVM) ? "--enable-kvm" : NULL,
    NULL
  };

  string bios_path_s = qsim_prefix + "qemu/pc-bios";
  char* bios_path = strdup(bios_path_s.c_str());
  static const char *argv_interactive_x86[] = {
    "qemu", "-no-hpet",
    "-L", bios_path,
    "-m", ramsize,
    "-hda",  disk_path,
    "-kernel", kernel_path,
    "-initrd", initrd_path,
    "-append", "root=/dev/sda1 console=ttyAMA0,115200 console=tty"
    " console=ttyS0 nowatchdog rcupdate.rcu_cpu_stall_suppress=1",
    "-nographic",
    "-redir", "tcp:2223::22",
    "-smp", ncpus,
    (mode == QSIM_KVM) ? "--enable-kvm" : NULL,
    NULL

  };

  //static char *argv_headless_a32[];
  initrd_path_s = qsim_prefix + "initrd/initrd.cpio";
  static const char *argv_headless_x86[] = {
    "qemu", "-no-hpet", "-no-acpi",
    "-L", bios_path,
    "-m", ramsize,
    "-kernel", strdup(kernel),
    "-initrd", strdup((initrd_path_s+".x86").c_str()),
    "-append", "init=/init lpj=34920500 console=ttyS0 console=/dev/ttyS0 notsc"
    " nowatchdog rcupdate.rcu_cpu_stall_suppress=1",
    "-nographic",
    /*
    "-icount", "1,sleep=off",
    "-rtc", "clock=vm",
    */
    "-smp", ncpus,
    (mode == QSIM_KVM) ? "--enable-kvm" : NULL,
    NULL
  };

  static const char *argv_headless_a64[] = {
    "qemu",
    "-m", ramsize, "-M", "virt",
    "-cpu", "cortex-a57",
    "-kernel", strdup(kernel),
    "-initrd", strdup((initrd_path_s+".arm64").c_str()),
    "-append", "init=/init lpj=34920500 console=ttyAMA0 console=ttyS0"
    " nowatchdog rcupdate.rcu_cpu_stall_suppress=1 console=/dev/ttyS0",
    "-nographic",
    "-smp", ncpus,
    (mode == QSIM_KVM) ? "--enable-kvm" : NULL,
    NULL
  };

  if (mode == QSIM_INTERACTIVE) {
    if (cpu_type == "x86")
      return argv_interactive_x86;
    else if (cpu_type == "a64")
      return argv_interactive_a64;
    else
      return argv_interactive_a32;
  } else {
    if (cpu_type == "x86")
      return argv_headless_x86;
    else if (cpu_type == "a64")
      return argv_headless_a64;
  }

  return NULL;
}

Qsim::QemuCpu::QemuCpu(int id, const char* kernel, unsigned ram_mb,
		                   int n_cpus, const string& type, qsim_mode mode)
{
  std::ostringstream ram_size_ss; ram_size_ss << ram_mb << 'M';
  cpu_type = type;

  // Load the library file and get pointers
  load_and_grab_pointers(get_qemu_lib(cpu_type).c_str());

  const char **cmd_argv = get_qemu_args(kernel, ram_mb, n_cpus, type, mode);
  // Initialize Qemu library
  qemu_init(cmd_argv);

  if (cpu_type == "x86") {
    // Load the Linux kernel
    load_linux(kernel);
  }
}

// Create QemuCpu from saved state file
Qsim::QemuCpu::QemuCpu(const char** cmd_argv, const string& type)
{
  cpu_type = type;
  load_and_grab_pointers(get_qemu_lib(cpu_type).c_str());
  qemu_init(cmd_argv);
}

void Qsim::QemuCpu::save_state(const char *filename)
{
  qsim_savevm_state(filename);
}

Qsim::QemuCpu::~QemuCpu() {
  // Close the library file
  Mgzd::close(qemu_lib);
}

vector<OSDomain*> Qsim::OSDomain::osdomains;

void Qsim::OSDomain::assign_id() {
  id = osdomains.size();
  osdomains.push_back(this);
}

Qsim::OSDomain::OSDomain(uint16_t n, string kernel_path, const string& cpu_type,
                         qsim_mode mode_arg, unsigned ram_mb)
  : n_cpus(n), waiting_for_eip(0), mode(mode_arg)
{
  assign_id();

  ram_size_mb = ram_mb;

  if (n > 0) {
    // Create a master CPU using the given kernel
    cpus.push_back(new QemuCpu(id << 16, kernel_path.c_str(), ram_mb, n, cpu_type, mode));
    cpus[0]->set_magic_cb(magic_cb_s);

    // Set master CPU state to "running"
    running.push_back(true);
    tids.push_back(0);
    idlevec.push_back(true);
    for (int i = 1; i < n_cpus; i++) {
      // Initialize Linux task ID to zero and idle to true
      running.push_back(false);
      tids.push_back(0);
      idlevec.push_back(true);
    }
  }
  cmd_argv = get_qemu_args(kernel_path.c_str(), ram_mb, n, cpu_type, mode);
}

void Qsim::OSDomain::init(const char* filename)
{
  assign_id();

  ifstream file(filename);
  if (!file) {
    cerr << "Could not open \"" << filename << "\" for reading.\n";
    exit(1);
  }

  int fd = open(filename, O_RDONLY);
  string fd_arg_s = "fd:" + std::to_string(fd);
  char *fd_arg = strdup(fd_arg_s.c_str());
  string cmd_filename = string(filename) + string(".cmd");

  ifstream cmd_file(cmd_filename);
  if (!cmd_file) {
    cerr << "Could not open \"" << cmd_filename << "\" for reading.\n";
    exit(1);
  }

  string arch, cmd;
  cmd_file >> arch;
  int argc = 0, n_pos = 0, m_pos = 0;
  while (cmd_file >> cmd) {
    argc++;
    if (cmd == "-smp")
      n_pos = argc;
    if (cmd == "-m")
      m_pos = argc;
  }

  if (n_pos == 0 || m_pos == 0) {
    cerr << "Error: SMP/Mem arg not found. Command file " << cmd_filename <<
      " corrupted?" << std::endl;
    exit(1);
  }

  // allocate space for args + incoming fd(2) + icount(2) + clock(2)
  char **cmd_args = (char **)malloc((argc+7)*sizeof(char*));

  // go to the beginning of the arg list
  cmd_file.clear();
  cmd_file.seekg(0, cmd_file.beg);
  cmd_file >> arch;
  argc = 0;
  while (cmd_file >> cmd) {
    cmd_args[argc++] = strdup(cmd.c_str());
  }

  cmd_args[argc] = strdup("-incoming");
  cmd_args[argc+1] = fd_arg;
  cmd_args[argc+2] = strdup("-icount");
  if (arch == "x86")
      cmd_args[argc+3] = strdup("7,sleep=off");
  else
      cmd_args[argc+3] = strdup("7,sleep=off");
  cmd_args[argc+4]= strdup("-rtc");
  cmd_args[argc+5]= strdup("clock=vm");
  cmd_args[argc+6] = NULL;

  cmd_argv = (const char **)cmd_args;

  n_cpus      = strtol(cmd_args[n_pos], NULL, 0);
  ram_size_mb = strtol(cmd_args[m_pos], NULL, 0);

  cpus.push_back(new QemuCpu(cmd_argv, arch));
  cpus[0]->set_magic_cb(magic_cb_s);

  for (int i = 0; i < n_cpus; i++) {
    running.push_back(true);
    tids.push_back(0);
    idlevec.push_back(true);
  }

  mode = QSIM_HEADLESS;
}

// Create an OSDomain from a saved state file
Qsim::OSDomain::OSDomain(const char* filename)
{
  init(filename);
}

// Create an OSDomain from a saved state file.
Qsim::OSDomain::OSDomain(int n, const char* filename)
{
  init(filename);
  if (n != n_cpus) {
    cerr << "Error: State file passed has " << n << " cpus, but " << n_cpus <<
      " specified" << std::endl;
    exit(1);
  }
}

void Qsim::OSDomain::save_state(const char* filename) {
  cpus[0]->save_state(filename);

  string cmd_filename = string(filename) + string(".cmd");
  ofstream cmd_file(cmd_filename);

  cmd_file << cpus[0]->getCpuType() << std::endl;
  for (int argc = 0; cmd_argv[argc] != NULL; argc++) {
    // skip kernel initrd, append and drive args
    if (!(strcmp(cmd_argv[argc], "-kernel") &&
          strcmp(cmd_argv[argc], "-initrd") &&
          strcmp(cmd_argv[argc], "-drive")  &&
          strcmp(cmd_argv[argc], "-append"))) {
      argc++;
      continue;
    }

    cmd_file << cmd_argv[argc] << " ";
  }

  cmd_file.close();
}

int Qsim::OSDomain::get_tid(uint16_t i) {
  if (!running[i]) return -1;
  else return tids[i];
}

Qsim::OSDomain::cpu_mode Qsim::OSDomain::get_mode(uint16_t i) {
  bool prot = (cpus[0]->get_reg(i, QSIM_X86_CR0))&1;

  return prot?MODE_PROT:MODE_REAL;
}

Qsim::OSDomain::cpu_prot Qsim::OSDomain::get_prot(uint16_t i) {
  bool user = (cpus[0]->get_reg(i, QSIM_X86_CS))&1;

  return user?PROT_USER:PROT_KERN;
}

string Qsim::OSDomain::getCpuType(uint16_t i) {
  return cpus[i]->getCpuType();
}

unsigned Qsim::OSDomain::run(uint16_t i, unsigned n) {
  if (running[i]) { return cpus[0]->run(i, n); }

  return 0;
}

unsigned Qsim::OSDomain::run(unsigned n) {
  if (running[0]) { return cpus[0]->run(n); } 

  return 0;
}

void Qsim::OSDomain::connect_console(std::ostream& s) {
  consoles.push_back(&s);
}

void Qsim::OSDomain::timer_interrupt() {
  if (n_cpus > 1 && running[0] && running[1]) {
    for (unsigned i = 0; i < n_cpus; i++) if (running[i]) {
      cpus[0]->interrupt(0xef);
    }
  } else {
    cpus[0]->interrupt(0x30);
  }
}

void Qsim::OSDomain::set_inst_cb  (inst_cb_t   cb) {
  cpus[0]->set_inst_cb  (cb);
}

void Qsim::OSDomain::set_brinst_cb  (brinst_cb_t cb) {
  cpus[0]->set_brinst_cb  (cb);
}

void Qsim::OSDomain::set_mem_cb   (mem_cb_t    cb) {
  cpus[0]->set_mem_cb   (cb);
}

void Qsim::OSDomain::set_int_cb   (int_cb_t    cb) {
  cpus[0]->set_int_cb   (cb);
}

void Qsim::OSDomain::set_io_cb    (io_cb_t     cb) {
  cpus[0]->set_io_cb    (cb);
}

void Qsim::OSDomain::set_atomic_cb(atomic_cb_t cb) {
  cpus[0]->set_atomic_cb(cb);
}

void Qsim::OSDomain::set_reg_cb(reg_cb_t cb) {
  cpus[0]->set_reg_cb(cb);
}

void Qsim::OSDomain::set_trans_cb(trans_cb_t cb) {
  cpus[0]->set_trans_cb(cb);
}

void Qsim::OSDomain::set_gen_cbs(bool state) {
  cpus[0]->set_gen_cbs(state);
}

void Qsim::OSDomain::set_sys_cbs(bool state) {
  cpus[0]->set_sys_cbs(state);
}

Qsim::OSDomain::~OSDomain() {
  // Destroy the callback objects.
  for (unsigned i = 0; i < atomic_cbs.size(); ++i) delete atomic_cbs[i];
  for (unsigned i = 0; i < io_cbs.size(); ++i) delete io_cbs[i];
  for (unsigned i = 0; i < mem_cbs.size(); ++i) delete mem_cbs[i];
  for (unsigned i = 0; i < int_cbs.size(); ++i) delete int_cbs[i];
  for (unsigned i = 0; i < inst_cbs.size(); ++i) delete inst_cbs[i];
  for (unsigned i = 0; i < start_cbs.size(); ++i) delete start_cbs[i];
  for (unsigned i = 0; i < end_cbs.size(); ++i) delete end_cbs[i];
  for (unsigned i = 0; i < magic_cbs.size(); ++i) delete magic_cbs[i];

  // Destroy the CPUs.
  delete cpus[0];
  //for (unsigned i = 0; i < n; i++) delete cpus[i];
}

void Qsim::OSDomain::unset_atomic_cb(atomic_cb_handle_t h) {
  atomic_cbs.erase(h);
}

void Qsim::OSDomain::unset_magic_cb(magic_cb_handle_t h) {
  magic_cbs.erase(h);
}

void Qsim::OSDomain::unset_io_cb(io_cb_handle_t h) {
  io_cbs.erase(h);
}

void Qsim::OSDomain::unset_mem_cb(mem_cb_handle_t h) {
  mem_cbs.erase(h);
}

void Qsim::OSDomain::unset_inst_cb(inst_cb_handle_t h) {
  inst_cbs.erase(h);
}

void Qsim::OSDomain::unset_brinst_cb(brinst_cb_handle_t h) {
  brinst_cbs.erase(h);
}

void Qsim::OSDomain::unset_reg_cb(reg_cb_handle_t h) {
  reg_cbs.erase(h);
}

void Qsim::OSDomain::unset_trans_cb(trans_cb_handle_t h) {
  trans_cbs.erase(h);
}

void Qsim::OSDomain::unset_app_start_cb(start_cb_handle_t h) {
  start_cbs.erase(h);
}

void Qsim::OSDomain::unset_app_end_cb(end_cb_handle_t h) {
  end_cbs.erase(h);
}

int Qsim::OSDomain::atomic_cb_s(int cpu_id) {
  osdomains[cpu_id >> 16]->atomic_cb(cpu_id & 0xffff);

  return 0;
}

int Qsim::OSDomain::atomic_cb(int cpu_id) {
  std::vector<atomic_cb_obj_base*>::iterator i;

  int rval = 0;

  // Logical OR the output of all the registered callbacks. If at least one
  // demands we stop, we must stop.
  for (i = atomic_cbs.begin(); i != atomic_cbs.end(); ++i) {
    if ( (**i)(cpu_id) ) rval = 1;
  }

  return rval;
}

void Qsim::OSDomain::inst_cb_s(int cpu_id, uint64_t va, uint64_t pa,
                               uint8_t l, const uint8_t *bytes,
                               enum inst_type type)
{
  osdomains[cpu_id >> 16]->inst_cb(cpu_id & 0xffff, va, pa, l, bytes, type);
}

void Qsim::OSDomain::brinst_cb_s(int cpu_id, uint64_t va, uint64_t pa,
                               uint8_t l, const uint8_t *bytes,
                               enum inst_type type)
{
  osdomains[cpu_id >> 16]->brinst_cb(cpu_id & 0xffff, va, pa, l, bytes, type);
}

void Qsim::OSDomain::brinst_cb(int cpu_id, uint64_t va, uint64_t pa, 
                             uint8_t l, const uint8_t *bytes, 
                             enum inst_type type)
{
  std::vector<brinst_cb_obj_base*>::iterator i;

  // Just iterate through the callbacks and call them all.
  for (i = brinst_cbs.begin(); i != brinst_cbs.end(); ++i)
    (**i)(cpu_id, va, pa, l, bytes, type);
}

void Qsim::OSDomain::inst_cb(int cpu_id, uint64_t va, uint64_t pa, 
                             uint8_t l, const uint8_t *bytes, 
                             enum inst_type type)
{
  std::vector<inst_cb_obj_base*>::iterator i;

  // Just iterate through the callbacks and call them all.
  for (i = inst_cbs.begin(); i != inst_cbs.end(); ++i)
    (**i)(cpu_id, va, pa, l, bytes, type);
}

void Qsim::OSDomain::mem_cb_s(int cpu_id, uint64_t va, uint64_t pa,
                             uint8_t s, int type)
{
  osdomains[cpu_id >> 16]->mem_cb(cpu_id & 0xffff, va, pa, s, type);
}

void Qsim::OSDomain::mem_cb(int cpu_id, uint64_t va, uint64_t pa,
			   uint8_t s, int type) {
  std::vector<mem_cb_obj_base*>::iterator i;

  for (i = mem_cbs.begin(); i != mem_cbs.end(); ++i)
    (**i)(cpu_id, va, pa, s, type);
}

uint32_t *Qsim::OSDomain::io_cb_s(int cpu_id, uint64_t port, uint8_t s,
                                  int type, uint32_t data)
{
  return osdomains[cpu_id >> 16]->io_cb(cpu_id & 0xffff, port, s, type, data);
}

uint32_t *Qsim::OSDomain::io_cb(int cpu_id, uint64_t port, uint8_t s, 
			  int type, uint32_t data) {
  std::vector<io_cb_obj_base*>::iterator i;

  for (i = io_cbs.begin(); i != io_cbs.end(); ++i) {
    (**i)(cpu_id, port, s, type, data);
  }

  return 0;
}

int Qsim::OSDomain::int_cb_s(int cpu_id, uint8_t vec) {
  return osdomains[cpu_id >> 16]->int_cb(cpu_id & 0xffff, vec);
}

int Qsim::OSDomain::int_cb(int cpu_id, uint8_t vec) {
  std::vector<int_cb_obj_base*>::iterator i;

  int rval = 0;

  // Logical OR the output of all the registered callbacks.
  for (i = int_cbs.begin(); i != int_cbs.end(); ++i)
    if ((**i)(cpu_id, vec)) rval = 1;

  return rval;
}

void Qsim::OSDomain::reg_cb_s(int cpu_id, int reg, uint8_t size, int type) {
  osdomains[cpu_id >> 16]->reg_cb(cpu_id & 0xffff, reg, size, type);
}

void Qsim::OSDomain::reg_cb(int cpu_id, int reg, uint8_t size, int type) {
  std::vector<reg_cb_obj_base*>::iterator i;
  for (i = reg_cbs.begin(); i != reg_cbs.end(); ++i)
    (**i)(cpu_id, reg, size, type);
}

void Qsim::OSDomain::trans_cb_s(int cpu_id) {
  osdomains[cpu_id >> 16]->trans_cb(cpu_id & 0xffff);
}

void Qsim::OSDomain::trans_cb(int cpu_id) {
  cpu_id &= 0xffff;

  std::vector<trans_cb_obj_base*>::iterator i;
  for (i = trans_cbs.begin(); i != trans_cbs.end(); ++i)
    (**i)(cpu_id);
}

int Qsim::OSDomain::magic_cb_s(int cpu_id, uint64_t rax) {
  return osdomains[cpu_id >> 16]->magic_cb(cpu_id & 0xffff, rax);
}

int Qsim::OSDomain::magic_cb(int cpu_id, uint64_t rax) {
  int rval = 0;

  // Start by calling other registered magic instruction callbacks. 
  std::vector<magic_cb_obj_base*>::iterator i;
 
  for (i = magic_cbs.begin(); i != magic_cbs.end(); ++i)
    if ((**i)(cpu_id, rax)) rval = 1;

  // If this is a "CD Ignore" magic instruction, ignore it.
  if ((rax&0xffff0000) == 0xcd160000) return rval;

  // Take appropriate action
  if ( (rax&0xffffff00) == 0xc501e000 ) {
    // Console output
    char c = rax & 0xff;
    if (isprint(c)) {
      linebuf += c;
    }
    if (c == '\n') {
      std::vector<std::ostream *>::iterator i;
      for (i = consoles.begin(); i != consoles.end(); i++) {
        **i << linebuf << '\n';
      }
      linebuf = "";
    }
  } else if ( (rax & 0xffffffff) == 0x1d1e1d1e ) {
    // This CPU is now in the idle loop.
    idlevec[cpu_id] = true;
  } else if ( (rax & 0xffff0000) == 0xc75c0000 ) {
    // Context switch
    idlevec[cpu_id] = false;
    tids[cpu_id] = rax & 0xffff;
  } else if ( (rax & 0xffff0000) == 0xb0070000 ) {
    // CPU bootstrap
    running[rax&0xffff] = true;
  } else if ( (rax & 0xff000000) == 0x1d000000 ) {
    // Inter-processor interrupt
    uint16_t cpu = (rax & 0x00ffff00)>>8;
    uint8_t  vec = (rax & 0x000000ff);
    rval = cpus[cpu]->interrupt(vec);
  } else if ( (rax & 0xffffffff) == 0xc7c7c7c7 ) {
    // CPU count request
    cpus[cpu_id]->set_reg(cpu_id, QSIM_X86_RAX, (uint64_t)n_cpus);
  } else if ( (rax & 0xffffffff) == 0x512e512e ) {
    // RAM size request
    //cpus[cpu_id]->set_reg(QSIM_RAX, ram_size_mb);
  } else if ( (rax & 0xffffffff) == 0xaaaaaaaa ) {
    // Application start marker.
    std::vector<start_cb_obj_base*>::iterator i;
    for (i = start_cbs.begin(); i != start_cbs.end(); ++i) {
      if ((**i)(cpu_id)) rval = 1;
    }

  } else if ( (rax & 0xffffffff) == 0xfa11dead ) {
    // Shutdown/application end marker.
    std::vector<end_cb_obj_base*>::iterator i;
    for (i = end_cbs.begin(); i != end_cbs.end(); ++i) {
      if ((**i)(cpu_id)) rval = 1;
    }
    if (mode == QSIM_HEADLESS) {
      for (unsigned i = 0; i < n_cpus; i++) running[i] = false;
    }
  } else if ( (rax & 0xfffffff0) != 0x00000000 &&
              (rax & 0xfffffff0) != 0x80000000 &&
              (rax & 0xfffffff0) != 0x40000000 ) {
    // Unknown CPUID
    // Could throw an exception. For now do nothing.
  }

  return rval;
}

void Qsim::OSDomain::lock_addr(uint64_t pa) {}
void Qsim::OSDomain::unlock_addr(uint64_t pa) {}
std::vector<Queue*> *Qsim::Queue::queues;

Qsim::Queue::Queue(OSDomain &cd, int cpu, bool h): cd(&cd), cpu(cpu), hlt(h) {
  // Create a way for the callbacks to find us.
  if (queues == NULL)
    queues = new vector<Queue*>(cd.get_n());
  (*queues)[cpu] = this;
  // Connect the callbacks
  cd.set_inst_cb(cpu, h?inst_cb_hlt:inst_cb);
  cd.set_brinst_cb(cpu, h?brinst_cb_hlt:brinst_cb);
  cd.set_mem_cb (cpu, mem_cb               );
  cd.set_int_cb (cpu, int_cb               );
}

Qsim::Queue::~Queue() {
  // Disconnect the callbacks
  cd->set_inst_cb(cpu, NULL);
  cd->set_mem_cb (cpu, NULL);
  cd->set_int_cb (cpu, NULL);

  // Remove this queue from the vector.
  (*queues)[cpu] = NULL;

  // If there are no queues left, delete queues.
  bool queues_all_null = true;
  for (int i = 0; i < cd->get_n(); i++) {
    if ((*queues)[i]) queues_all_null = false;
  }
  if (queues_all_null) delete queues;
}

void Qsim::Queue::set_filt(bool user, bool krnl, bool prot,
                           bool real, int tid)
{
  // Set filtering variables
  flt_tid = tid;
  flt_krnl = krnl;
  flt_prot = prot;
  flt_real = real;
  flt_user = user;

  // Set callbacks appropriately
  if (flt_krnl && flt_prot && flt_real && flt_user && flt_tid == -1) {
    cd->set_inst_cb(cpu, hlt?inst_cb_hlt:inst_cb);
    cd->set_brinst_cb(cpu, hlt?brinst_cb_hlt:brinst_cb);
    cd->set_mem_cb (cpu, mem_cb                 );
    cd->set_int_cb (cpu, int_cb                 );
  } else {
    cd->set_inst_cb(cpu, inst_cb_flt            );
    cd->set_brinst_cb(cpu, brinst_cb_flt         );
    cd->set_mem_cb (cpu, mem_cb_flt             );
    cd->set_int_cb (cpu, int_cb_flt             );
  }
}


void Qsim::Queue::inst_cb(int cpu_id,
                          uint64_t vaddr,
                          uint64_t paddr,
                          uint8_t len,
                          const uint8_t *bytes,
                          enum inst_type type)
{
  (*queues)[cpu_id]->push(QueueItem(cpu_id, vaddr, paddr, len, bytes, type));
}

void Qsim::Queue::brinst_cb(int cpu_id,
                          uint64_t vaddr,
                          uint64_t paddr,
                          uint8_t len,
                          const uint8_t *bytes,
                          enum inst_type type)
{
  (*queues)[cpu_id]->push(QueueItem(cpu_id, vaddr, paddr, len, bytes, type));
}

void Qsim::Queue::inst_cb_hlt(int cpu_id,
                              uint64_t vaddr,
                              uint64_t paddr,
                              uint8_t len,
                              const uint8_t *bytes,
                              enum inst_type type)
{
  if (len == 1 && *bytes == 0xf4) (*queues)[cpu_id]->cd->timer_interrupt();
  (*queues)[cpu_id]->push(QueueItem(cpu_id, vaddr, paddr, len, bytes, type));
}

void Qsim::Queue::brinst_cb_hlt(int cpu_id,
                              uint64_t vaddr,
                              uint64_t paddr,
                              uint8_t len,
                              const uint8_t *bytes,
                              enum inst_type type)
{
  if (len == 1 && *bytes == 0xf4) (*queues)[cpu_id]->cd->timer_interrupt();
  (*queues)[cpu_id]->push(QueueItem(cpu_id, vaddr, paddr, len, bytes, type));
}



void Qsim::Queue::inst_cb_flt(int cpu_id,
                              uint64_t vaddr,
                              uint64_t paddr,
                              uint8_t len,
                              const uint8_t *bytes,
                              enum inst_type type)
{
  Queue   *q        = (*queues)[cpu_id];
  OSDomain *cd       = q->cd      ;
  int      cpu      = q->cpu     ;
  int      flt_tid  = q->flt_tid ;
  bool     flt_krnl = q->flt_krnl;
  bool     flt_user = q->flt_user;
  bool     flt_prot = q->flt_prot;
  bool     flt_real = q->flt_real;
  if ( (flt_tid == -1 || cd->get_tid (cpu) == flt_tid           ) && (
         (flt_krnl      && cd->get_prot(cpu) == OSDomain::PROT_KERN) ||
         (flt_user      && cd->get_prot(cpu) == OSDomain::PROT_USER) ||
         (flt_prot      && cd->get_mode(cpu) == OSDomain::MODE_PROT) ||
         (flt_real      && cd->get_mode(cpu) == OSDomain::MODE_REAL)))
    (*queues)[cpu_id]->push(QueueItem(cpu_id, vaddr, paddr, len, bytes, type));
  if (q->hlt && len == 1 && *bytes == 0xf4) cd->timer_interrupt();
}

void Qsim::Queue::brinst_cb_flt(int cpu_id,
                              uint64_t vaddr,
                              uint64_t paddr,
                              uint8_t len,
                              const uint8_t *bytes,
                              enum inst_type type)
{
  Queue   *q        = (*queues)[cpu_id];
  OSDomain *cd       = q->cd      ;
  int      cpu      = q->cpu     ;
  int      flt_tid  = q->flt_tid ;
  bool     flt_krnl = q->flt_krnl;
  bool     flt_user = q->flt_user;
  bool     flt_prot = q->flt_prot;
  bool     flt_real = q->flt_real;
  if ( (flt_tid == -1 || cd->get_tid (cpu) == flt_tid           ) && (
         (flt_krnl      && cd->get_prot(cpu) == OSDomain::PROT_KERN) ||
         (flt_user      && cd->get_prot(cpu) == OSDomain::PROT_USER) ||
         (flt_prot      && cd->get_mode(cpu) == OSDomain::MODE_PROT) ||
         (flt_real      && cd->get_mode(cpu) == OSDomain::MODE_REAL)))
    (*queues)[cpu_id]->push(QueueItem(cpu_id, vaddr, paddr, len, bytes, type));
  if (q->hlt && len == 1 && *bytes == 0xf4) cd->timer_interrupt();
}

void Qsim::Queue::mem_cb(int cpu_id,
                         uint64_t vaddr,
                         uint64_t paddr,
                         uint8_t size,
                         int type)
{
  (*queues)[cpu_id]->push(QueueItem(cpu_id, vaddr, paddr, size, type));
}

void Qsim::Queue::mem_cb_flt(int cpu_id,
                             uint64_t vaddr,
                             uint64_t paddr,
                             uint8_t size,
                             int type)
{
  Queue   *q        = (*queues)[cpu_id];
  OSDomain *cd       = q->cd      ;
  int      cpu      = q->cpu     ;
  int      flt_tid  = q->flt_tid ;
  bool     flt_krnl = q->flt_krnl;
  bool     flt_user = q->flt_user;
  bool     flt_prot = q->flt_prot;
  bool     flt_real = q->flt_real;
  if ( (flt_tid == -1 || cd->get_tid (cpu) == flt_tid           ) && (
         (flt_krnl      && cd->get_prot(cpu) == OSDomain::PROT_KERN) ||
         (flt_user      && cd->get_prot(cpu) == OSDomain::PROT_USER) ||
         (flt_prot      && cd->get_mode(cpu) == OSDomain::MODE_PROT) ||
	 (flt_real      && cd->get_mode(cpu) == OSDomain::MODE_REAL)))
    (*queues)[cpu_id]->push(QueueItem(cpu_id, vaddr, paddr, size, type));
}

int Qsim::Queue::int_cb(int cpu_id, uint8_t vec)
{
  (*queues)[cpu_id]->push(QueueItem(cpu_id, vec));
  return 0;
}

int Qsim::Queue::int_cb_flt(int cpu_id, uint8_t vec)
{
  Queue   *q        = (*queues)[cpu_id];
  OSDomain *cd       = q->cd      ;
  int      cpu      = q->cpu     ;
  int      flt_tid  = q->flt_tid ;
  bool     flt_krnl = q->flt_krnl;
  bool     flt_user = q->flt_user;
  bool     flt_prot = q->flt_prot;
  bool     flt_real = q->flt_real;
  if ( (flt_tid == -1 || cd->get_tid (cpu) == flt_tid           ) && (
         (flt_krnl      && cd->get_prot(cpu) == OSDomain::PROT_KERN) ||
         (flt_user      && cd->get_prot(cpu) == OSDomain::PROT_USER) ||
         (flt_prot      && cd->get_mode(cpu) == OSDomain::MODE_PROT) ||
         (flt_real      && cd->get_mode(cpu) == OSDomain::MODE_REAL)))
    (*queues)[cpu_id]->push(QueueItem(cpu_id, vec));
  return 0;
}
