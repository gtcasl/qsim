#ifndef __QSIM_H
#define __QSIM_H
/*****************************************************************************\
* Qemu Simulation Framework (qsim)                                            *
* Qsim is a modified version of the Qemu emulator (www.qemu.org), coupled     *
* a C++ API, for the use of computer architecture researchers.                *
*                                                                             *
* This work is licensed under the terms of the GNU GPL, version 2. See the    *
* COPYING file in the top-level directory.                                    *
\*****************************************************************************/
#include <map>
#include <vector>
#include <sstream>
#include <string>
#include <queue>
#include <stdint.h>
#include <string.h>

#include "qsim-vm.h"
#include "qsim-regs.h"
#include "mgzd.h"

namespace Qsim {
  struct QueueItem {
    // Constructors for use within Queue; automatically set type field to
    // appropriate value.
    QueueItem() { id = -1; } 

    QueueItem(int core, uint64_t vadr, uint64_t padr, uint8_t len, const uint8_t *bytes, enum inst_type type):
      cb_type(INST)
    {
      data.inst.vaddr = vadr;
      data.inst.paddr = padr;
      data.inst.len   = len;
      memcpy((void *)data.inst.bytes, (const void *)bytes, len);
      data.inst.type  = type;
      id = core;
    }

    QueueItem(int core, uint64_t vaddr, uint64_t paddr, uint8_t size, int type):
      cb_type(MEM)
    {
      data.mem.vaddr  = vaddr;
      data.mem.paddr  = paddr;
      data.mem.size   = size;
      data.mem.type   = type;
      id = core;
    }

    QueueItem(int core, uint8_t vec):
      cb_type(INTR)
    {
      data.intr.vec   = vec;
      id = core;
    }

    QueueItem(int core, int reg, uint8_t size, int type):
      cb_type(REG)
    {
      data.reg.reg    = reg;
      data.reg.size   = size;
      data.reg.type   = type;
      id = core;
    }

    // TERMINATED/IDEL are used by qsim_proxy in Manifold
    enum {INST, MEM, INTR, REG, TERMINATED, IDLE} cb_type;
    union {
      struct {
        uint64_t vaddr; uint64_t paddr; uint8_t len ; uint8_t  bytes[15]; enum inst_type type; 
      } inst;
      struct {
        uint64_t vaddr; uint64_t paddr; uint8_t size; int type ;
      } mem;
      struct {
        uint8_t    vec;
      } intr;
      struct {
          int reg; uint8_t size; int type;
      } reg;
    } data;

    int id;
  };

  class Cpu {
  public:
    // Initialize with named parameter set p.
    Cpu() {}
    virtual ~Cpu();

    // Run for n instructions.
    virtual uint64_t run(unsigned n) = 0;
    virtual uint64_t run(int cpu, unsigned n) = 0;

    // Trigger an interrupt with vector v.
    virtual int interrupt(uint8_t v) = 0;

    // Set appropriate callbacks.
    virtual void set_atomic_cb(atomic_cb_t cb) = 0;
    virtual void set_magic_cb (magic_cb_t  cb) = 0;
    virtual void set_int_cb   (int_cb_t    cb) = 0;
    virtual void set_inst_cb  (inst_cb_t   cb) = 0;
    virtual void set_brinst_cb(brinst_cb_t cb) = 0;
    virtual void set_mem_cb   (mem_cb_t    cb) = 0;
    virtual void set_reg_cb   (reg_cb_t    cb) = 0;
  };

  class QemuCpu : public Cpu {
  private:
    std::string cpu_type;

    // The qemu library object                                                 
    Mgzd::lib_t qemu_lib;

    // Function pointers into the qemu library                                 
    void     (*qemu_init)(const char** argv);
    uint64_t (*qemu_run)(uint64_t n);
    uint64_t (*qemu_run_cpu)(int c, uint64_t n);
    int      (*qemu_interrupt)(uint8_t vec);

