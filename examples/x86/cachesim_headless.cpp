/*****************************************************************************\
* Qemu Simulation Framework (qsim)                                            *
* Qsim is a modified version of the Qemu emulator (www.qemu.org), couled     *
* a C++ API, for the use of computer architecture researchers.                *
*                                                                             *
* This work is licensed under the terms of the GNU GPL, version 2. See the    *
* COPYING file in the top-level directory.                                    *
\*****************************************************************************/
#include <iostream>
#include <fstream>
#include <iomanip>
#include <string>

#include <qsim.h>

using Qsim::OSDomain;

using std::ostream;

static const size_t cacheLineSizeLog2 = 6;
static const size_t cacheLineSize     = 1<<cacheLineSizeLog2;

#define KB(x) ((x) << 10)
#define MB(x) ((x) << 20)

class CacheHitCounter {

    static const size_t  depthLog2 = 4;
    static const size_t  depth     = 1<<depthLog2;
    size_t  widthLog2;
    size_t  width;
    size_t  widthMask;
    size_t  hits;
    size_t  misses;
    size_t  addressesLen;
    size_t* addresses;
    size_t  maxSize;

    CacheHitCounter & operator =(CacheHitCounter const & CacheHitProfile1);
    CacheHitCounter(CacheHitCounter const &);

    public:
    CacheHitCounter() {}
    CacheHitCounter(size_t maxSizeLog2) {
        maxSize         = size_t(1)<<maxSizeLog2;
        widthLog2       = maxSizeLog2 - cacheLineSizeLog2 - depthLog2;
        width           = size_t(1)<<widthLog2;
        widthMask       = width-1;
        addresses		= new size_t[addressesLen];

        clear();
    }

    void initialize(size_t size) {
        maxSize         = size;
        width           = size / ((1<<depthLog2) * cacheLineSize);
        widthMask       = width-1;
        addressesLen    = depth*width;

        addresses		= new size_t[addressesLen];

        clear();
    }

    void clear() {
        hits   = 0;
        misses = 0;
        for (size_t i = 0; i < addressesLen; i++) addresses[i] = 0;
    }

    void clearAddresses() 
    {
        memset(addresses, 0, addressesLen * sizeof(size_t));
        //for (size_t i = 0; i < addressesLen; i++) addresses[i] = 0;
    }

    ~CacheHitCounter() {
        delete [] addresses;
    }

    bool insert(size_t cacheLine, size_t hashedCacheLine) {

        size_t col = hashedCacheLine % width; 
        size_t* c  = &addresses[col*depth];
        size_t  pc = cacheLine;
        size_t  r  = 0;
        for (; r < depth; r++) {
            size_t oldC = c[r];
            c[r] = pc;
            if (oldC == cacheLine) {
                hits++;
                return true;
            }
            pc = oldC;
        }
        misses++;
        return false;
    };

    size_t getHits() {
        return hits;
    }

    double getHitRatio() {
        size_t total = hits + misses;

        return (double)hits / total;
    }

    double getMissRatio() {
        size_t total = hits + misses;

        return (double)misses / total;
    }

    size_t getTotalAccesses() { return hits + misses; }

    size_t getCacheSize()     { return maxSize / MB(1); }

    void PrintConfig() {
        printf("CacheSize %lu, width %lu, addressesLen %lu\n", maxSize / MB(1), width, addressesLen);
    }
};

class TraceWriter {
    public:
        TraceWriter(OSDomain &osd) : 
            osd(osd), ran(false), finished(false)
        { 
            l1cache = new CacheHitCounter[osd.get_n()];
            for (int i = 0; i < osd.get_n(); i++)
                l1cache[i].initialize(KB(32));
            osd.set_app_start_cb(this, &TraceWriter::app_start_cb); 
            l2cache.initialize(MB(8));
            total_instructions = 1;
        }

        bool hasFinished() { return finished; }

