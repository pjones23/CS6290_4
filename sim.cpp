#include "common.h"
#include "sim.h"
#include "trace.h" 
#include "cache.h"  /**** NEW-LAB2*/ 
#include "memory.h" // NEW-LAB2 
#include "bpred.h"
#include "vmem.h"
#include <stdlib.h>
#include <ctype.h> /* Library for useful character operations */
/*******************************************************************/
/* Simulator frame */
/*******************************************************************/

/****** LAB 3 ********/
uint64_t bpred_mispred_count = 0; /* total number of branch mispredictions */ // NEW-LAB3
uint64_t bpred_okpred_count = 0; /* total number of correct branch predictions */ // NEW-LAB3
uint64_t dtlb_hit_count = 0; /* total number of DTLB hits */ // NEW-LAB3
uint64_t dtlb_miss_count = 0; /* total number of DTLB miss */ // NEW-LAB3

bpred *branchpred; // NEW-LAB3 (student need to initialize)
tlb *dtlb;        // NEW-LAB3 (student need to intialize)

bool use_bpred = false;
bool vmem_enabled = false;
uint64_t vmem_page_size = 0;

bool pte_load_done = false;
Op* dummy_op = new Op;
/****** LAB 3 ********/

/*******************************************************************/
/*  LAB 4 Counters  */
/*******************************************************************/
//Overall
uint64_t numInstr;
uint64_t numSimCyles;
uint64_t numfpInstr;
uint64_t numIntInstr;
uint64_t numBranchInstr;
uint64_t numLDInstr;
uint64_t numSTInstr;
//Fetch
uint64_t numICacheAccesses;
uint64_t numBranchMisPred;
//ID
uint64_t numSchedAccesses;
uint64_t numIntSchedAccesses;
uint64_t numfpSchedAccesses;
uint64_t numIntRegReads;
uint64_t numfpRegReads;
//Execution
uint64_t numMultInstr;
//Memory
uint64_t numDCacheReads;
uint64_t numDCacheWrites;
uint64_t numMemAccesses;
//WB
uint64_t numIntRegWrites;
uint64_t numfpRegWrites;
bool is_fp(Op* op);
bool is_int(Op* op);

bool run_a_cycle(memory_c *m); // please modify run_a_cycle function argument  /** NEW-LAB2 */
void init_structures(memory_c *m); // please modify init_structures function argument  /** NEW-LAB2 */ 

/* uop_pool related variables */

uint32_t free_op_num;
uint32_t active_op_num;
Op *op_pool;
Op *op_pool_free_head = NULL;

/* simulator related local functions */

bool icache_access(ADDRINT addr);
bool dcache_access(ADDRINT addr);
void init_latches(void);

#include "knob.h"
#include "all_knobs.h"

// knob variables
KnobsContainer *g_knobsContainer; /* < knob container > */
all_knobs_c *g_knobs; /* < all knob variables > */

gzFile g_stream;

void init_knobs(int argc, char** argv) {
	// Create the knob managing class
	g_knobsContainer = new KnobsContainer();

	// Get a reference to the actual knobs for this component instance
	g_knobs = g_knobsContainer->getAllKnobs();

	// apply the supplied command line switches
	char* pInvalidArgument = NULL;
	g_knobsContainer->applyComandLineArguments(argc, argv, &pInvalidArgument);

	g_knobs->display();
}

void read_trace_file(void) {
	g_stream = gzopen((KNOB(KNOB_TRACE_FILE)->getValue()).c_str(), "r");
}

// simulator main function is called from outside of this file 

void simulator_main(int argc, char** argv) {
	init_knobs(argc, argv);

	// trace driven simulation
	read_trace_file();

	/** NEW-LAB2 *//* just note: passing main memory pointers is hack to mix up c++ objects and c-style code *//* Not recommended at all */
	memory_c *main_memory = new memory_c();  // /** NEW-LAB2 */

	init_structures(main_memory); // please modify run_a_cycle function argument  /** NEW-LAB2 */

	/*LAB 3*/
	if (KNOB(KNOB_USE_BPRED)->getValue()) {
		use_bpred = true;
		bpred_type type = (bpred_type) KNOB(KNOB_BPRED_TYPE)->getValue();
		unsigned int bhr_len;
		if (type == BPRED_TAKEN || type == BPRED_NOTTAKEN
				|| type == BPRED_BIMODAL)
			bhr_len = 0;
		else if (type == BPRED_GSHARE)
			bhr_len = KNOB(KNOB_BPRED_HIST_LEN)->getValue();
		branchpred = bpred_new(type, bhr_len);
	}
	/*LAB 3*/
	if (KNOB(KNOB_ENABLE_VMEM)->getValue()) {
		vmem_enabled = true;
		dtlb = tlb_new(KNOB(KNOB_TLB_ENTRIES)->getValue());
		vmem_page_size = KNOB(KNOB_VMEM_PAGE_SIZE)->getValue();
	}
	run_a_cycle(main_memory); // please modify run_a_cycle function argument  /** NEW-LAB2 */

}
int op_latency[NUM_OP_TYPE];

