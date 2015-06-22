/* ****************************************************************************************** */
/* ****************************************************************************************** */ 

#define REP_MOV_MEM_SIZE_MAX 4
#define REP_MOV_MEM_SIZE_MAX_NEW MAX2(REP_MOV_MEM_SIZE_MAX, (*KNOB(KNOB_MEM_SIZE_AMP)*4))
#define MAX_SRC_NUM 9
#define MAX_DST_NUM 6

typedef struct trace_info_a64_s {
  uint8_t  m_num_read_regs;     /**< num read registers */
  uint8_t  m_num_dest_regs;     /**< num dest registers */
  uint8_t  m_src[MAX_SRC_NUM];  /**< src register id */
  uint8_t  m_dst[MAX_DST_NUM];  /**< dest register id */
  uint8_t  m_cf_type;           /**< branch type */
  bool     m_has_immediate;     /**< has immediate field */
  uint16_t m_opcode;            /**< opcode */
  bool     m_has_st;            /**< has store operation */
  bool     m_is_fp;             /**< fp operation */
  bool     m_write_flg;         /**< write flag */
  uint8_t  m_num_ld;            /**< number of load operations */
  uint8_t  m_size;              /**< instruction size */
  // dynamic information
  uint64_t m_ld_vaddr1;         /**< load address 1 */
  uint64_t m_ld_vaddr2;         /**< load address 2 */
  uint64_t m_st_vaddr;          /**< store address */
  uint64_t m_instruction_addr;  /**< pc address */
  uint64_t m_branch_target;     /**< branch target address */
  uint8_t  m_mem_read_size;     /**< memory read size */
  uint8_t  m_mem_write_size;    /**< memory write size */
  bool     m_rep_dir;           /**< repetition direction */
  bool     m_actually_taken;    /**< branch actually taken */
} trace_info_a64_s;
