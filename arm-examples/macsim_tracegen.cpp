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
#include <thread>

#include <qsim.h>
#include <stdio.h>
#include <capstone.h>

#include "gzstream.h"
#include "cs_disas.h"
#include "macsim_tracegen.h"

using Qsim::OSDomain;

using std::ostream;

class InstHandler {
public:
    InstHandler() {}
    InstHandler(ogzstream *outfile);
    void setOutFile(ogzstream *outfile);
    bool populateInst(cs_insn *insn);
private:
    trace_info_cpu_s op; 
    ogzstream *outfile;
};

InstHandler::InstHandler(ogzstream *out)
{
    outfile = out;
}

void InstHandler::setOutFile(ogzstream *out)
{
    outfile = out;
}

static uint8_t get_xed_opcode(unsigned int inst)
{
    uint8_t ret;
    switch (inst) {
        case 0: 
            ret = XED_CATEGORY_INVALID;
		
            ret = XED_CATEGORY_3DNOW;
		case ARM64_INS_AESD:
		case ARM64_INS_AESE:
		case ARM64_INS_AESIMC:
		case ARM64_INS_AESMC:
            ret = XED_CATEGORY_AES;
		
            ret = XED_CATEGORY_AVX;
		
            ret = XED_CATEGORY_AVX2; // new
		
            ret = XED_CATEGORY_AVX2GATHER; // new
		
            ret = XED_CATEGORY_BDW; // new
		
            ret = XED_CATEGORY_BINARY;
		
            ret = XED_CATEGORY_BITBYTE;
		
            ret = XED_CATEGORY_BMI1; // new
		
            ret = XED_CATEGORY_BMI2; // new
		
            ret = XED_CATEGORY_BROADCAST;
		
            ret = XED_CATEGORY_CALL;
		
            ret = XED_CATEGORY_CMOV;
            ret = XED_CATEGORY_COND_BR;
		
            ret = XED_CATEGORY_CONVERT;
		
            ret = XED_CATEGORY_DATAXFER;
		
            ret = XED_CATEGORY_DECIMAL;
		
            ret = XED_CATEGORY_FCMOV;
		
            ret = XED_CATEGORY_FLAGOP;
		
            ret = XED_CATEGORY_FMA4; // new
		
            ret = XED_CATEGORY_INTERRUPT;
		
            ret = XED_CATEGORY_IO;
		
            ret = XED_CATEGORY_IOSTRINGOP;
		case ARM64_INS_ABS:
        case ARM64_INS_ADC:
        case ARM64_INS_ADDHN:
        case ARM64_INS_ADDHN2:
        case ARM64_INS_ADDP:
        case ARM64_INS_ADD:
        case ARM64_INS_ADDV:
        case ARM64_INS_ADR:
        case ARM64_INS_ADRP:
        case ARM64_INS_AND:
            ret = XED_CATEGORY_LOGICAL;
		
            ret = XED_CATEGORY_LZCNT; // new
		
            ret = XED_CATEGORY_MISC;
		
            ret = XED_CATEGORY_MMX;
		
            ret = XED_CATEGORY_NOP;
		
            ret = XED_CATEGORY_PCLMULQDQ;
		
            ret = XED_CATEGORY_POP;
		
            ret = XED_CATEGORY_PREFETCH;
		
            ret = XED_CATEGORY_PUSH;
		
            ret = XED_CATEGORY_RDRAND; // new
		
            ret = XED_CATEGORY_RDSEED; // new
		
            ret = XED_CATEGORY_RDWRFSGS; // new
		
            ret = XED_CATEGORY_RET;
		
            ret = XED_CATEGORY_ROTATE;
		
            ret = XED_CATEGORY_SEGOP;
		
            ret = XED_CATEGORY_SEMAPHORE;
        case ARM64_INS_ASR:
            ret = XED_CATEGORY_SHIFT;
		
            ret = XED_CATEGORY_SSE;
		
            ret = XED_CATEGORY_STRINGOP;
		
            ret = XED_CATEGORY_STTNI;
		
            ret = XED_CATEGORY_SYSCALL;
		
            ret = XED_CATEGORY_SYSRET;
		
            ret = XED_CATEGORY_SYSTEM;
		
            ret = XED_CATEGORY_TBM; // new
        case ARM64_INS_B:
            ret = XED_CATEGORY_UNCOND_BR;
		
            ret = XED_CATEGORY_VFMA; // new
		
            ret = XED_CATEGORY_VTX;
		
            ret = XED_CATEGORY_WIDENOP;
		
            ret = XED_CATEGORY_X87_ALU;
		
            ret = XED_CATEGORY_XOP;
		
            ret = XED_CATEGORY_XSAVE;
		
            ret = XED_CATEGORY_XSAVEOPT;
		
            ret = TR_MUL;
		
            ret = TR_DIV;
		
            ret = TR_FMUL;
		
            ret = TR_FDIV;
		
            ret = TR_NOP;
		
            ret = PREFETCH_NTA;
		
            ret = PREFETCH_T0;
		
            ret = PREFETCH_T1;
		
            ret = PREFETCH_T2;
		
        default:
            ret = XED_CATEGORY_INVALID;
    }

    return ret;
}