void init_op_latency(void) {
	op_latency[OP_INV] = 1;
	op_latency[OP_NOP] = 1;
	op_latency[OP_CF] = 1;
	op_latency[OP_CMOV] = 1;
	op_latency[OP_LDA] = 1;
	op_latency[OP_LD] = 1;
	op_latency[OP_ST] = 1;
	op_latency[OP_IADD] = 1;
	op_latency[OP_IMUL] = 2;
	op_latency[OP_IDIV] = 4;
	op_latency[OP_ICMP] = 2;
	op_latency[OP_LOGIC] = 1;
	op_latency[OP_SHIFT] = 2;
	op_latency[OP_BYTE] = 1;
	op_latency[OP_MM] = 2;
	op_latency[OP_FMEM] = 2;
	op_latency[OP_FCF] = 1;
	op_latency[OP_FCVT] = 4;
	op_latency[OP_FADD] = 2;
	op_latency[OP_FMUL] = 4;
	op_latency[OP_FDIV] = 16;
	op_latency[OP_FCMP] = 2;
	op_latency[OP_FBIT] = 2;
	op_latency[OP_FCMP] = 2;
}

void init_op(Op *op) {
	op->num_src = 0;
	op->src[0] = -1;
	op->src[1] = -1;
	op->dst = -1;
	op->opcode = 0;
	op->is_fp = false;
	op->cf_type = NOT_CF;
	op->mem_type = NOT_MEM;
	op->write_flag = 0;
	op->inst_size = 0;
	op->ld_vaddr = 0;
	op->st_vaddr = 0;
	op->instruction_addr = 0;
	op->branch_target = 0;
	op->actually_taken = 0;
	op->mem_read_size = 0;
	op->mem_write_size = 0;
	op->valid = FALSE;
	op->mispredicted_branch = false;
	/* you might add more features here */
}

void init_op_pool(void) {
	/* initialize op pool */
	op_pool = new Op[1024];
	free_op_num = 1024;
	active_op_num = 0;
	uint32_t op_pool_entries = 0;
	int ii;
	for (ii = 0; ii < 1023; ii++) {

		op_pool[ii].op_pool_next = &op_pool[ii + 1];
		op_pool[ii].op_pool_id = op_pool_entries++;
		init_op(&op_pool[ii]);
	}
	op_pool[ii].op_pool_next = op_pool_free_head;
	op_pool[ii].op_pool_id = op_pool_entries++;
	init_op(&op_pool[ii]);
	op_pool_free_head = &op_pool[0];
}

Op *get_free_op(void) {
	/* return a free op from op pool */

	if (op_pool_free_head == NULL || (free_op_num == 1)) {
		std::cout << "ERROR! OP_POOL SIZE is too small!! " << endl;
		std::cout << "please check free_op function " << endl;
		assert(1);
		exit(1);
	}

	free_op_num--;
	assert(free_op_num);

	Op *new_op = op_pool_free_head;
	op_pool_free_head = new_op->op_pool_next;
	assert(!new_op->valid);
	init_op(new_op);
	active_op_num++;
	return new_op;
}

void free_op(Op *op) {
	free_op_num++;
	active_op_num--;
	op->valid = FALSE;
	op->op_pool_next = op_pool_free_head;
	op_pool_free_head = op;
}

/*******************************************************************/
/*  Data structure */
/*******************************************************************/

typedef struct pipeline_latch_struct {
	Op *op; /* you must update this data structure. */
	list<Op*> op_queue;	//for MEM latch multiple instruction retirement
	bool op_valid;
	bool stage_stall;//added another variable to latch to understand if there is a stall
	/* you might add more data structures. But you should complete the above data elements */
} pipeline_latch;

typedef struct Reg_element_struct {
	bool valid;
	// data is not needed
	/* you might add more data structures. But you should complete the above data elements */
} REG_element;

REG_element register_file[NUM_REG];
void init_register_file();
bool pipeline_latches_empty();

/*******************************************************************/
/* These are the functions you'll have to write.  */
/*******************************************************************/

void FE_stage();
void ID_stage();
void EX_stage();
void MEM_stage(memory_c *main_memory); // please modify MEM_stage function argument  /** NEW-LAB2 */ 
void WB_stage();

/*******************************************************************/
/*  These are the variables you'll have to write.  */
/*******************************************************************/

bool sim_end_condition = FALSE; /* please complete the condition. */
UINT64 retired_instruction = 0; /* number of retired instruction. (only correct instructions) */
UINT64 cycle_count = 0; /* total number of cycles */
UINT64 data_hazard_count = 0;
UINT64 control_hazard_count = 0;
UINT64 icache_miss_count = 0; /* total number of icache misses. for Lab #2 and Lab #3 */
UINT64 dcache_hit_count = 0; /* total number of dcache  misses. for Lab #2 and Lab #3 */
UINT64 dcache_miss_count = 0; /* total number of dcache  misses. for Lab #2 and Lab #3 */
UINT64 l2_cache_miss_count = 0; /* total number of L2 cache  misses. for Lab #2 and Lab #3 */
UINT64 dram_row_buffer_hit_count = 0; /* total number of dram row buffer hit. for Lab #2 and Lab #3 */ // NEW-LAB2
UINT64 dram_row_buffer_miss_count = 0; /* total number of dram row buffer hit. for Lab #2 and Lab #3 */ // NEW-LAB2
UINT64 store_load_forwarding_count = 0; /* total number of store load forwarding for Lab #2 and Lab #3 */ // NEW-LAB2
UINT64 store_store_forwarding_count = 0; /* total number of store load forwarding for Lab #2 and Lab #3 */ // NEW-LAB2

