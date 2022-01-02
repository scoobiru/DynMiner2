#include "cMiner.h"

void cMiner::startMiner(string params, cGetWork *getWork, cSubmitter* submitter, cStatDisplay *statDisplay) {

    vector<string> vParams = split(params, ",");

    char type = tolower(vParams[0].c_str()[0]);
    if ((type != 'c') && (type != 'g')) {
        printf("Invalid miner type - must be GPU or CPU: %s\n", params.c_str());
        exit(0);
    }

    int numThread = atoi(vParams[1].c_str());
    int platformID;
    int deviceID;

    if (type == 'g') {
        if (vParams.size() != 5) {
            printf("GPU miner must specify work items, platform ID and device ID\n");
            exit(0);
        }
        int workSize = atoi(vParams[2].c_str());
        int platformID = atoi(vParams[3].c_str());
        int deviceID = atoi(vParams[4].c_str());
        startGPUMiner(numThread, platformID, deviceID, getWork, submitter, statDisplay);
    }
    else
        printf("CPU miner not implemented\n");


}


static inline uint32_t CLZz(register uint32_t x)
{
    x |= x >> 1;
    x |= x >> 2;
    x |= x >> 4;
    x |= x >> 8;
    x |= x >> 16;
    x -= ((x >> 1) & 0x55555555U);
    x = (x & 0x33333333U) + ((x >> 2) & 0x33333333U);
    return 32U - ((((x + (x >> 4)) & 0x0F0F0F0FU) * 0x01010101U) >> 24);
}


unsigned int countLeadingZeros(unsigned char* hash) {
    int c = CLZz((hash[0] << 24) + (hash[1] << 16) + (hash[2] << 8) + hash[3]);
    if (c == 32) {
        c += CLZz((hash[4] << 24) + (hash[5] << 16) + (hash[6] << 8) + hash[7]);
        if (c == 64) {
            c += CLZz((hash[8] << 24) + (hash[9] << 16) + (hash[10] << 8) + hash[11]);
            if (c == 96) {
                c += CLZz((hash[12] << 24) + (hash[13] << 16) + (hash[14] << 8) + hash[15]);
                if (c == 128) {
                    c += CLZz((hash[16] << 24) + (hash[17] << 16) + (hash[18] << 8) + hash[19]);
                    if (c == 160) {
                        c += CLZz((hash[20] << 24) + (hash[21] << 16) + (hash[22] << 8) + hash[23]);
                        if (c == 192) {
                            c += CLZz((hash[24] << 24) + (hash[25] << 16) + (hash[26] << 8) + hash[27]);
                            if (c == 224)
                                c += CLZz((hash[28] << 24) + (hash[29] << 16) + (hash[30] << 8) + hash[31]);
                        }
                    }
                }
            }
        }
    }
    return c;
}

