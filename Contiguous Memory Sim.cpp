#include <iostream>
#include <queue>
#include <string>
#include <vector>
#include <list>
#include <iterator>
using namespace std;

struct PCB
{
    int processID;
    int state; // 0 = new, 1 = ready, 2 = running, 3 = terminated, 4 = IOWaiting
    int programCounter;
    int instructionBase;
    int dataBase;
    int memoryLimit;
    int cpuCyclesUsed;
    int registerValue;
    int maxMemoryNeeded;
    int mainMemoryBase;
    vector<int> logicalMemory;
    int instructionSize;
    int startTime;
    int endTime;
};

struct IOWaitEntry
{
    int baseAddress;
    int entryTime;
    int ioCycles;
};

struct memoryBlock
{
    int processID = -1;
    int startingAddress;
    int size;

    memoryBlock(int processID, int startingAddress, int size)
    {
        this->processID = processID;
        this->startingAddress = startingAddress;
        this->size = size;
    }
};

// Function to check if there is a sufficient memory block available for the job required size
bool hasSufficientMemoryBlock(list<memoryBlock>& memoryList, int requiredSize) {

    for (auto block : memoryList) {
        if (block.processID == -1 && block.size >= requiredSize) {
            return true;
        }
    }
    return false;
}

// Function to coalesce adjacent free memory blocks
// This function merges adjacent free blocks into a single larger block
// and returns true if any coalescing was done
bool coalesce(list<memoryBlock> &memoryList) {
    if (memoryList.size() <= 1)
        return false;

    bool coalesced = false;
    bool merged;

    do {
        merged = false;
        auto current = memoryList.begin();
        while (current != memoryList.end() && std::next(current) != memoryList.end()) {
            auto nextBlock = std::next(current);
            if (current->processID == -1 && nextBlock->processID == -1 &&
                current->startingAddress + current->size == nextBlock->startingAddress) {
                current->size += nextBlock->size;
                memoryList.erase(nextBlock);
                coalesced = true;
                merged = true;
            } else {
                ++current;
            }
        }
    } while (merged);

    return coalesced;
}
// Function to print the current state of the memory list
// This function is used for debugging purposes
void printList(const list<memoryBlock> &memoryList)
{
    cout << "Memory List State:" << endl;
    for (auto it = memoryList.begin(); it != memoryList.end(); ++it)
    {
        cout << "PID: " << it->processID << ", Start: " << it->startingAddress
             << ", Size: " << it->size << endl;
    }
    cout << endl;
}