pipeline_latch *MEM_latch;
pipeline_latch *EX_latch;
pipeline_latch *ID_latch;
pipeline_latch *FE_latch;
UINT64 ld_st_buffer[LD_ST_BUFFER_SIZE]; /* this structure is deprecated. do not use */
UINT64 next_pc;

Cache *data_cache = new Cache;  // NEW-LAB2

//New variables added by apkarande
UINT64 reg_writing_ops[NUM_REG];
UINT64 last_inst_id = 0;
bool trace_over = false;

/*******************************************************************/
/*  Print messages  */
/*******************************************************************/
void print_stats() {
	std::ofstream out((KNOB(KNOB_OUTPUT_FILE)->getValue()).c_str());
	/* Do not modify this function. This messages will be used for grading */
	out << "Total instruction: " << retired_instruction << endl;
	out << "Total cycles: " << cycle_count << endl;
	float ipc = (
			cycle_count ?
					((float) retired_instruction / (float) cycle_count) : 0);
	out << "IPC: " << ipc << endl;
	out << "Total I-cache miss: " << icache_miss_count << endl;
	out << "Total D-cache miss: " << dcache_miss_count << endl;
	out << "Total D-cache hit: " << dcache_hit_count << endl;
	out << "Total data hazard: " << data_hazard_count << endl;
	out << "Total control hazard : " << control_hazard_count << endl;
	out << "Total DRAM ROW BUFFER Hit: " << dram_row_buffer_hit_count << endl;
	out << "Total DRAM ROW BUFFER Miss: " << dram_row_buffer_miss_count << endl;
	out << "Total Store-load forwarding: " << store_load_forwarding_count
			<< endl;

	// new for LAB3
	out << "Total Branch Predictor Mispredictions: " << bpred_mispred_count
			<< endl;
	out << "Total Branch Predictor OK predictions: " << bpred_okpred_count
			<< endl;
	out << "Total DTLB Hit: " << dtlb_hit_count << endl;
	out << "Total DTLB Miss: " << dtlb_miss_count << endl;

	// print out McPAT counters
	out << "---------------McPAT Counters--------------" << endl;
	/*
	 Overall
	 Total number of instructions (McPAT: total_instructions, committed_instructions)
	 Total simulation cycles (McPAT: total_cycles, busy_cycles)
	 Number of floating point operations (McPAT: fp_instructions, committed_fp_instructions)
	 Number of integer operations (McPAT: int_instructions, committed_int_instructions)
	 Number of branch instructions (McPAT: branch_instructions)
	 Number of load instructions (McPAT: load_instructions)
	 Number of store instructions (McPAT: store_instructions)
	 */

	out << "Overall" << endl;
	numInstr = retired_instruction;
	out << "Total number of instructions: " << numInstr << endl;
	numSimCyles = cycle_count;
	out << "Total simulation cycles: " << numSimCyles << endl;
	out << "Number of floating point operations: " << numfpInstr << endl;
	out << "Number of integer operations: " << numIntInstr << endl;
	out << "Number of branch instructions: " << numBranchInstr << endl;
	out << "Number of load instructions: " << numLDInstr << endl;
	out << "Number of store instructions: " << numSTInstr << endl;

	/*
	 Fetch Stage
	 Number of I-cache accesses (McPAT:icache read_accesses)
	 Number of branch mispredictions (McPAT: branch_mispredictions)
	 */
	out << "Fetch stage" << endl;
	numICacheAccesses = numInstr;
	out << "Number of I-cache accesses: " << numICacheAccesses << endl;
	numBranchMisPred = bpred_mispred_count;
	out << "Number of branch mispredictions: " << numBranchMisPred << endl;

	/*
	 Instruction Decode Stage
	 Number of scheduler accesses (i.e. the number of times a scheduler is accessed. Since we do not have a scheduler in our simulator, if there is an instruction in the decode stage, you just increment this counter value. Based on instruction type, you have to increment floating point and integer instructions. McPAT: ROB_reads, ROB_writes, rename_accesses, fp_rename_accesses, inst_window_reads, inst_window_writes, inst_window_wakeup_accesses, fp_inst_window_reads, fp_inst_window_writes, fp_inst_window_wakeup_accesses)
	 Number of register reads for integer operations (McPAT: int_regfile_reads, ialu_access)
	 Number of register reads for fp operations (McPAT: float_regfile_reads, fpu_access) (You can use the number of src operands.)
	 */
	out << "Instruction Decode stage" << endl;
	out << "Number of scheduler accesses: " << numSchedAccesses
				<< endl;
	out << "Number of integer scheduler accesses: " << numIntSchedAccesses
			<< endl;
	out << "Number of floating point scheduler accesses: " << numfpSchedAccesses
			<< endl;
	out << "Number of register reads for integer operations: " << numIntRegReads
			<< endl;
	out << "Number of register reads for fp operations: " << numfpRegReads
			<< endl;

	/*
	 Execution Stage
	 Number of multiply instructions (OP_IMUL, OP_MM in simulator) (McPAT: mul_accesses)
	 */
	out << "Execution stage" << endl;
	out << "Number of multiply instructions: " << numMultInstr << endl;

	/*
	 Memory
	 Number of data cache reads (McPAT: dtlb:total_accesses (read+write), dcache:read_accesses)
	 Number of data cache writes (McPAT: dtlb:total_accesses (read+write), dcache:write_accesses)
	 Number of memory accesses (McPAT: system.Noc0:total_accesses = 4 times of number of memory accesses, system.mem.Memory accesses, system.mem.memory_reads, system.mem.memory_writes, (system.mc.memory_accesses, = 4 times of number of memory accesses system.mc.memory_reads = system.mc.memory_writes = 2 times of number of memory accesses))
	 */
	out << "Memory stage" << endl;
	out << "Number of data cache reads: " << numDCacheReads << endl;
	numDCacheWrites = dcache_miss_count;
	out << "Number of data cache writes: " << numDCacheWrites << endl;
	numMemAccesses = dram_row_buffer_hit_count + dram_row_buffer_miss_count;
	out << "Number of memory accesses: " << numMemAccesses << endl;

	/*
	 WB
	 Number of register writes for integer operations (int_regfile_writes)
	 Number of register writes for fp operations (float_regfile_writes)
	 */
	out << "WB stage" << endl;
	out << "Number of register writes for integer operations: "
			<< numIntRegWrites << endl;
	out << "Number of register writes for fp operations: " << numfpRegWrites
			<< endl;

	out.close();
}