void cMiner::startGPUMiner(const size_t computeUnits, int platformID, int deviceID, cGetWork *getWork, cSubmitter *submitter, cStatDisplay *statDisplay) {

    bool workReady = false;
    while (!workReady) {
        getWork->lockJob.lock();
        workReady = (getWork->workID != 0);
        getWork->lockJob.unlock();
        Sleep(10);
    }

    cl_int returnVal; 
    cl_uint ret_num_platforms;
    cl_uint numOpenCLDevices;

    cl_mem clGPUProgramBuffer;

    uint32_t hashResultSize;
    cl_mem clGPUHashResultBuffer;
    uint32_t* buffHashResult;

    uint32_t headerBuffSize;
    cl_mem clGPUHeaderBuffer;
    unsigned char* buffHeader;

    cl_platform_id* platform_id = (cl_platform_id*)malloc(16 * sizeof(cl_platform_id));
    cl_device_id* open_cl_devices = (cl_device_id*)malloc(16 * sizeof(cl_device_id));

    returnVal = clGetPlatformIDs(16, platform_id, &ret_num_platforms);
    returnVal = clGetDeviceIDs(platform_id[platformID], CL_DEVICE_TYPE_GPU, 16, open_cl_devices, &numOpenCLDevices);
    cl_context context = clCreateContext(NULL, 1, &open_cl_devices[deviceID], NULL, NULL, &returnVal);

    cl_program program = loadMiner(context, &open_cl_devices[deviceID]);

    kernel = clCreateKernel(program, "dyn_hash", &returnVal);
    commandQueue = clCreateCommandQueueWithProperties(context, open_cl_devices[deviceID], NULL, &returnVal);

    clGPUProgramBuffer = clCreateBuffer(context, CL_MEM_READ_WRITE | CL_MEM_ALLOC_HOST_PTR, getWork->programVM->byteCode.size() * 4, NULL, &returnVal);
    returnVal = clSetKernelArg(kernel, 0, sizeof(cl_mem), (void*)&clGPUProgramBuffer);
    //returnVal = clEnqueueWriteBuffer(commandQueue, clGPUProgramBuffer, CL_TRUE, 0, getWork->programVM->byteCode.size(), getWork->programVM->byteCode.data(), 0, NULL, NULL);

    hashResultSize = computeUnits * 32;
    clGPUHashResultBuffer = clCreateBuffer(context, CL_MEM_READ_WRITE | CL_MEM_ALLOC_HOST_PTR, hashResultSize, NULL, &returnVal);
    returnVal = clSetKernelArg(kernel, 1, sizeof(cl_mem), (void*)&clGPUHashResultBuffer);
    buffHashResult = (uint32_t*)malloc(hashResultSize);
    //memset(buffHashResult, 0, hashResultSize);
    //returnVal = clEnqueueWriteBuffer(command_queue[i], clGPUHashResultBuffer[i], CL_TRUE, 0, hashResultSize, buffHashResult[i], 0, NULL, NULL);

    headerBuffSize = 80;
    clGPUHeaderBuffer = clCreateBuffer(context, CL_MEM_READ_WRITE | CL_MEM_ALLOC_HOST_PTR, headerBuffSize, NULL, &returnVal);
    returnVal = clSetKernelArg(kernel, 2, sizeof(cl_mem), (void*)&clGPUHeaderBuffer);
    buffHeader = (unsigned char*)malloc(headerBuffSize);
    //memset(buffHeader[i], 0, headerBuffSize);
    //returnVal = clEnqueueWriteBuffer(command_queue[i], clGPUHeaderBuffer[i], CL_TRUE, 0, headerBuffSize, buffHeader[i], 0, NULL, NULL);

    while (true) {
        unsigned int nonce;
        int workID;

        time_t now;
        time(&now);
        srand(now);

        getWork->lockJob.lock();

        workID = getWork->workID;
        string jobID = getWork->jobID;
        memcpy(buffHeader, getWork->nativeData, 80);

        returnVal = clEnqueueWriteBuffer(commandQueue, clGPUProgramBuffer, CL_TRUE, 0, getWork->programVM->byteCode.size() * 4,
            getWork->programVM->byteCode.data(), 0, NULL, NULL);

        getWork->lockJob.unlock();


        nonce = rand() * rand() * now;

        while (workID == getWork->workID) {
            memcpy(&buffHeader[76], &nonce, 4);

            returnVal = clEnqueueWriteBuffer(commandQueue, clGPUHeaderBuffer, CL_TRUE, 0, headerBuffSize, buffHeader, 0, NULL, NULL);
            size_t localWorkSize = 64;
            returnVal = clEnqueueNDRangeKernel(commandQueue, kernel, 1, NULL, &computeUnits, &localWorkSize, 0, NULL, NULL);
            returnVal = clFinish(commandQueue);
            returnVal = clEnqueueReadBuffer(commandQueue, clGPUHashResultBuffer, CL_TRUE, 0, hashResultSize, buffHashResult, 0, NULL, NULL);
            submitter->addHashResults((unsigned char*)buffHashResult, computeUnits * 32, nonce, jobID);
            nonce += computeUnits;
            statDisplay->nonce_count += computeUnits;
        }
    }
    
}


vector<string> cMiner::split(string str, string token) {
    vector<string>result;
    while (str.size()) {
        int index = str.find(token);
        if (index != string::npos) {
            result.push_back(str.substr(0, index));
            str = str.substr(index + token.size());
            if (str.size() == 0)result.push_back(str);
        }
        else {
            result.push_back(str);
            str = "";
        }
    }
    return result;
}


cl_program cMiner::loadMiner(cl_context context, cl_device_id* deviceID) {

    FILE* kernelSourceFile;
    cl_int returnVal;


    kernelSourceFile = fopen("dyn_miner2.cl", "r");
    if (!kernelSourceFile) {
        fprintf(stderr, "Failed to load openCL kernel.\n");
        exit(0);
    }
    fseek(kernelSourceFile, 0, SEEK_END);
    size_t sourceFileLen = ftell(kernelSourceFile) + 1;
    char* kernelSource = (char*)malloc(sourceFileLen);
    memset(kernelSource, 0, sourceFileLen);
    fseek(kernelSourceFile, 0, SEEK_SET);
    size_t numRead = fread(kernelSource, 1, sourceFileLen, kernelSourceFile);
    fclose(kernelSourceFile);

    cl_program program;

    // Create kernel program
    program = clCreateProgramWithSource(context, 1, (const char**)&kernelSource, &numRead, &returnVal);
    returnVal = clBuildProgram(program, 1, deviceID, NULL, NULL, NULL);


    
    if (returnVal == CL_BUILD_PROGRAM_FAILURE) {
        printf("Error building openCL program:\n");
        size_t log_size;
        clGetProgramBuildInfo(program, *deviceID, CL_PROGRAM_BUILD_LOG, 0, NULL, &log_size);
        char* log = (char*)malloc(log_size);
        clGetProgramBuildInfo(program, *deviceID, CL_PROGRAM_BUILD_LOG, log_size, log, NULL);
        printf("\n\n%s\n", log);
        exit(0);
    }
    

    free(kernelSource);

    return program;

}