bool InstHandler::populateInst(cs_insn *insn)
{
    cs_arm64* arm64;

    if (insn->detail == NULL)
        return false;

    arm64 = &(insn->detail->arm64);

    op.m_num_read_regs = insn->detail->regs_read_count;
    op.m_num_dest_regs = insn->detail->regs_write_count;
    op.m_size = 4;
    op.m_instruction_addr  = insn->address;

    for (int i = 0; i < op.m_num_read_regs; i++)
        op.m_src[i] = insn->detail->regs_read[i];
    for (int i = 0; i < op.m_num_dest_regs; i++)
        op.m_dst[i] = insn->detail->regs_write[i];

    op.m_cf_type = 0;
    for (int grp_idx = 0; grp_idx < insn->detail->groups_count; grp_idx++) {
        if (insn->detail->groups[grp_idx] == ARM64_GRP_JUMP) {
            op.m_cf_type = 1;
            break;
        }
    }

    op.m_has_immediate = 0;
    for (int op_idx = 0; op_idx < arm64->op_count; op_idx++) {
        if (arm64->operands[op_idx].type == ARM64_OP_IMM ||
            arm64->operands[op_idx].type == ARM64_OP_CIMM) {
            op.m_has_immediate = 1;
            break;
        }
    }

    op.m_has_st = 0;
    if (ARM64_INS_ST1 <= insn->id && insn->id <= ARM64_INS_STXR)
        op.m_has_st = 1;

    op.m_is_fp = 0;
    for (int op_idx = 0; op_idx < arm64->op_count; op_idx++) {
        if (arm64->operands[op_idx].type == ARM64_OP_FP) {
            op.m_is_fp = 1;
            break;
        }
    }

    op.m_write_flg  = arm64->writeback;
    op.m_num_ld = 0;
    op.m_ld_vaddr2 = 0;
    op.m_st_vaddr = 0;
    op.m_branch_target = 0;
    op.m_mem_read_size = 0;
    op.m_mem_write_size = 0;
    op.m_rep_dir = 0;
    op.m_actually_taken = 0;
    op.m_opcode = get_xed_opcode(insn->id);

	for (int i = 0; i < arm64->op_count; i++) {
		cs_arm64_op *op = &(arm64->operands[i]);
    }
}

class TraceWriter {
public:
  TraceWriter(OSDomain &osd) :
    osd(osd), finished(false), dis(CS_ARCH_ARM64, CS_MODE_ARM)
  { 
    osd.set_app_start_cb(this, &TraceWriter::app_start_cb);
    trace_file_count = 0;
    finished = false;
  }

  bool hasFinished() { return finished; }

  int app_start_cb(int c) {
    static bool ran = false;
    if (!ran) {
      ran = true;
      osd.set_inst_cb(this, &TraceWriter::inst_cb);
      osd.set_mem_cb(this, &TraceWriter::mem_cb);
      osd.set_app_end_cb(this, &TraceWriter::app_end_cb);
    }
    tracefile = new ogzstream(("trace_" + std::to_string(trace_file_count) + ".log.gz").c_str());
    inst_handle.setOutFile(tracefile);
    trace_file_count++;
    finished = false;

    return 0;
  }

  int app_end_cb(int c)
  {
      std::cout << "App end cb called" << std::endl;
      finished = true;

      if (tracefile) {
          tracefile->close();
          delete tracefile;
          tracefile = NULL;
          inst_handle.setOutFile(tracefile);
      }

      return 0;
  }

  void inst_cb(int c, uint64_t v, uint64_t p, uint8_t l, const uint8_t *b, 
               enum inst_type t)
  {
      cs_insn *insn;
      int count = dis.decode((unsigned char *)b, l, insn);
      if (tracefile) {
          for (int j = 0; j < count; j++) {
              *tracefile << std::dec << "count: "  << count << 
                  ": " << std::hex << v <<
                  ": " << std::hex << insn[j].address <<
                  ": " << insn[j].mnemonic <<
                  ": " << insn[j].op_str <<
                  ": " << get_inst_string(t) <<
                  std::endl;
          }
      } else {
          std::cout << "Writing to a null tracefile" << std::endl;
      }
      dis.free_insn(insn, count);
      return;
  }

  int mem_cb(int c, uint64_t v, uint64_t p, uint8_t s, int w)
  {
      if (tracefile) {
          *tracefile << (w ? "Write: " : "Read: ")
                     << "v: 0x" << std::hex << v
                     << " p: 0x" << std::hex << p 
                     << " s: " << std::dec << (int)s
                     << " val: " << *(uint32_t *)p
                     << std::endl;
      }
  }

private:
  cs_disas dis;
  OSDomain &osd;
  ogzstream* tracefile;
  bool finished;
  int  trace_file_count;
  InstHandler inst_handle;

  static const char * itype_str[];
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

  unsigned n_cpus = 1;

  std::string qsim_prefix(getenv("QSIM_PREFIX"));

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
    osd_p = new OSDomain(n_cpus, qsim_prefix + "/../arm64_images/vmlinuz");
  }
  OSDomain &osd(*osd_p);

  // Attach a TraceWriter if a trace file is given.
  TraceWriter tw(osd);

  // If this OSDomain was created from a saved state, the app start callback was
  // received prior to the state being saved.
  //if (argc >= 4) tw.app_start_cb(0);

  osd.connect_console(std::cout);

  //tw.app_start_cb(0);
  // The main loop: run until 'finished' is true.
  uint64_t inst_per_iter = 1000000000;
  int inst_run = inst_per_iter;
  while (!(inst_per_iter - inst_run)) {
    for (unsigned long j = 0; j < n_cpus; j++) {
        inst_run = osd.run(j, inst_per_iter);
    }
    osd.timer_interrupt();
  }

  delete osd_p;

  return 0;
}