/*******************************************************************/

/*******************************************************************/
/*  Support Functions  */
/*******************************************************************/

bool get_op(Op *op) {
	static UINT64 unique_count = 0;
	Trace_op trace_op;
	bool success = FALSE;
	// read trace
	// fill out op info
	// return FALSE if the end of trace
	//success = (gzread(g_stream, &trace_op, sizeof(Trace_op)) >0 );
	success =
			(gzread(g_stream, &trace_op, sizeof(Trace_op)) == sizeof(Trace_op));

	/* copy trace structure to op */
	if (success) {
		if (KNOB(KNOB_PRINT_INST)->getValue())
			dprint_trace(&trace_op);
		copy_trace_op(&trace_op, op);

		op->inst_id = unique_count++;
		op->valid = TRUE;
		last_inst_id = op->inst_id + 1;
	} else {
		if (unique_count == 0) {
			//cout << "Error in trace file" << endl;
			exit(0);
		}
		//last_inst_id = unique_count - 1;
		trace_over = true;
	}

	return success;
}
/* return op execution cycle latency */

int get_op_latency(Op *op) {
	assert(op->opcode < NUM_OP_TYPE);
	return op_latency[op->opcode];
}

/* Print out all the register values */
void dump_reg() {
	for (int ii = 0; ii < NUM_REG; ii++) {
		std::cout << cycle_count << ":register[" << ii << "]: V:"
				<< register_file[ii].valid << endl;
	}
}

void print_pipeline() {
	std::cout << "--------------------------------------------" << endl;
	std::cout << "cycle count : " << dec << cycle_count
			<< " retired_instruction : " << retired_instruction << endl;
	std::cout << (int) cycle_count << " FE: ";
	if (FE_latch->op_valid) {
		Op *op = FE_latch->op;
		cout << (int) op->inst_id;
	} else {
		cout << "####";
	}
	std::cout << " ID: ";
	if (ID_latch->op_valid) {
		Op *op = ID_latch->op;
		cout << (int) op->inst_id;
	} else {
		cout << "####";
	}
	std::cout << " EX: ";
	if (EX_latch->op_valid) {
		Op *op = EX_latch->op;
		cout << (int) op->inst_id;
	} else {
		cout << "####";
	}

	std::cout << " MEM: ";
	if (MEM_latch->op_valid || (!temp_mem_queue.empty())) {
		if (MEM_latch->op_valid) {
			list<Op *>::iterator cii;
			for (cii = MEM_latch->op_queue.begin();
					cii != MEM_latch->op_queue.end(); cii++) {
				Op *entry = (*cii);
				cout << " " << (int) (entry->inst_id);
			}
		}
		if (!temp_mem_queue.empty()) {
			list<Op *>::iterator cii;
			for (cii = temp_mem_queue.begin(); cii != temp_mem_queue.end();
					cii++) {
				Op *entry = (*cii);
				cout << " " << (int) (entry->inst_id);
			}
		}
	} else {
		cout << "####";
	}
	cout << endl;
	//  dump_reg();
	std::cout << "--------------------------------------------" << endl;
}

void print_heartbeat() {
	static uint64_t last_cycle;
	static uint64_t last_inst_count;
	float temp_ipc = float(retired_instruction - last_inst_count)
			/ (float) (cycle_count - last_cycle);
	float ipc = float(retired_instruction) / (float) (cycle_count);
	/* Do not modify this function. This messages will be used for grading */
	cout << "**Heartbeat** cycle_count: " << cycle_count << " inst:"
			<< retired_instruction << " IPC: " << temp_ipc << " Overall IPC: "
			<< ipc << endl;
	last_cycle = cycle_count;
	last_inst_count = retired_instruction;
}
/*******************************************************************/
/*                                                                 */
/*******************************************************************/