    void (*qemu_set_atomic_cb)(atomic_cb_t);
    void (*qemu_set_inst_cb)  (inst_cb_t  );
    void (*qemu_set_brinst_cb)(brinst_cb_t);
    void (*qemu_set_int_cb)   (int_cb_t   );
    void (*qemu_set_mem_cb)   (mem_cb_t   );
    void (*qemu_set_magic_cb) (magic_cb_t );
    void (*qemu_set_io_cb)    (io_cb_t    );
    void (*qemu_set_reg_cb)   (reg_cb_t   );
    void (*qemu_set_trans_cb) (trans_cb_t );
    void (*qemu_set_gen_cbs)  (bool state);
    void (*qemu_set_sys_cbs)  (bool state);

    uint64_t (*qemu_get_reg) (int c, int r);
    void     (*qemu_set_reg) (int c, int r, uint64_t val );

    uint8_t  (*qemu_mem_rd)  (uint64_t paddr);
    void     (*qemu_mem_wr)  (uint64_t paddr, uint8_t data);
    uint8_t  (*qemu_mem_rd_virt) (int c, uint64_t vaddr);
    void     (*qemu_mem_wr_virt) (int c, uint64_t vaddr, uint8_t data);

    int      (*qsim_savevm_state) (const char *filename);
    int      (*qsim_loadvm_state) (const char *filename);

    void load_and_grab_pointers(const char *libfile);

    // Load Linux from bzImage into QEMU RAM
    void load_linux(const char* bzImage);

  public:
    QemuCpu(int id, const char* kernel, unsigned ram_mb = 1024, int n_cpus = 1, const std::string& cpu_type = "x86", qsim_mode mode = QSIM_HEADLESS);
    QemuCpu(const char** args, const std::string& type);
    virtual ~QemuCpu();
 
    uint64_t run(unsigned n) { return qemu_run(n); }
    uint64_t run(int c, unsigned n) { return qemu_run_cpu(c, n); }

    std::string getCpuType() { return cpu_type; }
    void setCpuType(std::string arch) { cpu_type = arch; }

    // Save state to file.
    void save_state(const char *file);

    virtual void set_atomic_cb(atomic_cb_t cb) { 
      qemu_set_atomic_cb(cb); 
    }

    virtual void set_inst_cb  (inst_cb_t  cb) { 
      qemu_set_inst_cb  (cb); 
    }

    virtual void set_brinst_cb (brinst_cb_t  cb) { 
      qemu_set_brinst_cb(cb); 
    }

    virtual void set_mem_cb   (mem_cb_t   cb) { 
      qemu_set_mem_cb   (cb);
    }
    virtual void set_int_cb   (int_cb_t   cb) { 
      qemu_set_int_cb   (cb);
    }

    virtual void set_magic_cb (magic_cb_t cb) { 
      qemu_set_magic_cb (cb);
    }

    virtual void set_io_cb    (io_cb_t    cb) { 
      qemu_set_io_cb    (cb);
    }

    virtual void set_reg_cb(reg_cb_t cb) {
      qemu_set_reg_cb(cb);
    }

    virtual void set_trans_cb(trans_cb_t cb) {
      qemu_set_trans_cb(cb);
    }

    virtual void set_gen_cbs(bool state) {
      qemu_set_gen_cbs(state);
    }

    virtual void set_sys_cbs(bool state) {
      qemu_set_sys_cbs(state);
    }

    // Read memory at given physical address
    uint8_t mem_rd(uint64_t pa) {
      return qemu_mem_rd(pa);
    }

    void    mem_wr(uint64_t pa, uint8_t val) {
      qemu_mem_wr(pa, val);
    }

    uint8_t mem_rd_virt(int c, uint64_t va) { return qemu_mem_rd_virt(c, va); }
    void    mem_wr_virt(int c, uint64_t va, uint8_t val)
    {
      qemu_mem_wr_virt(c, va, val);
    }

    virtual int  interrupt    (uint8_t   vec)   { 
      int r;
      r = qemu_interrupt(vec);
      return r;
    }

    virtual uint64_t get_reg (int c, int r)      {
      uint64_t v; 
      v = qemu_get_reg(c, r);
      return v;
    }

