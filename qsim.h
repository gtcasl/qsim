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
#include <pthread.h>

#include "qsim-vm.h"
#include "mgzd.h"

namespace Qsim {
  class QemuCpu {
  private:
    // Local copy of ID number                                                 
    int cpu_id;

    // The qemu library object                                                 
    Mgzd::lib_t qemu_lib;

    // Mutexes.
    pthread_mutex_t irq_mutex;
    pthread_mutex_t cb_mutex;

    // Function pointers into the qemu library                                 
    void     (*qemu_init)(qemu_ramdesc_t *ram,
			  const char* ram_size, 
			  int cpu_id);
    uint64_t (*qemu_run)(uint64_t n);
    int      (*qemu_interrupt)(uint8_t vec);

    void (*qemu_set_atomic_cb)(atomic_cb_t);
    void (*qemu_set_inst_cb)  (inst_cb_t  );
    void (*qemu_set_int_cb)   (int_cb_t   );
    void (*qemu_set_mem_cb)   (mem_cb_t   );
    void (*qemu_set_magic_cb) (magic_cb_t );
    void (*qemu_set_io_cb)    (io_cb_t    );
    void (*qemu_set_reg_cb)   (reg_cb_t   );
    void (*qemu_set_trans_cb) (trans_cb_t );

    uint64_t (*qemu_get_reg) (enum regs r               );
    void     (*qemu_set_reg) (enum regs r, uint64_t val );

    uint8_t  (*qemu_mem_rd)  (uint64_t paddr);
    void     (*qemu_mem_wr)  (uint64_t paddr, uint8_t data);
    uint8_t  (*qemu_mem_rd_virt) (uint64_t vaddr);
    void     (*qemu_mem_wr_virt) (uint64_t vaddr, uint8_t data);

    void load_and_grab_pointers(const char *libfile);

    // Structure for accessing and sharing QEMU RAM.
    qemu_ramdesc_t **ramdesc_p;
    qemu_ramdesc_t  *ramdesc;  // Load this from *ramdesc_p after init.
    unsigned         ram_size_mb;

    // Load Linux from bzImage into QEMU RAM
    void load_linux(const char* bzImage);

  public:
    QemuCpu(int id, const char* kernel, unsigned ram_mb = 1024);
    QemuCpu(int id, QemuCpu *master_cpu, unsigned ram_mb = 1024);
    QemuCpu(int id, std::istream &file, unsigned ram_mb);
    QemuCpu(int id, std::istream &file, Qsim::QemuCpu* master_cpu,
            unsigned ram_mb);
    virtual ~QemuCpu();
 
    uint64_t run(unsigned n) { return qemu_run(n); }

    // Save state to file.
    void save_state(std::ostream &file);

    virtual void set_atomic_cb(atomic_cb_t cb) { 
      pthread_mutex_lock(&cb_mutex); 
      qemu_set_atomic_cb(cb); 
      pthread_mutex_unlock(&cb_mutex); 
    }

    virtual void set_inst_cb  (inst_cb_t  cb) { 
      pthread_mutex_lock(&cb_mutex); 
      qemu_set_inst_cb  (cb); 
      pthread_mutex_unlock(&cb_mutex); 
    }

    virtual void set_mem_cb   (mem_cb_t   cb) { 
      pthread_mutex_lock(&cb_mutex); 
      qemu_set_mem_cb   (cb);
      pthread_mutex_unlock(&cb_mutex); 
    }
    virtual void set_int_cb   (int_cb_t   cb) { 
      pthread_mutex_lock(&cb_mutex); 
      qemu_set_int_cb   (cb);
      pthread_mutex_unlock(&cb_mutex); 
    }

    virtual void set_magic_cb (magic_cb_t cb) { 
      pthread_mutex_lock(&cb_mutex); 
      qemu_set_magic_cb (cb);
      pthread_mutex_unlock(&cb_mutex);
    }

    virtual void set_io_cb    (io_cb_t    cb) { 
      pthread_mutex_lock(&cb_mutex); 
      qemu_set_io_cb    (cb);
      pthread_mutex_unlock(&cb_mutex);
    }

    virtual void set_reg_cb(reg_cb_t cb) {
      pthread_mutex_lock(&cb_mutex);
      qemu_set_reg_cb(cb);
      pthread_mutex_unlock(&cb_mutex);
    }

    virtual void set_trans_cb(trans_cb_t cb) {
      pthread_mutex_lock(&cb_mutex);
      qemu_set_trans_cb(cb);
      pthread_mutex_unlock(&cb_mutex);
    }

    // Read memory at given physical address
    uint8_t mem_rd(uint64_t pa) {
      return qemu_mem_rd(pa);
    }

    void    mem_wr(uint64_t pa, uint8_t val) {
      qemu_mem_wr(pa, val);
    }

    uint8_t mem_rd_virt(uint64_t va)      { return qemu_mem_rd_virt(va); }
    void    mem_wr_virt(uint64_t va, uint8_t val){ qemu_mem_wr_virt(va, val); }

    virtual int  interrupt    (uint8_t   vec)   { 
      int r;
      pthread_mutex_lock(&irq_mutex);
      r = qemu_interrupt(vec);
      pthread_mutex_unlock(&irq_mutex);
      return r;
    }

    virtual uint64_t get_reg (enum regs r)      { 
      uint64_t v; 
      v = qemu_get_reg(r);
      return v;
    }

    virtual void     set_reg (enum regs r, uint64_t v) {
      qemu_set_reg(r, v);
    }