bool run_a_cycle(memory_c *main_memory) {
	long int retired_instruction_cpy = 0;
	int terminate_count = 0;
	bool terminate_program = false;
	for (;;) {
		if ((KNOB(KNOB_MAX_SIM_COUNT)->getValue()
				&& (cycle_count >= KNOB(KNOB_MAX_SIM_COUNT)->getValue()))
				|| (KNOB(KNOB_MAX_INST_COUNT)->getValue()
						&& (retired_instruction
								>= KNOB(KNOB_MAX_INST_COUNT)->getValue()))
				|| (sim_end_condition) || terminate_program) {
			// please complete sim_end_condition
			// finish the simulation
			print_heartbeat();
			print_stats();
			return TRUE;
		}
		cycle_count++;
		if (!(cycle_count % 5000)) {
			print_heartbeat();
		}

		/*section to terminate the program if it fails to exit normally*/
		if (terminate_count > 1000) {
			if (retired_instruction_cpy == retired_instruction)
				terminate_program = true;
			else {
				retired_instruction_cpy = retired_instruction;
				terminate_count = 0;
			}
		} else
			terminate_count++;
		/*section to terminate the program if it fails to exit normally*/

		main_memory->run_a_cycle();          // *NEW-LAB2

		WB_stage();
		MEM_stage(main_memory); // please modify MEM_stage function argument  /** NEW-LAB2 */
		EX_stage();
		ID_stage();
		FE_stage();

		if (trace_over && main_memory->all_mem_structures_empty()
				&& pipeline_latches_empty())
			sim_end_condition = true;
		if (KNOB(KNOB_PRINT_PIPE_FREQ)->getValue()
				&& !(cycle_count % KNOB(KNOB_PRINT_PIPE_FREQ)->getValue()))
			print_pipeline();
	}
	return TRUE;
}

/*******************************************************************/
/* Complete the following fuctions.  */
/* You can add new data structures and also new elements to Op, Pipeline_latch data structure */
/*******************************************************************/

void init_structures(memory_c *main_memory) // please modify init_structures function argument  /** NEW-LAB2 */ 
		{
	init_op_pool();
	init_op_latency();
	/* please initialize other data stucturs */
	/* you must complete the function */
	init_latches();
	init_register_file();	//fn added by apkarande
	main_memory->init_mem();	//initialize main memory
	cache_init(data_cache, KNOB(KNOB_DCACHE_SIZE)->getValue(),
	KNOB(KNOB_BLOCK_SIZE)->getValue(),
	KNOB(KNOB_DCACHE_WAY)->getValue(), "L1 dcache");	//initialize data cache

	//Initialize lab4 counters
	//Overall
	numInstr = 0;
	numSimCyles = 0;
	numfpInstr = 0;
	numIntInstr = 0;
	numBranchInstr = 0;
	numLDInstr = 0;
	numSTInstr = 0;
	//Fetch
	numICacheAccesses = 0;
	numBranchMisPred = 0;
	//ID
	numSchedAccesses = 0;
	numIntSchedAccesses = 0;
	numfpSchedAccesses = 0;
	numIntRegReads = 0;
	numfpRegReads = 0;
	//Execution
	numMultInstr = 0;
	//Memory
	numDCacheReads = 0;
	numDCacheWrites = 0;
	numMemAccesses = 0;
	//WB
	numIntRegWrites = 0;
	numfpRegWrites = 0;

}

#define MEM_MISPRED_BRANCH		(retire_op->mispredicted_branch == true)
#define MEM_CONDITIONAL_BRANCH (retire_op->cf_type==CF_CBR)

void WB_stage() {
	/* You MUST call free_op function here after an op is retired */
	/* you must complete the function */
	while (!MEM_latch->op_queue.empty()) {
		Op *retire_op = MEM_latch->op_queue.front();

		if (retire_op->cf_type >= CF_BR
				&& ((!use_bpred)
						|| (use_bpred
								&& (MEM_CONDITIONAL_BRANCH && MEM_MISPRED_BRANCH)))) {
			FE_latch->stage_stall = false;
		}
		if ((retire_op->dst != -1)
				&& (reg_writing_ops[retire_op->dst] == retire_op->inst_id)) {
			register_file[retire_op->dst].valid = true;
			// increment reg write counter
			if (is_fp(retire_op)) {
				numfpRegWrites++;
			} else if (is_int(retire_op)){
				numIntRegWrites++;
			}

		}
		retired_instruction++;
		MEM_latch->op_queue.pop_front();
		free_op(retire_op);

		//if(retire_op->inst_id == last_inst_id)
		//sim_end_condition = true;
	}
	MEM_latch->op_valid = false;		//not used
}

