typedef struct HackResults {
  ulong addr;
  int value;
  int padding;
} HackResults;

typedef struct HackParams {
  ulong addr;
  uint blockSize;
  int secretValue;
} HackParams;

__kernel void testDeviceMem(__global HackParams * params, __global HackResults * hackResults) {
    ulong addr = params->addr;
    hackResults[get_global_id(0)].addr = addr;
    hackResults[get_global_id(0)].value = (int) addr;
}
