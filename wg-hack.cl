typedef struct HackParams {
  ulong memLength;
  uint blockSize;
  int canary;
} HackParams;

typedef struct HackResults {
  ulong index;
  int value;
  int padding; // since this needs to be aligned to 8 bytes
} HackResults;

__kernel void hack(__global HackParams * params, __global HackResults * hackResults, __local int * mem) {
  for (uint i = 0; i < params->blockSize; i++) {
    uint idx = get_local_id(0) * params->blockSize + i;
//    uint idx = (uint) params->memLength + get_local_id(0) * params->blockSize + i;
    if (mem[idx] == params->canary) {
      hackResults[get_global_id(0)].index = (ulong) idx + 1;
      hackResults[get_global_id(0)].value = mem[idx + 1];
      return;
    }
  }
}

