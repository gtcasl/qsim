#include <capstone.h>

struct platform {
	cs_arch arch;
	cs_mode mode;
};

class cs_disas {

public:
    cs_disas(cs_arch arch, cs_mode mode);
    ~cs_disas();

    int decode(unsigned char *code, int size, cs_insn *&insn); 
    void free_insn(cs_insn *insn, int count);
private:
    csh handle;
    struct platform pf;
};
