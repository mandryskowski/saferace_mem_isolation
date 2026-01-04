typedef struct HackResults {
  ulong index;
  int value;
  int padding; // since this needs to be aligned to 8 bytes
} HackResults;

typedef struct HackParams {
  ulong memLength;
  uint blockSize;
  int canary;
} HackParams;

__kernel void hack(__global HackParams * params, __global HackResults * hackResults) {
  int priv_mem[8];
  for (uint i = 0; i < params->blockSize; i++) {
    uint idx = i;
//    uint idx =  i + 8;
    if (priv_mem[idx] == params->canary) {
      hackResults[get_global_id(0)].index = (ulong) idx + 1;
      hackResults[get_global_id(0)].value = priv_mem[idx + 1];
      return;
    }
  }
}