    virtual void     set_reg (int c, int r, uint64_t v) {
      qemu_set_reg(c, r, v);
    }
  };


  // Coherence domain singleton-- encapsulates a set of CPUs and the needed 
  // virtual hardware (of which there is little). Limit of one per process to
  // simplify the design of the magic instruction callback. If there's a need
  // for more OSDomains, the cpu id could be expanded to provide both a OSDomain
  // ID and a CPU index in the upper and lower 16 bits respectively.
  class OSDomain {
  public:
    // CPU modes
    enum cpu_mode { MODE_REAL, MODE_PROT, MODE_LONG };

    // CPU protection rings
    enum cpu_prot { PROT_KERN, PROT_USER };

    // Create a OSDomain with n CPUs, booting the kernel at the given path
    OSDomain(uint16_t n, std::string kernel_path, const std::string& cpu_type, qsim_mode mode = QSIM_HEADLESS, unsigned ram_mb = 1024);

    // Create a new OSDomain from a state file.
    OSDomain(const char *filename);
    OSDomain(int n_cpus, const char *filename);

    // Save a snapshot of the OSDomain state
    void save_state(std::ostream &outfile);
    void save_state(const char* filename);

    // Get the current mode, protection ring, or Linux task ID for CPU i
    int           get_tid (uint16_t i);
    enum cpu_mode get_mode(uint16_t i);
    enum cpu_prot get_prot(uint16_t i);

    std::string getCpuType(uint16_t i);
    
    // Run CPU i for n instructions, if it's ready. Otherwise, do nothing.
    // Returns the number of instructions the CPU ran for (either n or 0)
    unsigned run(uint16_t i, unsigned n);

    // Run QEMU for n instructions
    // Returns the number of instructions the CPU ran for (either n or 0)
    unsigned run(unsigned n);

    // Send console output to a given C++ ostream
    void connect_console(std::ostream& s);

    // Timer interrupt should come at 100Hz
    void timer_interrupt();

    // Other interrupts can be sent as needed.
    void interrupt(unsigned i, uint8_t vec) { cpus[i]->interrupt(vec); }

    // Return true if CPU i has been bootstrapped.
    bool runnable(unsigned i) const { return running[i]; }
    bool booted(unsigned i) const __attribute__ ((deprecated)) {
      return runnable(i);
    }

    // Return the size of RAM in MB.
    unsigned get_ram_size_mb() { return ram_size_mb; }

    // Return true if CPU i is executing in its idle loop.
    bool idle(unsigned i) const { return idlevec[i]; }
    
    // Set callbacks for specific CPU i, or for all CPUs [deprecated]
    void set_atomic_cb(uint16_t i, atomic_cb_t cb){cpus[i]->set_atomic_cb(cb);}
    void set_atomic_cb(atomic_cb_t cb);
    void set_inst_cb  (uint16_t i, inst_cb_t cb)  {cpus[i]->set_inst_cb  (cb);}
    void set_inst_cb  (inst_cb_t   cb);
    void set_brinst_cb  (uint16_t i, brinst_cb_t cb)  {cpus[i]->set_brinst_cb(cb);}
    void set_brinst_cb  (brinst_cb_t cb);
    void set_mem_cb   (uint16_t i, mem_cb_t  cb)  {cpus[i]->set_mem_cb   (cb);}
    void set_mem_cb   (mem_cb_t    cb);
    void set_int_cb   (uint16_t i, int_cb_t  cb)  {cpus[i]->set_int_cb   (cb);}
    void set_int_cb   (int_cb_t    cb);
    void set_io_cb    (uint16_t i, io_cb_t   cb)  {cpus[i]->set_io_cb    (cb);}
    void set_io_cb    (io_cb_t     cb);
    void set_reg_cb   (uint16_t i, reg_cb_t cb)   {cpus[i]->set_reg_cb   (cb);}
    void set_reg_cb   (reg_cb_t    cb);
    void set_trans_cb (uint16_t i, trans_cb_t cb) {cpus[i]->set_trans_cb (cb);}
    void set_trans_cb (trans_cb_t  cb);
    void set_gen_cbs  (uint16_t i,  bool state) {cpus[i]->set_gen_cbs (state);}
    void set_gen_cbs  (bool  state);
    void set_sys_cbs  (uint16_t i,  bool state) {cpus[i]->set_sys_cbs (state);}
    void set_sys_cbs  (bool  state);

