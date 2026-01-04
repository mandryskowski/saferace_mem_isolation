typedef struct HackParams {
  ulong memLength;
  uint blockSize;
  int secretValue;
} HackParams;

typedef struct WriteSecretParams {
  int secretValue;
  int canary;
  uint blockSize;
} WriteSecretParams;

typedef struct HackResults {
  ulong index;
  int value;
  int padding; // since this needs to be aligned to 8 bytes
} HackResults;

__kernel void poison(__global HackParams * params, __global int * mem) {
  for (uint i = 0; i < params->blockSize; i++) {
    uint idx = get_global_id(0) * params->blockSize + i;
    mem[idx] = params->secretValue;
  }
}

__kernel void getPoisoned(__global WriteSecretParams * params, __global HackResults * hackResults, __global int * mem) {
  for (uint i = 0; i < params->blockSize; i++) {
    uint idx = get_global_id(0) * params->blockSize + i;
    int val = mem[idx];
    if (val == params->secretValue) {
      hackResults[get_global_id(0)].index = (ulong) idx;
      hackResults[get_global_id(0)].value = val;
    }
  }
}
