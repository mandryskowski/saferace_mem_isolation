typedef struct WriteSecretParams {
  int secretValue;
  int canary;
  uint blockSize;
} WriteSecretParams;

__kernel void writeSecret(__global WriteSecretParams * params, __local int * mem) {
  for (uint i = 0; i < params->blockSize; i++) {
    int val;
    if (i % 2 == 0) {
      val = params->canary;
    } else {
      val = params->secretValue;
    }
    mem[get_local_id(0) * params->blockSize + i] = val;
  }
}

