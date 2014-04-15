// Add this code snippet to appropriate place in sim.cpp


#include "bpred.h"
#include "vmem.h"

uint64_t bpred_mispred_count = 0;  /* total number of branch mispredictions */  // NEW-LAB3
uint64_t bpred_okpred_count = 0;   /* total number of correct branch predictions */  // NEW-LAB3
uint64_t dtlb_hit_count = 0;       /* total number of DTLB hits */ // NEW-LAB3
uint64_t dtlb_miss_count = 0;      /* total number of DTLB miss */ // NEW-LAB3

bpred *branchpred; // NEW-LAB3 (student need to initialize)
tlb *dtlb;        // NEW-LAB3 (student need to intialize)


/*******************************************************************/
/*  Print messages  */
/*******************************************************************/
void print_stats() {
  std::ofstream out((KNOB(KNOB_OUTPUT_FILE)->getValue()).c_str());
  /* Do not modify this function. This messages will be used for grading */ 
  out << "Total instruction: " << retired_instruction << endl; 
  out << "Total cycles: " << cycle_count << endl; 
  float ipc = (cycle_count ? ((float)retired_instruction/(float)cycle_count): 0 );
  out << "IPC: " << ipc << endl; 
  out << "Total I-cache miss: " << icache_miss_count << endl; 
  out << "Total D-cache miss: " << dcache_miss_count << endl; 
  out << "Total D-cache hit: " << dcache_hit_count << endl;
  out << "Total data hazard: " << data_hazard_count << endl;
  out << "Total control hazard : " << control_hazard_count << endl;   
  out << "Total DRAM ROW BUFFER Hit: " << dram_row_buffer_hit_count << endl; 
  out << "Total DRAM ROW BUFFER Miss: "<< dram_row_buffer_miss_count << endl; 
  out <<" Total Store-load forwarding: " << store_load_forwarding_count << endl; 

  // new for LAB3
  out << "Total Branch Predictor Mispredictions: " << bpred_mispred_count << endl;   
  out << "Total Branch Predictor OK predictions: " << bpred_okpred_count << endl;   
  out << "Total DTLB Hit: " << dtlb_hit_count << endl; 
  out << "Total DTLB Miss: " << dtlb_miss_count << endl; 

  out.close();
}

/*******************************************************************/