void MEM_stage(memory_c *main_memory) // please modify MEM_stage function argument  /** NEW-LAB2 */
		{
	/* you must complete the function */
	static int latency_count;
	static bool mshr_full = false;
	static bool address_translation_done = false;
	static bool tlb_previously_missed = false;
	static bool pte_load_in_progress = false;
	if (EX_latch->op_valid == true) {
		/********* LAB 3 ************/
		if ((EX_latch->op)->mem_type >= 1) {
			// If virtual memory enabled but translation not done
			if (vmem_enabled == true && address_translation_done == false) {
				uint64_t vaddr, phyFNum, phy_addr, virPNum;

				if ((EX_latch->op)->mem_type == MEM_LD)
					vaddr = (EX_latch->op)->ld_vaddr;
				else
					vaddr = (EX_latch->op)->st_vaddr;

				virPNum = (vaddr) / vmem_page_size;

				if (tlb_access(dtlb, virPNum, 0, &phyFNum) == true)	//if a TLB hit
						{
					if (tlb_previously_missed == false)
						dtlb_hit_count++;
					else
						tlb_previously_missed = false;
					uint64_t page_offset = vaddr % vmem_page_size;
					phy_addr = ((phyFNum) * vmem_page_size) + page_offset;
					//cout << (EX_latch->op)->inst_id << " " << phy_addr << endl;
					if ((EX_latch->op)->mem_type == MEM_LD)
						(EX_latch->op)->ld_vaddr = phy_addr;
					else
						(EX_latch->op)->st_vaddr = phy_addr;
					address_translation_done = true;
					EX_latch->stage_stall = false;//remove any stalls due to TLB
				} else						//if TLB miss
				{
					if (tlb_previously_missed == false) {
						dtlb_miss_count++;
						tlb_previously_missed = true;
					}
					uint64_t pteaddr = vmem_get_pteaddr(virPNum, 0);
					EX_latch->stage_stall = true;//if you get a TLB miss, stall pipeline for sure
					if (pte_load_in_progress == false)//if pte request not put into MSHR, check in dcache
							{
						if (mshr_full == false) {
							if (latency_count == 0) {
								latency_count =
								KNOB(KNOB_DCACHE_LATENCY)->getValue();
							}
							latency_count--;
							if (latency_count == 0) {
								numDCacheReads++;
								if (dcache_access(pteaddr)) {
									dcache_hit_count++;
									uint64_t pfnum = vmem_vpn_to_pfn(virPNum,
											0);
									tlb_install(dtlb, virPNum, 0, pfnum);
								} else {
									//cout << "tlb cache miss" <<endl;
									dcache_miss_count++;
									dummy_op->valid = true;
									dummy_op->mem_type = MEM_LD;
									dummy_op->ld_vaddr = pteaddr;
									dummy_op->mem_read_size = 4;//some non-zero value
									dummy_op->opcode = OP_LD_PTE;//to distinguish a LD PTE op

									if (main_memory->insert_mshr(dummy_op)
											== true) {
										//cout << "dummy_op_insert" << endl;
										pte_load_in_progress = true;
										pte_load_done = false;
										mshr_full = false;
									} else {
										EX_latch->stage_stall = true;//if mshr is full
										mshr_full = true;
									}
								}
							}
						} else {
							if (main_memory->insert_mshr(dummy_op) == true)	//if cannot be piggybacked, try inserting a new entry
									{
								pte_load_in_progress = true;
								pte_load_done = false;
								mshr_full = false;
							} else {
								EX_latch->stage_stall = true;//if mshr is full
								mshr_full = true;
							}
						}
					} else {
						if (pte_load_done) {
							uint64_t pfnum = vmem_vpn_to_pfn(virPNum, 0);
							tlb_install(dtlb, virPNum, 0, pfnum);
							pte_load_done = false;
							pte_load_in_progress = false;
						}
					}
				}
			}
			// If virtual memory enabled & translation complete OR virtual memory disabled
			if ((vmem_enabled == true && address_translation_done == true)
					|| (vmem_enabled == false)) {
				/* Do everything*/
				if ((EX_latch->op)->mem_type == MEM_LD) {
					if (mshr_full == false) {
						if (latency_count == 0) {
							latency_count =
							KNOB(KNOB_DCACHE_LATENCY)->getValue();
							EX_latch->stage_stall = true;
						}
						latency_count--;
						if (latency_count == 0) {
							numDCacheReads++;
							if (dcache_access((EX_latch->op)->ld_vaddr)) {
								address_translation_done = false;//reset the address_translation_done flag
								dcache_hit_count++;
								EX_latch->op_valid = false;
								EX_latch->stage_stall = false;
								fill_retire_queue(EX_latch->op);//not really a broadcast, just using available function
							} else {
								//cout << "op cache access" << endl;
								dcache_miss_count++;
								if (main_memory->store_load_forwarding(
										EX_latch->op))//check if store load fwding is possible
										{
									//cout << "store-load " << EX_latch->op->inst_id << endl;
									address_translation_done = false;//reset the address_translation_done flag
									store_load_forwarding_count++;
									EX_latch->op_valid = false;
									EX_latch->stage_stall = false;

									fill_retire_queue(EX_latch->op);//not really a broadcast, just using available function
								} else if (main_memory->check_piggyback(
										EX_latch->op) == false) {
									if (main_memory->insert_mshr(EX_latch->op)
											== true)//if cannot be piggybacked, try inserting a new entry
											{
										//cout << "op_insert" << endl;
										address_translation_done = false;//reset the address_translation_done flag
										EX_latch->op_valid = false;
										EX_latch->stage_stall = false;
										mshr_full = false;
									} else {
										EX_latch->stage_stall = true;//if mshr is full
										mshr_full = true;
									}
								} else {
									address_translation_done = false;//reset the address_translation_done flag
									EX_latch->op_valid = false;
									EX_latch->stage_stall = false;
								}
							}
						}
					} else {
						if (main_memory->insert_mshr(EX_latch->op) == true)	//if cannot be piggybacked, try inserting a new entry
								{
							address_translation_done = false;//reset the address_translation_done flag
							EX_latch->op_valid = false;
							EX_latch->stage_stall = false;
							mshr_full = false;
						} else {
							EX_latch->stage_stall = true;	//if mshr is full
							mshr_full = true;
						}
					}
				} else if ((EX_latch->op)->mem_type == MEM_ST) {
					if (mshr_full == false) {
						if (latency_count == 0) {
							latency_count =
							KNOB(KNOB_DCACHE_LATENCY)->getValue();
							EX_latch->stage_stall = true;
						}
						latency_count--;
						if (latency_count == 0) {
							numDCacheReads++;
							if (dcache_access((EX_latch->op)->st_vaddr)) {
								address_translation_done = false;//reset the address_translation_done flag
								dcache_hit_count++;
								EX_latch->op_valid = false;
								EX_latch->stage_stall = false;

								fill_retire_queue(EX_latch->op);//not really a broadcast, just using available function
							} else {
								dcache_miss_count++;
								if (main_memory->store_store_forwarding(
										EX_latch->op))//check if store load fwding is possible
										{
									address_translation_done = false;//reset the address_translation_done flag
									store_store_forwarding_count++;
									EX_latch->op_valid = false;
									EX_latch->stage_stall = false;

									fill_retire_queue(EX_latch->op);//not really a broadcast, just using available function
								} else if (main_memory->check_piggyback(
										EX_latch->op) == false) {
									if (main_memory->insert_mshr(EX_latch->op)
											== true)//if cannot be piggybacked, try inserting a new entry
											{
										address_translation_done = false;//reset the address_translation_done flag
										EX_latch->op_valid = false;
										EX_latch->stage_stall = false;

										mshr_full = false;
									} else {
										EX_latch->stage_stall = true;//if mshr is full
										mshr_full = true;
									}
								} else {
									address_translation_done = false;//reset the address_translation_done flag
									EX_latch->op_valid = false;
									EX_latch->stage_stall = false;
								}
							}
						}
					} else {
						if (main_memory->insert_mshr(EX_latch->op) == true)	//if cannot be piggybacked, try inserting a new entry
								{
							address_translation_done = false;//reset the address_translation_done flag
							EX_latch->op_valid = false;
							EX_latch->stage_stall = false;
							mshr_full = false;
						} else {
							EX_latch->stage_stall = true;	//if mshr is full
							mshr_full = true;
						}
					}
				}

			}
		} else {
			EX_latch->op_valid = false;
			EX_latch->stage_stall = false;

			fill_retire_queue(EX_latch->op);//not really a broadcast, just using available function
		}
	}
}