        int app_start_cb(int c) {
            static bool ran = false;
            total_instructions = 1;
            if (!ran) {
                ran = true;
                osd.set_inst_cb(this, &TraceWriter::inst_cb);
                osd.set_mem_cb(this, &TraceWriter::mem_cb);
                osd.set_app_end_cb(this, &TraceWriter::app_end_cb);

                return 1;
            }

            return 0;
        }

        void inst_cb(int c, uint64_t v, uint64_t p, uint8_t l, const uint8_t *b, 
                enum inst_type t)
        {
            total_instructions++;
        }

        void mem_cb(int c, uint64_t v, uint64_t p, uint8_t s, int w) {
            uint64_t hashed_addr = v ^ (v >> 13);
            if (!l1cache[c].insert(v, hashed_addr))
                l2cache.insert(v, hashed_addr);
        }

        void print_stats()
        {
            if (!ran) return;
            std::cout << "L1 Hit%: (";
            for (int i = 0; i < osd.get_n(); i++)
                std::cout << " " << std::fixed << std::setw(5) <<
                    std::setprecision(2) << l1cache[i].getHits() * 100.0 /
                    l1cache[i].getTotalAccesses();

            std::cout << "), L2 Hit%: " << l2cache.getHits() * 100.0 /
                            l2cache.getTotalAccesses() << ", L2 MPKI : " <<
                            (l2cache.getTotalAccesses() - l2cache.getHits()) * 1000.0 
                                                   / total_instructions << std::endl;
        }

        int app_end_cb(int c)
        {
            finished = true;
            print_stats();
            return 0;
        }

        double get_hit_ratio() { return l2cache.getHitRatio(); }

    private:
        OSDomain &osd;
        bool ran;
        bool finished;

        static const char * itype_str[];
        CacheHitCounter *l1cache, l2cache;
        uint64_t total_instructions;
};

const char *TraceWriter::itype_str[] = {
    "QSIM_INST_NULL",
    "QSIM_INST_INTBASIC",
    "QSIM_INST_INTMUL",
    "QSIM_INST_INTDIV",
    "QSIM_INST_STACK",
    "QSIM_INST_BR",
    "QSIM_INST_CALL",
    "QSIM_INST_RET",
    "QSIM_INST_TRAP",
    "QSIM_INST_FPBASIC",
    "QSIM_INST_FPMUL",
    "QSIM_INST_FPDIV"
};

int main(int argc, char** argv) {
    using std::istringstream;
    using std::ofstream;

    if (!getenv("QSIM_PREFIX")) {
        fprintf(stderr, "QSIM_PREFIX env variable not set! Exiting...\n");
        exit(1);
    }

    std::string qsim_prefix(getenv("QSIM_PREFIX"));
    ofstream *outfile(NULL);

    unsigned n_cpus = 1;

    // Read number of CPUs as a parameter. 
    if (argc >= 2) {
        istringstream s(argv[1]);
        s >> n_cpus;
    }

    OSDomain *osd_p(NULL);

    if (argc >= 4) {
        // Create new OSDomain from saved state.
        osd_p = new OSDomain(argv[3]);
        n_cpus = osd_p->get_n();
    } else {
        osd_p = new OSDomain(n_cpus, qsim_prefix + "/linux/bzImage", "x86", QSIM_HEADLESS);
    }
    OSDomain &osd(*osd_p);

    // Attach a TraceWriter if a trace file is given.
    TraceWriter tw(osd);

    // If this OSDomain was created from a saved state, the app start callback was
    // received prior to the state being saved.
    //if (argc >= 4) tw.app_start_cb(0);

    osd.connect_console(std::cout);

    // The main loop: run until 'finished' is true.
    std::cout << "Starting execution..." << std::endl;
    while (!tw.hasFinished()) {
      for (unsigned i = 0; i < 100; i++) {
          osd.run(0, 100000);
      }
      osd.timer_interrupt();
      tw.print_stats();
    }

    if (outfile) { outfile->close(); }
    delete outfile;

    delete osd_p;

    return 0;
}
