typedef struct WriteSecretParams {
  int secretValue;
  int canary;
  uint blockSize;
} WriteSecretParams;

__kernel void writeSecret(__global WriteSecretParams * params, __global int * mem) {
  int priv_mem[16];
  for (uint i = 0; i < 16; i++) {
    int val;
    if (i % 2 == 0) {
      val = params->canary;
    } else {
      val = params->secretValue;
    }
    priv_mem[i] = val;
  }
  mem[get_global_id(0)] = priv_mem[get_global_id(0) % 16];
}