void EX_stage() {
	/* you must complete the function */
	static int ex_latency = 0;
	if (ID_latch->op_valid == true) {

		if (EX_latch->stage_stall == false) {

			if (ex_latency == 0)
				ex_latency = get_op_latency(ID_latch->op);//check execution latency of op
			ex_latency--;
			if (ex_latency == 0) {
				ID_latch->op_valid = false;	//set op as invalid for previous stage since op is already transferred
				ID_latch->stage_stall = false;	//if stage stalled remove stall
				EX_latch->op = ID_latch->op;	//transfer pointer to next stage
				EX_latch->op_valid = true;		// set op as valid for the stage

				// increment multiply counter
				if ((EX_latch->op)->opcode == OP_IMUL
						|| (EX_latch->op)->opcode == OP_MM) {
					numMultInstr++;
				}

			} else
				ID_latch->stage_stall = true;
		} else
			ID_latch->stage_stall = true;
	}
}

void ID_stage() {
	/* you must complete the function */
#define NUM_SRC_1			((FE_latch->op)->num_src==1)
#define NUM_SRC_2			((FE_latch->op)->num_src==2)
#define SRC(x)				(FE_latch->op)->src[x]
#define SRC_INVALID(x)		!(register_file[SRC(x)].valid)
#define FE_MISPRED_BRANCH		((FE_latch->op)->mispredicted_branch == true)
#define FE_CONDITIONAL_BRANCH ((FE_latch->op)->cf_type==CF_CBR)

	if (FE_latch->op_valid == true)		//check if inst lies in the FE latch
			{
		// increment reg read counter and scheduler access counter
		numSchedAccesses++;
		if (is_fp(FE_latch->op)) {
			numfpSchedAccesses++;
			numfpRegReads++;
		} else if (is_int(FE_latch->op)) {
			numIntSchedAccesses++;
			numIntRegReads++;
		}
		if ((NUM_SRC_1 && (SRC_INVALID(0)))
				|| (NUM_SRC_2 && (SRC_INVALID(0) || SRC_INVALID(1))))//check for source-dest clash
				{
			if (ID_latch->stage_stall == false) {
				if ((FE_latch->op)->cf_type >= CF_BR)//irrespective of branch predictor enabled or disabled
					control_hazard_count++;
				data_hazard_count++;
			}
			FE_latch->stage_stall = true;
		} else {
			FE_latch->stage_stall = false;

			if (ID_latch->stage_stall == false)	//transfer op only if ID stage is not stalled
					{
				FE_latch->op_valid = false;
				ID_latch->op = FE_latch->op;
				ID_latch->op_valid = true;
				if ((FE_latch->op)->dst != -1)//if dest is valid then mark the specific register as busy/invalid and indicate which op writes to the reg
						{
					register_file[(FE_latch->op)->dst].valid = false;
					reg_writing_ops[(FE_latch->op)->dst] =
							FE_latch->op->inst_id; // in WB stage, clear valid flag of register only if this is the op
				}
				//checking branch in the end is important
				if ((FE_latch->op)->cf_type >= CF_BR) {

					if ((!use_bpred)
							|| (use_bpred
									&& (FE_CONDITIONAL_BRANCH
											&& FE_MISPRED_BRANCH))) {
						control_hazard_count++;
						FE_latch->stage_stall = true;
					}
				}
			} else {
				FE_latch->stage_stall = true;
			}
		}
	}

}

