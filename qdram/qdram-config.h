#ifndef __QDRAM_CONFIG
#define __QDRAM_CONFIG

#include <qcache.h>

namespace Qcache {
  // DramDimensions is just a convenient way to package up a bunch of constants
  // into a class that can be dropped in as a template parameter.
  template
    <int L2_CHANNELS, int L2_RANKS, int L2_BANKS,
     int L2_ROWS, int L2_COLS, int L2_LINESZ>
    struct Dimensions
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
  #define DIMENSIONS_PASSTHROUGH_FUNCS \
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
    DIMENSIONS_PASSTHROUGH_FUNCS
    Dimensions <0, 1, 3, 15, 7, 6> d;
  };

  // Realistic DIMM based on Micron and Samsung datasheets.
  struct Dim4GB1Rank {
    DIMENSIONS_PASSTHROUGH_FUNCS
    Dimensions <0, 0, 3, 16, 7, 6> d;
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

  private:
    DIM d;
  };

  template <typename DIM> class AddrMappingB {
  public:
    int getChannel(addr_t addr) {
      return getBits(addr, d.l2Linesz(), d.l2Channels());
    }

    int getRank(addr_t addr) {
      return getBits(addr, d.l2Banks()+d.l2Channels+d.l2Linesz(), d.l2Ranks());
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

  private:
    DIM d;
  };


};
#endif
