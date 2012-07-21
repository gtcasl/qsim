#ifndef __QDRAM_CONFIG
#define __QDRAM_CONFIG

#include <qcache.h>

namespace Qcache {
  // DramDimensions is just a convenient way to package up a bunch of constants
  // into a class that can be dropped in as a template parameter.
  template
    <int L2_CHANNELS, int L2_RANKS, int L2_BANKS,
     int L2_ROWS, int L2_COLS, int L2_LINESZ>
    struct DramDimensions
  {
    int l2Channels() { return L2_CHANNELS; }
    int l2Ranks()    { return L2_RANKS; }
    int l2Banks()    { return L2_BANKS; }
    int l2Rows()     { return L2_ROWS; }
    int l2Cols()     { return L2_COLS; }
    int l2Linesz()   { return L2_LINESZ; }

    unsigned channels() { return 1<<l2Channels(); }
    unsigned ranks()    { return 1<<l2Ranks(); }
    unsigned banks()    { return 1<<l2Banks(); }
    unsigned rows()     { return 1<<l2Rows(); }
    unsigned cols()     { return 1<<l2Cols(); }
    unsigned linesz()   { return 1<<l2Linesz(); }
  };

  // Another case where alias templates would be super-convenient.
  #define DRAMDIMENSIONS_PASSTHROUGH_FUNCS \
    int l2Channels() { return d.l2Channels(); } \
    int l2Ranks()    { return d.l2Ranks(); } \
    int l2Banks()    { return d.l2Banks(); } \
    int l2Rows()     { return d.l2Rows(); } \
    int l2Cols()     { return d.l2Cols(); } \
    int l2Linesz()   { return d.l2Linesz(); } \
    unsigned channels() { return d.channels(); } \
    unsigned ranks()    { return d.ranks(); } \
    unsigned banks()    { return d.banks(); } \
    unsigned rows()     { return d.rows(); } \
    unsigned cols()     { return d.cols(); }

  // Changed row number from datasheet.
  struct Dim4GB2Rank {
    DRAMDIMENSIONS_PASSTHROUGH_FUNCS
    DramDimensions <0, 1, 3, 15, 7, 6> d;
  };

  // Realistic DIMM based on Micron and Samsung datasheets.
  struct Dim4GB1Rank {
    DRAMDIMENSIONS_PASSTHROUGH_FUNCS
    DramDimensions <0, 0, 3, 16, 7, 6> d;
  };

  #undef DIMENSIONS_PASSTHROUGH_FUNCS

  // This should probably be used in more places instead of the potentially
  // less-clear idiom it uses.
  static inline addr_t getBits(addr_t a, unsigned start, unsigned len) {
    return (a>>start)&((1ul<<len)-1);
  }

  template <typename DIM> class AddrMappingA {
  public:
    int getChannel(addr_t addr) {
      return getBits(addr, d.l2Cols()+d.l2Linesz(), d.l2Channels());
    }

    int getRank(addr_t addr) {
      return getBits(addr,
                     d.l2Banks()+d.l2Channels()+d.l2Cols()+d.l2Linesz(),
                     d.l2Ranks());
    }

    int getBank(addr_t addr) {
      return getBits(addr,
                     d.l2Channels()+d.l2Cols()+d.l2Linesz(),
                     d.l2Banks());
    }

    int getRow(addr_t addr) {
      return getBits(addr,
               d.l2Ranks()+d.l2Banks()+d.l2Channels()+d.l2Cols()+d.l2Linesz(),
               d.l2Rows());
    }

    int getCol(addr_t addr) {
      return getBits(addr, d.l2Linesz(), d.l2Cols());
    }

    DIM d;
  };

  template <typename DIM> class AddrMappingB {
  public:
    int getChannel(addr_t addr) {
      return getBits(addr, d.l2Linesz(), d.l2Channels());
    }

    int getRank(addr_t addr) {
      return getBits(addr,d.l2Banks()+d.l2Channels()+d.l2Linesz(),d.l2Ranks());
    }

    int getBank(addr_t addr) {
      return getBits(addr, d.l2Channels()+d.l2Linesz(), d.l2Banks());
    }

    int getRow(addr_t addr) {
      return getBits(addr,
        d.l2Cols()+d.l2Ranks()+d.l2Banks()+d.l2Channels()+d.l2Linesz(),
        d.l2Rows());
    }

    int getCol(addr_t addr) {
      return getBits(addr, d.l2Ranks()+d.l2Banks()+d.l2Channels()+d.l2Linesz(),
                     d.l2Cols());
    }

    DIM d;
  };

  // Timing parameters for 1.067GHz DIMMs
  struct DramTiming1067 {
    int tCPD()     { return    1; }  int tFAW()     { return  27; }
    int tCL()      { return   14; }  int tCWL()     { return  10; }
    int tCCD()     { return    4; }  int tRCD()     { return  14; }
    int tRP()      { return   14; }  int tRAS()     { return  36; }
    int tRRD()     { return    6; }  int tRTP()     { return   8; }
    int tWR()      { return   16; }  int tRFC()     { return 118; }
    int tWTR()     { return    8; }  int tRTW()     { return  16; }
    int tREF()     { return 8319; }  int tPD()      { return   6; }
    int tXP()      { return    7; }  int tXPDLL()   { return  26; }
    int tACTPDEN() { return    2; }  int tPRPDEN()  { return   2; }
    int tREFPDEN() { return    2; }  int tRDPDEN()  { return  19; }
    int tWRPDEN()  { return   34; }  int tWRAPDEN() { return  35; }

    // The longest timing that isn't tREF.
    int tMAX() { return tRFC(); }
  };  
};
#endif