    // Better callback support. Variadic templates would make this prettier.
    struct atomic_cb_obj_base { 
      virtual ~atomic_cb_obj_base() {} 
      virtual int operator()(int)=0;
    };

    struct magic_cb_obj_base {
      virtual ~magic_cb_obj_base() {}
      virtual int operator()(int, uint64_t)=0;
    };
 
    struct io_cb_obj_base {
      virtual ~io_cb_obj_base() {}
      virtual void operator()(int, uint64_t, uint8_t, int, uint32_t)=0;
    };

    struct mem_cb_obj_base {
      virtual ~mem_cb_obj_base() {}
      virtual void operator()(int, uint64_t, uint64_t, uint8_t, int)=0;
    };
 
    struct int_cb_obj_base {
      virtual ~int_cb_obj_base() {}
      virtual int operator()(int, uint8_t)=0;
    };

    struct inst_cb_obj_base {
      virtual ~inst_cb_obj_base() {}
      virtual 
      void operator()(int, uint64_t, uint64_t, uint8_t, const uint8_t*, 
                      enum inst_type)=0;
    };

    struct brinst_cb_obj_base {
      virtual ~brinst_cb_obj_base() {}
      virtual 
      void operator()(int, uint64_t, uint64_t, uint8_t, const uint8_t*, 
                      enum inst_type)=0;
    };

    struct reg_cb_obj_base {
      virtual ~reg_cb_obj_base() {}
      virtual void operator()(int, int, uint8_t, int)=0;
    };

    struct start_cb_obj_base {
      virtual ~start_cb_obj_base() {}
      virtual int operator()(int)=0;
    };

    struct end_cb_obj_base {
      virtual ~end_cb_obj_base() {}
      virtual int operator()(int)=0;
    };

    struct trans_cb_obj_base {
      virtual ~trans_cb_obj_base() {}
      virtual void operator()(int)=0;
    };

    template <typename T> struct atomic_cb_obj : public atomic_cb_obj_base {
      typedef int (T::*atomic_cb_t)(int);
      T* p; atomic_cb_t f;
      atomic_cb_obj(T* p, atomic_cb_t f) : p(p), f(f) {}
      int operator()(int cpu_id) { return ((p)->*(f))(cpu_id); }
    };

    template <typename T> struct magic_cb_obj : public magic_cb_obj_base {
      typedef int (T::*magic_cb_t)(int, uint64_t);
      T* p; magic_cb_t f;
      magic_cb_obj(T* p, magic_cb_t f) : p(p), f(f) {}
      int operator()(int cpu_id, uint64_t rax) {
        return ((p)->*(f))(cpu_id, rax);
      }
    };

    template <typename T> struct io_cb_obj : public io_cb_obj_base {
      typedef uint32_t *(T::*io_cb_t)(int, uint64_t, uint8_t, int, uint32_t);
      T* p; io_cb_t f;
      io_cb_obj(T* p, io_cb_t f) : p(p), f(f) {}
      void operator()
        (int cpu_id, uint64_t port, uint8_t size, int type, uint32_t val)
      {
        ((p)->*(f))(cpu_id, port, size, type, val);
      }
    };

    template <typename T> struct mem_cb_obj : public mem_cb_obj_base {
      typedef void (T::*mem_cb_t)(int, uint64_t, uint64_t, uint8_t, int);
      T* p; mem_cb_t f;
      mem_cb_obj(T* p, mem_cb_t f) : p(p), f(f) {}
      void operator()(int cpu_id, uint64_t va, uint64_t pa, uint8_t s, int t) {
        ((p)->*(f))(cpu_id, va, pa, s, t);
      }
    };