// Function to free a block of memory
// and update the memory list
void freeBlock(int processID, list<memoryBlock> &memoryList, int *mainMemory)
{
    bool found = false;
    for (auto it = memoryList.begin(); it != memoryList.end(); ++it)
    {
        if (it->processID == processID)
        {
            found = true;
            cout << "Process " << processID << " terminated and released memory from "
                 << it->startingAddress << " to "
                 << (it->startingAddress + it->size - 1) << "." << endl;

            for (int i = it->startingAddress; i < it->startingAddress + it->size; i++)
            {
                mainMemory[i] = -1;
            }

            it->processID = -1;
            //coalesce(memoryList);
           // printList(memoryList); // Debug output
            break;
        }
    }
    if (!found)
    {
        cout << "Error: Process " << processID << " not found in memory list for freeing." << endl;
    }
}
void checkIOWaitingQueue(queue<IOWaitEntry> &ioWaitQueue, int &globalClock, queue<int> &readyQueue, int *mainMemory)
{
    queue<IOWaitEntry> temp;
    while (!ioWaitQueue.empty())
    {
        IOWaitEntry entry = ioWaitQueue.front();
        ioWaitQueue.pop();
        if (globalClock - entry.entryTime >= entry.ioCycles)
        {
            // globalClock += entry.ioCycles; // Update global clock
            int base = entry.baseAddress;
            int processID = mainMemory[base];
            cout << "print" << endl;
            mainMemory[base + 1] = 1; // Ready
            readyQueue.push(base);
            cout << "Process " << processID << " completed I/O and is moved to the ReadyQueue." << endl;
        }
        else
        {
            temp.push(entry);
        }
    }
    ioWaitQueue = temp;
}
// Function to load jobs into memory
// This function checks if there is sufficient memory available
// and loads the job into memory if possible
// If not, it attempts to coalesce memory blocks
// and retry loading the job
// If still not possible, the job is left in the new job queue
// until memory becomes available
void loadJobsToMemory(queue<PCB> &newJobQueue, queue<int> &readyQueue, int *mainMemory,
                      int maxMemory, list<memoryBlock> &memoryList)
{
    int lastAddress = 0;
    while (!newJobQueue.empty())
    {

       // printList(memoryList); // Debug output

        queue<PCB> tempQueue;
        
        PCB newJob = newJobQueue.front();
        
        int pcbSize = 10;
        int totalSize = pcbSize + newJob.maxMemoryNeeded;
        bool loaded = false;

        for (auto it = memoryList.begin(); it != memoryList.end(); it++)
        {
            if (it->processID == -1 && it->size >= totalSize)
            {
                // Allocate the block
                int startAddress = it->startingAddress;
                it->processID = newJob.processID;
                int originalSize = it->size;
                it->size = totalSize;

                // Split the block if it’s larger than needed
                if (originalSize > totalSize)
                {
                    memoryList.insert(std::next(it), memoryBlock(-1, startAddress + totalSize, originalSize - totalSize));
                }

                newJob.mainMemoryBase = startAddress;
                newJob.instructionBase = startAddress + pcbSize;
                newJob.dataBase = newJob.instructionBase + newJob.instructionSize;
                newJob.state = 1;

                // Store PCB fields
                mainMemory[startAddress] = newJob.processID;
                mainMemory[startAddress+ 1] = newJob.state;
                mainMemory[startAddress+ 2] = newJob.programCounter;
                mainMemory[startAddress + 3] = newJob.instructionBase;
                mainMemory[startAddress + 4] = newJob.dataBase;
                mainMemory[startAddress+ 5] = newJob.memoryLimit;
                mainMemory[startAddress+ 6] = newJob.cpuCyclesUsed;
                mainMemory[startAddress+ 7] = newJob.registerValue;
                mainMemory[startAddress+ 8] = newJob.maxMemoryNeeded;
                mainMemory[startAddress + 9] = newJob.mainMemoryBase;

                // Load instructions
                int j = 0;
                for (int i = 0; i < newJob.instructionSize; i++)
                {
                    mainMemory[newJob.instructionBase + i] = newJob.logicalMemory[j];
                    if (newJob.logicalMemory[j] == 1 || newJob.logicalMemory[j] == 3)
                    {
                        j += 3;
                    }
                    else
                    {
                        j += 2;
                    }
                }

                // Load data
                j = 0;
                for (int i = newJob.dataBase; i < newJob.dataBase + newJob.maxMemoryNeeded - 1 && j < newJob.logicalMemory.size(); i++)
                {
                    if (newJob.logicalMemory[j] == 1 || newJob.logicalMemory[j] == 3)
                    {
                        mainMemory[i] = newJob.logicalMemory[j + 1];
                        mainMemory[i + 1] = newJob.logicalMemory[j + 2];
                        i++;
                        j += 3;
                    }
                    else if (newJob.logicalMemory[j] == 2 || newJob.logicalMemory[j] == 4)
                    {
                        mainMemory[i] = newJob.logicalMemory[j + 1];
                        j += 2;
                    }
                }
                readyQueue.push(newJob.mainMemoryBase);
                lastAddress = newJob.instructionBase + newJob.maxMemoryNeeded + pcbSize;
                cout << "Process " << newJob.processID << " loaded into memory at address "
                     << it->startingAddress << " with size " << newJob.maxMemoryNeeded + pcbSize << "." << endl;
                loaded = true;
                break;
            }
        }

        if (!loaded) {
            cout << "Insufficient memory for Process " << newJob.processID 
                 << ". Attempting memory coalescing." << endl;
            bool coalesced = coalesce(memoryList);

            if (coalesced && hasSufficientMemoryBlock(memoryList, newJob.maxMemoryNeeded +  pcbSize)) {
                cout << "Memory coalesced. Process " << newJob.processID << " can now be loaded."<< endl;
            } else {
                cout << "Process " << newJob.processID 
                     << " waiting in NewJobQueue due to insufficient memory." << endl;
                
                break;
            }
        }
        else {
            newJobQueue.pop();
        }
}
}
// Function to print the contents of the new job queue
// This function is used for debugging purposes
void printNewJobQueue(queue<PCB> jobQueue) {
    cout << "New Job Queue Contents:" << endl;
    cout << "Total Jobs: " << jobQueue.size() << endl;
    
    // Create a copy of the queue to avoid modifying the original
    queue<PCB> tempQueue = jobQueue;
    
    while (!tempQueue.empty()) {
        PCB job = tempQueue.front();
        tempQueue.pop();
        
        cout << "Process ID: " << job.processID << endl;
        cout << "  Memory Needed: " << job.maxMemoryNeeded << endl;
        cout << "  Instruction Size: " << job.instructionSize << endl;
        cout << "  Logical Memory Contents:" << endl;
        
        // Print logical memory contents
        for (size_t i = 0; i < job.logicalMemory.size(); ) {
            int opcode = job.logicalMemory[i];
            cout << "    Opcode: " << opcode;
            
            switch (opcode) {
                case 1: // Compute
                    cout << " (Compute): Iterations = " << job.logicalMemory[i+1] 
                         << ", Cycles = " << job.logicalMemory[i+2] << endl;
                    i += 3;
                    break;
                case 2: // Print
                    cout << " (Print): IO Cycles = " << job.logicalMemory[i+1] << endl;
                    i += 2;
                    break;
                case 3: // Store
                    cout << " (Store): Value = " << job.logicalMemory[i+1] 
                         << ", Address = " << job.logicalMemory[i+2] << endl;
                    i += 3;
                    break;
                case 4: // Load
                    cout << " (Load): Address = " << job.logicalMemory[i+1] << endl;
                    i += 2;
                    break;
                default:
                    cout << " (Unknown Opcode)" << endl;
                    i++;
                    break;
            }
        }
        cout << endl;
    }
}
// Function to execute the CPU instructions
// It updates the program counter, CPU cycles used, and register value
// It also handles I/O interrupts and memory management
// The function takes the starting address of the process in memory
// and updates the main memory, global clock, and other parameters  
void executeCPU(int startAddress, int *mainMemory, int CPUAllocated, int &globalClock,
                queue<IOWaitEntry> &ioWaitQueue, queue<int> &readyQueue, int &totalCpuTime, vector<int> &processStartTimes, list<memoryBlock> &memoryList, int maxMemory, queue<PCB> &newJobQueue)
{
    int processID = mainMemory[startAddress];
    int programCounter = mainMemory[startAddress + 2];
    // cout<< "Prog counter: " <<  programCounter << endl;
    int instructionBase = mainMemory[startAddress + 3];
    // cout<< "Instruction Base1: " <<  instructionBase << endl;
    int dataBase = mainMemory[startAddress + 4];
    int memoryLimit = mainMemory[startAddress + 5];
    int cpuCyclesUsed = mainMemory[startAddress + 6];
    int registerValue = mainMemory[startAddress + 7];

    int maxMemoryNeeded = mainMemory[startAddress + 8];
    int mainMemoryBase = mainMemory[startAddress + 9];

    if (processStartTimes[processID - 1] == -1)
    {
        processStartTimes[processID - 1] = globalClock;
    }
    mainMemory[startAddress + 1] = 2; // Running state
    int burstCycles = 0;
    int instructionSize = dataBase - instructionBase;
    int dataOffset = 0;

    for (int i = 0; i < programCounter-1; i++)
    { // Update data offset for all instructions before current program counter - 1
        if (mainMemory[instructionBase + i] == 1 || mainMemory[instructionBase + i] == 3)
        {
            dataOffset += 2;
        }
        else if (mainMemory[instructionBase + i] == 2 || mainMemory[instructionBase + i] == 4)
        {
            dataOffset += 1;
        }
    }

    while (programCounter < instructionSize && burstCycles < CPUAllocated)
    {
        int prevProgramCounter = instructionBase + programCounter - 1; // Update data offset for current program counter - 1
                                                                       // (data now ready for program counter)
        if (mainMemory[prevProgramCounter] == 1 || mainMemory[prevProgramCounter] == 3)
        {
            dataOffset += 2;
        }
        else if (mainMemory[prevProgramCounter] == 2 || mainMemory[prevProgramCounter] == 4)
        {
            dataOffset += 1;
        }

        int instruction = mainMemory[instructionBase + programCounter];
        instructionSize = dataBase - instructionBase;
        switch (instruction)
        {
        case 1:
        { // Compute: 1 iterations cycles
            int iterations = mainMemory[dataBase + dataOffset];
            int cycles = mainMemory[dataBase + dataOffset + 1];
            // cout << "instructionBase: " << instructionBase << endl;
            // cout << "Prog count: " << programCounter << endl;
            // cout << "iterations:" << iterations << endl;

            //  cout << "Cycles:" << cycles << endl;
            cout << "compute" << endl;
            cpuCyclesUsed += cycles;
            globalClock += cycles;
            burstCycles += cycles;
            programCounter += 1;
            break;
        }
        case 2:
        { // Print: 2 cycles
            int ioCycles = mainMemory[dataBase + dataOffset];
            cout << "Process " << processID << " issued an IOInterrupt and moved to the IOWaitingQueue." << endl;
            mainMemory[startAddress + 1] = 4;
            mainMemory[startAddress + 2] = programCounter + 1; // point to next

            cpuCyclesUsed += ioCycles;
            mainMemory[startAddress + 6] = cpuCyclesUsed;
            mainMemory[startAddress + 7] = registerValue;
            cpuCyclesUsed += ioCycles;
            // burstCycles += ioCycles;
            ioWaitQueue.push({startAddress, globalClock, ioCycles});
            return;
        }
        case 3:
        { // Store: 3 value address
            int value = mainMemory[dataBase + dataOffset];
            int addressOffset = mainMemory[dataBase + dataOffset + 1];

            int physicalAddress = instructionBase + addressOffset;
            registerValue = value;
            // cout << "New Register Value: " << registerValue << endl;

            if (physicalAddress >= instructionBase && physicalAddress < instructionBase + maxMemoryNeeded)
            {
                mainMemory[physicalAddress] = registerValue;
                // cout << "Value to be stored:  " << registerValue  << endl;
                // cout << "Address to be stored " << physicalAddress << endl;
                cout << "stored" << endl;
            }
            else
            {
                cout << "store error!" << endl;
            }
            cpuCyclesUsed += 1;
            globalClock += 1;
            burstCycles += 1;
            programCounter += 1;
            break;
        }
        case 4:
        { // Load: 4 address
            int addressOffset = mainMemory[dataBase + dataOffset];
            int physicalAddress = instructionBase + addressOffset;

            registerValue = mainMemory[physicalAddress];
            // cout << "New Register Value: " << registerValue << endl;

            if (physicalAddress >= instructionBase && physicalAddress < instructionBase + maxMemoryNeeded)
            {
                cout << "loaded" << endl;
            }
            else
            {
                cout << "load error!" << endl;
            }
            cpuCyclesUsed += 1;
            globalClock += 1;
            burstCycles += 1;
            programCounter += 1;
            break;
        }
        }
        if (burstCycles >= CPUAllocated && programCounter < instructionSize)
        {
            mainMemory[startAddress + 1] = 1;
            mainMemory[startAddress + 2] = programCounter;
            mainMemory[startAddress + 6] = cpuCyclesUsed;
            mainMemory[startAddress + 7] = registerValue; // Save updated registerValue
            readyQueue.push(startAddress);
            cout << "Process " << processID << " has a TimeOUT interrupt and is moved to the ReadyQueue." << endl;
            return;
        }
    }

    if (programCounter >= dataBase - instructionBase)
    {
        programCounter = instructionBase - 1;
        mainMemory[startAddress + 1] = 3;
        mainMemory[startAddress + 2] = programCounter;
        mainMemory[startAddress + 6] = cpuCyclesUsed;
        mainMemory[startAddress + 7] = registerValue; // Save updated registerValue
        int startTime = processStartTimes[processID - 1];
        int endTime = globalClock;
        totalCpuTime += cpuCyclesUsed;
        cout << "Process ID: " << processID << endl;
        cout << "State: TERMINATED" << endl;
        cout << "Program Counter: " << programCounter << endl;
        cout << "Instruction Base: " << instructionBase << endl;
        cout << "Data Base: " << dataBase << endl;
        cout << "Memory Limit: " << memoryLimit << endl;
        cout << "CPU Cycles Used: " << cpuCyclesUsed << endl;
        cout << "Register Value: " << registerValue << endl;
        cout << "Max Memory Needed: " << maxMemoryNeeded << endl;
        cout << "Main Memory Base: " << mainMemoryBase << endl;
        cout << "Total CPU Cycles Consumed: " << (endTime - processStartTimes[processID - 1]) << endl;
        cout << "Process " << processID << " terminated. Entered running state at: " << processStartTimes[processID - 1]
             << ". Terminated at: " << endTime << ". Total Execution Time: " << (endTime - processStartTimes[processID - 1]) << "." << endl;
        freeBlock(processID, memoryList, mainMemory);
        loadJobsToMemory(newJobQueue, readyQueue, mainMemory, maxMemory, memoryList);
        //printList(memoryList); // Debug output
        //printNewJobQueue(newJobQueue); // Debug output
    }
    else
    {
        mainMemory[startAddress + 1] = 1;
        mainMemory[startAddress + 2] = programCounter;
        mainMemory[startAddress + 6] = cpuCyclesUsed;
        mainMemory[startAddress + 7] = registerValue; // Save updated registerValue
        readyQueue.push(startAddress);
        cout << "Process " << processID << " has moved to Ready state." << endl;
    }
}