void FE_stage() {
	/* only part of FE_stage function is implemented */
	/* please complete the rest of FE_stage function */

	if (FE_latch->stage_stall == false)	//check if no pipeline stalled
			{
		Op* new_op = get_free_op();	//get a placeholder op out of the pool
		if (get_op(new_op))		//copy op from trace into the placeholder op
				{
			if ((new_op->opcode == OP_CF) && new_op->cf_type == CF_CBR
					&& use_bpred == true) {
				uint64_t pc = new_op->instruction_addr;
				bool predicted_dir = 0;
				predicted_dir = bpred_access(branchpred, pc);
				bpred_update(branchpred, pc, predicted_dir,
						new_op->actually_taken);
				if (new_op->actually_taken != predicted_dir) {
					new_op->mispredicted_branch = true;
				}
				bpred_mispred_count = branchpred->mispred;
				bpred_okpred_count = branchpred->okpred;
			}

			FE_latch->op = new_op;
			FE_latch->op_valid = true;

			// increment instruction type counters
			if (is_fp(FE_latch->op)) {
				// increment # of fp operations
				numfpInstr++;
			} else if (is_int(FE_latch->op)){
				// increment # of integer operations
				numIntInstr++;
			}
			if ((FE_latch->op)->opcode == OP_CF) {
				// increment # of branch instructions
				numBranchInstr++;
			}
			if ((FE_latch->op)->mem_type == MEM_LD) {
				// increment # of load instructions
				numLDInstr++;
			}
			if ((FE_latch->op)->mem_type == MEM_ST) {
				// increment # of store instructions
				numSTInstr++;
			}

		} else
			free_op(new_op);
	}

	//   next_pc = pc + op->inst_size;  // you need this code for building a branch predictor

}

void init_latches() {
	MEM_latch = new pipeline_latch();
	EX_latch = new pipeline_latch();
	ID_latch = new pipeline_latch();
	FE_latch = new pipeline_latch();

	MEM_latch->op = NULL;
	EX_latch->op = NULL;
	ID_latch->op = NULL;
	FE_latch->op = NULL;

	/* you must set valid value correctly  */
	MEM_latch->op_valid = false;
	EX_latch->op_valid = false;
	ID_latch->op_valid = false;
	FE_latch->op_valid = false;

	MEM_latch->stage_stall = false;
	EX_latch->stage_stall = false;
	ID_latch->stage_stall = false;
	FE_latch->stage_stall = false;
}

bool icache_access(ADDRINT addr) { /** please change uint32_t to ADDRINT NEW-LAB2 */

	/* For Lab #1, you assume that all I-cache hit */
	bool hit = FALSE;
	if (KNOB(KNOB_PERFECT_ICACHE)->getValue())
		hit = TRUE;
	return hit;
}

bool dcache_access(ADDRINT addr) { /** please change uint32_t to ADDRINT NEW-LAB2 */
	/* For Lab #1, you assume that all D-cache hit */
	/* For Lab #2, you need to connect cache here */   // NEW-LAB2
	bool hit = FALSE;
	if (KNOB(KNOB_PERFECT_DCACHE)->getValue())
		hit = TRUE;
	else {
		hit = cache_access(data_cache, addr);
	}
	return hit;
}

//fn added by apk
void init_register_file() {
	for (int i = 0; i < NUM_REG; i++)
		register_file[i].valid = true;	//all registers are initially available
}

void fill_retire_queue(Op* op)             // NEW-LAB2 
		{                                          // NEW-LAB2
	/* you must complete the function */     // NEW-LAB2
	// mem ops are done.  move the op into WB stage   // NEW-LAB2
	MEM_latch->op_queue.push_back(op);
	MEM_latch->op_valid = true;
}

bool pipeline_latches_empty() {
	if (FE_latch->op_valid == false && ID_latch->op_valid == false
			&& EX_latch->op_valid == false && MEM_latch->op_valid == false)
		return true;
	else
		return false;
}

/*
 For floating point operations, you should consider:
 OP_FMEM, // fp memory instruction
 OP_FCF,
 OP_FCVT, // floating point convert
 OP_FADD, // floating point add
 OP_FMUL, // floating point multiply
 OP_FDIV, // floating point divide
 OP_FCMP, // floating point compare
 OP_FBIT, // floating point bit
 OP_FCMO, // floating point cond move

 For integer operations, you should consider:
 OP_CF, // change of flow
 OP_CMOV, // conditional move
 OP_LDA, // load address
 OP_IADD, // integer add
 OP_IMUL, // integer multiply
 OP_ICMP, // integer compare
 OP_IDIV, // integer divide
 OP_LOGIC, // logical
 OP_SHIFT, // shift
 OP_BYTE, // byte manipulation
 OP_MM, // multimedia instructions
 */

bool is_fp(Op* op) {
	if (op->opcode == OP_FMEM || op->opcode == OP_FCF || op->opcode == OP_FCVT
			|| op->opcode == OP_FADD || op->opcode == OP_FMUL
			|| op->opcode == OP_FDIV || op->opcode == OP_FCMP
			|| op->opcode == OP_FBIT || op->opcode == OP_FCMO) {
		return true;
	} else {
		return false;
	}
}
bool is_int(Op* op) {
	if (op->opcode == OP_CF || op->opcode == OP_CMOV || op->opcode == OP_LDA
			|| op->opcode == OP_IADD || op->opcode == OP_IMUL
			|| op->opcode == OP_ICMP || op->opcode == OP_IDIV
			|| op->opcode == OP_LOGIC || op->opcode == OP_SHIFT
			|| op->opcode == OP_BYTE || op->opcode == OP_MM) {
		return true;
	} else {
		return false;
	}
}