    template <typename T> struct int_cb_obj : public int_cb_obj_base {
      typedef int (T::*int_cb_t)(int, uint8_t);
      T* p; int_cb_t f;
      int_cb_obj(T* p, int_cb_t f) : p(p), f(f) {}
      int operator()(int cpu_id, uint8_t vec) { 
        return ((p)->*(f))(cpu_id, vec); 
      }
    };

    template <typename T> struct inst_cb_obj : public inst_cb_obj_base {
      typedef void(T::*inst_cb_t)(int, uint64_t, uint64_t,
                                  uint8_t, const uint8_t*, enum inst_type);
      T* p; inst_cb_t f;
      inst_cb_obj(T* p, inst_cb_t f) : p(p), f(f) {}
      void operator()(int cpu_id,
                      uint64_t va,
                      uint64_t pa,
                      uint8_t l,
                      const uint8_t* b,
                      enum inst_type t) {
        ((p)->*(f))(cpu_id, va, pa, l, b, t);
      }
    };

    template <typename T> struct brinst_cb_obj : public brinst_cb_obj_base {
      typedef void(T::*brinst_cb_t)(int, uint64_t, uint64_t,
                                  uint8_t, const uint8_t*, enum inst_type);
      T* p; brinst_cb_t f;
      brinst_cb_obj(T* p, brinst_cb_t f) : p(p), f(f) {}
      void operator()(int cpu_id,
                      uint64_t va,
                      uint64_t pa,
                      uint8_t l,
                      const uint8_t* b,
                      enum inst_type t) {
        ((p)->*(f))(cpu_id, va, pa, l, b, t);
      }
    };

    template <typename T> struct reg_cb_obj : public reg_cb_obj_base {
      typedef void(T::*reg_cb_t)(int, int, uint8_t, int);
      T* p; reg_cb_t f;
      reg_cb_obj(T* p, reg_cb_t f) : p(p), f(f) {}
      void operator()(int cpu_id, int reg, uint8_t size, int type) {
        ((p)->*(f))(cpu_id, reg, size, type);
      }
    };

    template <typename T> struct start_cb_obj : public start_cb_obj_base {
      typedef int(T::*start_cb_t)(int);
      T* p; start_cb_t f;
      start_cb_obj(T* p, start_cb_t f) : p(p), f(f) {}
      int operator()(int cpu_id) {
        return ((p)->*(f))(cpu_id);
      }
    };

    template <typename T> struct end_cb_obj : public end_cb_obj_base {
      typedef int(T::*end_cb_t)(int);
      T* p; end_cb_t f;
      end_cb_obj(T* p, end_cb_t f) : p(p), f(f) {}
      int operator()(int cpu_id) {
        return ((p)->*(f))(cpu_id);
      }
    };

    template <typename T> struct trans_cb_obj : public trans_cb_obj_base {
      typedef void(T::*trans_cb_t)(int);
      T* p; trans_cb_t f;
      trans_cb_obj(T* p, trans_cb_t f): p(p), f(f) {}
      void operator()(int cpu_id) {
        ((p)->*(f))(cpu_id);
      }
    };

    struct start_cb_obj_s : public start_cb_obj_base {
      typedef int(*start_cb_t)(int);
      start_cb_t f;
      start_cb_obj_s(start_cb_t f): f(f) {}
      int operator()(int cpu_id) {
        return f(cpu_id);
      }
    };

    struct end_cb_obj_s : public end_cb_obj_base {
      typedef int(*end_cb_t)(int);
      end_cb_t f;
      end_cb_obj_s(end_cb_t f): f(f) {}
      int operator()(int cpu_id) {
        return f(cpu_id);
      }
    };

