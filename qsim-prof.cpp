/*****************************************************************************\
* Qemu Simulation Framework (qsim)                                            *
* Qsim is a modified version of the Qemu emulator (www.qemu.org), coupled     *
* a C++ API, for the use of computer architecture researchers.                *
*                                                                             *
* This work is licensed under the terms of the GNU GPL, version 2. See the    *
* COPYING file in the top-level directory.                                    *
\*****************************************************************************/
#include <qsim-prof.h>

#include <fstream>
#include <vector>
#include <set>
#include <iomanip>

#include <cstdlib>

#include <pthread.h>

using namespace Qsim;
using namespace std;

class QsimProf {
public:
  QsimProf(OSDomain &osd, const char *filename, unsigned w, unsigned s):
    tr(filename), osd(osd), w(w), s(s), t(osd.get_n())
  {
    pthread_mutex_init(&trLock, NULL);
    icbH = osd.set_inst_cb(this, &QsimProf::inst_cb);
  }

  void inst_cb(int c, uint64_t pa, uint64_t va, uint8_t len, const uint8_t *b,
               enum inst_type type)
  {
    if (++t[c].windowCount == w) t[c].windowCount = 0;
    if (t[c].windowCount == 0) {
      t[c].samp.clear();
      for (unsigned i = 0; i < s; ++i) t[c].samp.insert(rand()%w);
    }

    if (t[c].samp.find(t[c].windowCount) != t[c].samp.end()) {
      pthread_mutex_lock(&trLock);
      tr << std::dec << c << ", " << va << ", " << osd.get_prot(c) << ", ";
      for (unsigned i = 0; i < len; ++i)
        tr << std::hex << setw(2) << setfill('0') << (unsigned)b[i] << ' ';
      tr << '\n';
      pthread_mutex_unlock(&trLock);
    }
  }
  
private:
  struct Thread {
    Thread(): samp(), windowCount(0) {}
    set<unsigned> samp;
    unsigned windowCount;
    unsigned char padding[64];
  };

  OSDomain::inst_cb_handle_t icbH;

  pthread_mutex_t trLock;
  ofstream tr;

  OSDomain &osd;
  unsigned w, s;

  vector<Thread> t;
};

static QsimProf *prof(NULL);

void Qsim::start_prof(OSDomain &osd, const char *tracefile,
                      unsigned window, unsigned samplesPerWindow)
{
  prof = new QsimProf(osd, tracefile, window, samplesPerWindow);
}

void Qsim::end_prof(OSDomain &osd) {
  if (prof) delete(prof);
}