int main()
{
    int maxMemory, CPUAllocated, switchTime, numProcesses;
    list<memoryBlock> memoryList;
    int globalClock = 0;
    int totalCpuTime = 0;
    queue<PCB> newJobQueue;
    queue<int> readyQueue;
    queue<IOWaitEntry> ioWaitQueue;
    cin >> maxMemory >> CPUAllocated >> switchTime;
    cin >> numProcesses;
    memoryList.push_front(memoryBlock(-1, 0, maxMemory)); // Initialize memory list with a single free block of size maxMemory

    vector<int> processStartTimes(numProcesses, -1); // Initialize with -1

    int *mainMemory = new int[maxMemory];
    for (int i = 0; i < maxMemory; i++)
    {
        mainMemory[i] = -1;
    }

    for (int i = 0; i < numProcesses; i++)
    {
        PCB newJob;

        cin >> newJob.processID;
        cin >> newJob.maxMemoryNeeded;
        cin >> newJob.instructionSize;

        newJob.state = 1;
        newJob.programCounter = 0;
        newJob.cpuCyclesUsed = 0;
        newJob.registerValue = 0;
        newJob.startTime = -1;
        newJob.endTime = -1;
        newJob.memoryLimit = newJob.maxMemoryNeeded;

        newJob.logicalMemory.clear();
        for (int j = 0; j < newJob.instructionSize; j++)
        {
            int opcode;
            cin >> opcode;
            newJob.logicalMemory.push_back(opcode);
            switch (opcode)
            {
            case 1:
            {
                int iterations, cycles;
                cin >> iterations >> cycles;
                newJob.logicalMemory.push_back(iterations);
                newJob.logicalMemory.push_back(cycles);
                break;
            }
            case 2:
            {
                int cycles;
                cin >> cycles;
                newJob.logicalMemory.push_back(cycles);
                break;
            }
            case 3:
            {
                int value, address;
                cin >> value >> address;
                newJob.logicalMemory.push_back(value);
                newJob.logicalMemory.push_back(address);
                break;
            }
            case 4:
            {
                int address;
                cin >> address;
                newJob.logicalMemory.push_back(address);
                break;
            }
            }
        }
        newJobQueue.push(newJob);
    }

    loadJobsToMemory(newJobQueue, readyQueue, mainMemory, maxMemory, memoryList);

    // Memory dump
    for (int i = 0; i < maxMemory; i++)
    {
        cout << i << " : " << mainMemory[i] << endl;
    }

    while (!readyQueue.empty() || !ioWaitQueue.empty() || !newJobQueue.empty())
    {
        if (!readyQueue.empty())
        {
            globalClock += switchTime;
            int startAddress = readyQueue.front();
            readyQueue.pop();
            cout << "Process " << mainMemory[startAddress] << " has moved to Running." << endl;
            executeCPU(startAddress, mainMemory, CPUAllocated, globalClock, ioWaitQueue, readyQueue, totalCpuTime, processStartTimes, memoryList, maxMemory, newJobQueue);
            checkIOWaitingQueue(ioWaitQueue, globalClock, readyQueue, mainMemory);
        }
        else if (!ioWaitQueue.empty())
        {
            globalClock += switchTime;
            checkIOWaitingQueue(ioWaitQueue, globalClock, readyQueue, mainMemory);
        }
        else if (!newJobQueue.empty())
        {
            globalClock += switchTime;
            loadJobsToMemory(newJobQueue, readyQueue, mainMemory, maxMemory, memoryList);
        }
    }

    globalClock += switchTime;
    cout << "Total CPU time used: " << globalClock << "." << endl;

    delete[] mainMemory;
    return 0;
}