    std::vector<atomic_cb_obj_base*> atomic_cbs;
    std::vector<magic_cb_obj_base*>  magic_cbs;
    std::vector<io_cb_obj_base*>     io_cbs;
    std::vector<mem_cb_obj_base*>    mem_cbs;
    std::vector<int_cb_obj_base*>    int_cbs;
    std::vector<brinst_cb_obj_base*> brinst_cbs;
    std::vector<inst_cb_obj_base*>   inst_cbs;
    std::vector<reg_cb_obj_base*>    reg_cbs;
    std::vector<start_cb_obj_base*>  start_cbs;
    std::vector<end_cb_obj_base*>    end_cbs;
    std::vector<trans_cb_obj_base*>  trans_cbs;

    typedef std::vector<atomic_cb_obj_base*>::iterator atomic_cb_handle_t;
    typedef std::vector<magic_cb_obj_base*>::iterator  magic_cb_handle_t;
    typedef std::vector<io_cb_obj_base*>::iterator     io_cb_handle_t;
    typedef std::vector<mem_cb_obj_base*>::iterator    mem_cb_handle_t;
    typedef std::vector<int_cb_obj_base*>::iterator    int_cb_handle_t;
    typedef std::vector<inst_cb_obj_base*>::iterator   inst_cb_handle_t;
    typedef std::vector<brinst_cb_obj_base*>::iterator brinst_cb_handle_t;
    typedef std::vector<reg_cb_obj_base*>::iterator    reg_cb_handle_t;
    typedef std::vector<start_cb_obj_base*>::iterator  start_cb_handle_t;
    typedef std::vector<end_cb_obj_base*>::iterator    end_cb_handle_t;
    typedef std::vector<trans_cb_obj_base*>::iterator  trans_cb_handle_t;

    template <typename T>
      atomic_cb_handle_t
      set_atomic_cb(T* p, typename atomic_cb_obj<T>::atomic_cb_t f)
    {
      atomic_cbs.push_back(new atomic_cb_obj<T>(p, f));
      set_atomic_cb(atomic_cb_s);
      return atomic_cbs.end() - 1;
    }

    template <typename T>
      magic_cb_handle_t
        set_magic_cb(T* p, typename magic_cb_obj<T>::magic_cb_t f) 
    {
      magic_cbs.push_back(new magic_cb_obj<T>(p, f));
      return magic_cbs.end() - 1;
    }

    template <typename T>
      io_cb_handle_t set_io_cb(T* p, typename io_cb_obj<T>::io_cb_t f)
    {
      io_cbs.push_back(new io_cb_obj<T>(p, f));
      set_io_cb(io_cb_s);
      return io_cbs.end() - 1;
    }

    template <typename T>
      mem_cb_handle_t set_mem_cb(T* p, typename mem_cb_obj<T>::mem_cb_t f)
    {
      mem_cbs.push_back(new mem_cb_obj<T>(p, f));
      set_mem_cb(mem_cb_s);
      return mem_cbs.end() - 1;
    }

    template <typename T>
      int_cb_handle_t set_int_cb(T* p, typename int_cb_obj<T>::int_cb_t f)
    {
      int_cbs.push_back(new int_cb_obj<T>(p, f));
      set_int_cb(int_cb_s);
      return int_cbs.end() - 1;
    }

    template <typename T>
      inst_cb_handle_t set_inst_cb(T* p, typename inst_cb_obj<T>::inst_cb_t f)
    {
      inst_cbs.push_back(new inst_cb_obj<T>(p, f));
      set_inst_cb(inst_cb_s);
      return inst_cbs.end() - 1;
    }

    template <typename T>
      brinst_cb_handle_t set_brinst_cb(T* p, typename brinst_cb_obj<T>::brinst_cb_t f)
    {
      brinst_cbs.push_back(new brinst_cb_obj<T>(p, f));
      set_brinst_cb(brinst_cb_s);
      return brinst_cbs.end() - 1;
    }

    template <typename T>
      reg_cb_handle_t set_reg_cb(T* p, typename reg_cb_obj<T>::reg_cb_t f)
    {
      reg_cbs.push_back(new reg_cb_obj<T>(p, f));
      set_reg_cb(reg_cb_s);
      return reg_cbs.end() - 1;
    }