    qemu_ramdesc_t get_ramdesc() const { return *ramdesc; }
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
    OSDomain(uint16_t n, std::string kernel_path, unsigned ram_mb = 2048);

    // Create a new OSDomain from a state file.
    OSDomain(const char *filename);

    // Save a snapshot of the OSDomain state
    void save_state(std::ostream &outfile);
    void save_state(const char* filename);

    // Get the current mode, protection ring, or Linux task ID for CPU i
    int           get_tid (uint16_t i);
    enum cpu_mode get_mode(uint16_t i);
    enum cpu_prot get_prot(uint16_t i);
    
    // Run CPU i for n instructions, if it's ready. Otherwise, do nothing.
    // Returns the number of instructions the CPU ran for (either n or 0)
    unsigned run(uint16_t i, unsigned n);

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
      virtual uint32_t *operator()(int, uint64_t, uint8_t, int, uint32_t)=0;
    };

    struct mem_cb_obj_base {
      virtual ~mem_cb_obj_base() {}
      virtual int operator()(int, uint64_t, uint64_t, uint8_t, int)=0;
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
      uint32_t *operator()
        (int cpu_id, uint64_t port, uint8_t size, int type, uint32_t val)
      {
	return ((p)->*(f))(cpu_id, port, size, type, val);
      }
    };

    template <typename T> struct io_cb_old_obj : public io_cb_obj_base {
      typedef void (T::*io_cb_t)(int, uint64_t, uint8_t, int, uint32_t);
      T* p; io_cb_t f;
      io_cb_old_obj(T* p, io_cb_t f) : p(p), f(f) {}
      uint32_t *operator()
        (int cpu_id, uint64_t port, uint8_t size, int type, uint32_t val)
      {
        ((p)->*(f))(cpu_id, port, size, type, val);
        return NULL;
      }
    };

    template <typename T> struct mem_cb_obj : public mem_cb_obj_base {
      typedef int (T::*mem_cb_t)(int, uint64_t, uint64_t, uint8_t, int);
      T* p; mem_cb_t f;
      mem_cb_obj(T* p, mem_cb_t f) : p(p), f(f) {}
      int operator()(int cpu_id, uint64_t va, uint64_t pa, uint8_t s, int t) {
	return ((p)->*(f))(cpu_id, va, pa, s, t);
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

    std::vector<atomic_cb_obj_base*> atomic_cbs;
    std::vector<magic_cb_obj_base*>  magic_cbs;
    std::vector<io_cb_obj_base*>     io_cbs;
    std::vector<mem_cb_obj_base*>    mem_cbs;
    std::vector<int_cb_obj_base*>    int_cbs;
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
      io_cb_handle_t set_io_cb(T* p, typename io_cb_old_obj<T>::io_cb_t f)
    {
      io_cbs.push_back(new io_cb_old_obj<T>(p, f));
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
    void unset_reg_cb(reg_cb_handle_t);
    void unset_app_start_cb(start_cb_handle_t);
    void unset_app_end_cb(end_cb_handle_t);
    void unset_trans_cb(trans_cb_handle_t);

    // Get the number of CPUs
    int get_n() const { return n; }

    // Get the QEMU RAM descriptor
    qemu_ramdesc_t get_ramdesc() const { return ramdesc; }

    // Retreive/set register contents.
    uint64_t get_reg(unsigned i, enum regs r) { return cpus[i]->get_reg(r); } 
    void     set_reg(unsigned i, enum regs r, uint64_t v) {
      cpus[i]->set_reg(r, v);
    }

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
	d |= cpus[cpu]->mem_rd_virt(vaddr--);
      }
    }

    template <typename T> void mem_wr_virt(unsigned cpu, T d, uint64_t vaddr)
    {
      size_t sz = sizeof(T);
      while (sz--) {
	cpus[cpu]->mem_wr_virt(vaddr++, d&0xff);
	d >>= 8;
      }
    }

    uint8_t *mem_ptr() { return ramdesc.mem_ptr; }
    size_t   mem_sz()  { return ramdesc.sz; }

    void lock_addr(uint64_t pa);
    void unlock_addr(uint64_t pa);

    ~OSDomain();

  private:
    int id;
    void assign_id();

    std::string linebuf;

    uint16_t              n      ;       // Number of CPUs
    std::vector<QemuCpu*> cpus   ;       // Vector of CPU objects
    std::vector<bool>     idlevec;       // Whether CPU is in idle loop.
    std::vector<uint16_t> tids   ;       // Current tid of each CPU
    std::vector<bool>     running;       // Whether CPU is running.

    std::vector<std::queue<uint8_t> > pending_ipis;
    std::vector<std::ostream *>       consoles;
    pthread_mutex_t pending_ipis_mutex;
   
    qemu_ramdesc_t ramdesc;
    unsigned ram_size_mb;
    
    static int magic_cb_s(int cpu_id, uint64_t rax);
    int waiting_for_eip;
    int  magic_cb(int cpu_id, uint64_t rax);
    static int atomic_cb_s(int cpu_id);
    int  atomic_cb(int cpu_id);

    static void inst_cb_s(int cpu_id, uint64_t va, uint64_t pa, 
                          uint8_t l, const uint8_t *bytes,
                          enum inst_type type);
    void inst_cb(int cpu_id, uint64_t va, uint64_t pa,
                 uint8_t l, const uint8_t *bytes, enum inst_type type);
    static int mem_cb_s(int cpu_id, uint64_t va, uint64_t pa, 
                        uint8_t size, int type);
    int mem_cb(int cpu_id, uint64_t va, uint64_t pa, 
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
    static pthread_mutex_t osdomains_lock;
  };
};

#endif
