typedef struct HackResults {
  ulong addr;
  int value;
  int padding; // since this needs to be aligned to 8 bytes
} HackResults;

typedef struct HackParams {
  ulong addr;
  uint blockSize;
  int canary;
} HackParams;

__kernel void hack(__global HackParams * params, __global HackResults * hackResults) {
    ulong baseAddr = params->addr;
    for (uint i = 0; i < params->blockSize; i++) {
      ulong offset = (ulong) (get_global_id(0) * params->blockSize + i) * 4;
      ulong addr = baseAddr + offset;
      int val = (int) addr; // manually cast to pointer and load from it in spv
      if (val == params->canary) {
        hackResults[get_global_id(0)].addr = addr;
        hackResults[get_global_id(0)].value = val;
        return;
      }
    }
}