    template <typename T>
      start_cb_handle_t
        set_app_start_cb(T* p, typename start_cb_obj<T>::start_cb_t f)
    {
      start_cbs.push_back(new start_cb_obj<T>(p, f));
      return start_cbs.end() - 1;
    }

    template <typename T>
      end_cb_handle_t set_app_end_cb(T* p, typename end_cb_obj<T>::end_cb_t f)
    {
      end_cbs.push_back(new end_cb_obj<T>(p, f));
      return end_cbs.end() - 1;
    }

    template <typename T>
      trans_cb_handle_t
        set_trans_cb(T* p, typename trans_cb_obj<T>::trans_cb_t f)
    {
      trans_cbs.push_back(new trans_cb_obj<T>(p, f));
      set_trans_cb(trans_cb_s);
      return trans_cbs.end() - 1;
    }

    void unset_atomic_cb(atomic_cb_handle_t);
    void unset_magic_cb(magic_cb_handle_t);
    void unset_io_cb(io_cb_handle_t);
    void unset_mem_cb(mem_cb_handle_t);
    void unset_inst_cb(inst_cb_handle_t);
    void unset_brinst_cb(brinst_cb_handle_t);
    void unset_reg_cb(reg_cb_handle_t);
    void unset_app_start_cb(start_cb_handle_t);
    void unset_app_end_cb(end_cb_handle_t);
    void unset_trans_cb(trans_cb_handle_t);

    // Set the "application start" and "application end" callbacks.
    void set_app_start_cb(int f(int))
    {
      start_cbs.push_back(new start_cb_obj_s(f));
    }

    void set_app_end_cb  (int f(int))
    {
      end_cbs.push_back(new end_cb_obj_s(f));
    }

    // Get the number of CPUs
    int get_n() const { return n_cpus; }
    // Set the number of CPUs
    void set_n(int num) { n_cpus = num; }

    // Retreive/set register contents.
    uint64_t get_reg(int c, int r) { return cpus[0]->get_reg(c, r); }
    void     set_reg(int c, int r, uint64_t v) { cpus[0]->set_reg(c, r, v); }

    // Get/set memory contents (physical address)
    template <typename T> void mem_rd(T& d, uint64_t paddr) {
      size_t sz = sizeof(T);
      paddr += sz - 1;
      d = 0;
      while (sz--) {
        d <<= 8;
        d |= cpus[0]->mem_rd(paddr--);
      }
    }

    template <typename T> void mem_wr(T d, uint64_t paddr) {
      size_t sz = sizeof(T);
      while (sz--) {
        cpus[0]->mem_wr(paddr++, (d)&0xff);
        d >>= 8;
      }
    }

    // Get/set memory contents (virtual address)
    template <typename T> void mem_rd_virt(unsigned cpu, T& d, uint64_t vaddr)
    {
      size_t sz = sizeof(T);
      vaddr += sz - 1;
      d = 0;
      while (sz--) {
        d <<= 8;
        d |= cpus[0]->mem_rd_virt(cpu, vaddr--);
      }
    }

    template <typename T> void mem_wr_virt(unsigned cpu, T d, uint64_t vaddr)
    {
      size_t sz = sizeof(T);
      while (sz--) {
        cpus[0]->mem_wr_virt(cpu, vaddr++, d&0xff);
        d >>= 8;
      }
    }

    size_t   mem_sz()  { return ram_size_mb; }

    void lock_addr(uint64_t pa);
    void unlock_addr(uint64_t pa);

    void qsim_qemu_mode(qsim_mode _mode) { mode = _mode; }

    int get_bench_pid(void) { return bench_pid; }
    void set_bench_pid(int pid) { bench_pid = pid; }

    ~OSDomain();

  private:
    int id;
    int bench_pid;
    void assign_id();

    void init(const char* filename);

    std::string linebuf;
    uint16_t              n_cpus ;       // Number of CPUs
    std::vector<QemuCpu*> cpus   ;       // Vector of CPU objects
    std::vector<bool>     idlevec;       // Whether CPU is in idle loop.
    std::vector<uint16_t> tids   ;       // Current tid of each CPU
    std::vector<bool>     running;       // Whether CPU is running.

    int (*app_start_cb)(int);  // Call this when the app starts running
    int (*app_end_cb  )(int);  // Call this when the app finishes

    std::vector<std::ostream *>       consoles;
   
    unsigned ram_size_mb;
    
    static int magic_cb_s(int cpu_id, uint64_t rax);
    int waiting_for_eip;
    int  magic_cb(int cpu_id, uint64_t rax);
    static int atomic_cb_s(int cpu_id);
    int  atomic_cb(int cpu_id);

    static void inst_cb_s(int cpu_id, uint64_t va, uint64_t pa, 
                          uint8_t l, const uint8_t *bytes,
                          enum inst_type type);
    static void brinst_cb_s(int cpu_id, uint64_t va, uint64_t pa, 
                          uint8_t l, const uint8_t *bytes,
                          enum inst_type type);
    void inst_cb(int cpu_id, uint64_t va, uint64_t pa,
                 uint8_t l, const uint8_t *bytes, enum inst_type type);
    void brinst_cb(int cpu_id, uint64_t va, uint64_t pa,
                 uint8_t l, const uint8_t *bytes, enum inst_type type);
    static void mem_cb_s(int cpu_id, uint64_t va, uint64_t pa,
                        uint8_t size, int type);
    void mem_cb(int cpu_id, uint64_t va, uint64_t pa,
               uint8_t size, int type);
    static uint32_t *io_cb_s(int cpu_id, uint64_t port, uint8_t s, int type,
                             uint32_t data);
    uint32_t *io_cb(int cpu_id, uint64_t port, uint8_t s, int type,
                    uint32_t data);
    static int int_cb_s(int cpu_id, uint8_t vec);
    int int_cb(int cpu_id, uint8_t vec);
    static void reg_cb_s(int cpu_id, int reg, uint8_t size, int type);
    void reg_cb(int cpu_id, int reg, uint8_t size, int type);
    static void trans_cb_s(int cpu_id);
    void trans_cb(int cpu_id);

    static std::vector<OSDomain *> osdomains;

    qsim_mode mode;

    // save the current VM arguments
    const char **cmd_argv;
  };

  // These can be attached on a per-CPU basis to store info about the
  // instruction stream.
  class Queue : public std::queue<QueueItem> {
  public:
    Queue(OSDomain &cd, int cpu, bool make_hlt_timer_interrupt = true);
    ~Queue();
    void set_filt(bool user, bool krnl, bool prot, bool real, int tid = -1);

  private:
    OSDomain *cd     ;
    int      cpu     ;
    bool     hlt     ;

    int      flt_tid ;
    bool     flt_krnl;
    bool     flt_user;
    bool     flt_prot;
    bool     flt_real;

    static std::vector<Queue*> *queues;

    // The callbacks; static. Use queues[cpuid] to find the appropriate queue.
    static void inst_cb_flt(int, uint64_t, uint64_t, uint8_t, const uint8_t *,
                            enum inst_type);
    static void inst_cb_hlt(int, uint64_t, uint64_t, uint8_t, const uint8_t *,
                            enum inst_type);
    static void inst_cb    (int, uint64_t, uint64_t, uint8_t, const uint8_t *,
                            enum inst_type);


    static void brinst_cb_flt(int, uint64_t, uint64_t, uint8_t, const uint8_t *,
                            enum inst_type);
    static void brinst_cb_hlt(int, uint64_t, uint64_t, uint8_t, const uint8_t *,
                            enum inst_type);
    static void brinst_cb    (int, uint64_t, uint64_t, uint8_t, const uint8_t *,
                            enum inst_type);

    static void mem_cb     (int, uint64_t, uint64_t, uint8_t, int            );
    static void mem_cb_flt (int, uint64_t, uint64_t, uint8_t, int            );

    static int  int_cb     (int, uint8_t                                     );
    static int  int_cb_flt (int, uint8_t                                     );
  };

};

#endif
