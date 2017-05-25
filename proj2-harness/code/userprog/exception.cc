// exception.cc 
//  Entry point into the Nachos kernel from user programs.
//  There are two kinds of things that can cause control to
//  transfer back to here from user code:
//
//  syscall -- The user code explicitly requests to call a procedure
//  in the Nachos kernel.  Right now, the only function we support is
//  "Halt".
//
//  exceptions -- The user code does something that the CPU can't handle.
//  For instance, accessing memory that doesn't exist, arithmetic errors,
//  etc.  
//
//  Interrupts (which can also cause control to transfer from user
//  code into the Nachos kernel) are handled elsewhere.
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.

#include "copyright.h"
#include "system.h"
#include "syscall.h"
#include "machine.h"
#include "pcb.h"

#define MAX_FILENAME_LEN 128
#define USER_READ 0 // passed as type for userReadWrite
#define USER_WRITE 1 // passed as type for userReadWrite

void IncrementPC(void);
int forkImpl(void);
void copyStateBack(int forkPC);
void yieldImpl(void);
void exitImpl(void);
int joinImpl(void);
SpaceId execImpl(char* filename);
void execHelper(void);
void readFilenameFromUsertoKernel(char* filename);
void createImpl(char* filename);
char* copyString(char* oldStr);
int openImpl(char* filename);
int userReadWrite(int virtAddr, char* buffer, int size, int type);
void writeImpl(void);
int readImpl(void);
void closeImpl(void);

//----------------------------------------------------------------------
// ExceptionHandler
//  Entry point into the Nachos kernel.  Called when a user program
//  is executing, and either does a syscall, or generates an addressing
//  or arithmetic exception.
//
//  For system calls, the following is the calling convention:
//
//  system call code -- r2
//    arg1 -- r4
//    arg2 -- r5
//    arg3 -- r6
//    arg4 -- r7
//
//  The result of the system call, if any, must be put back into r2. 
//
// And don't forget to increment the pc before returning. (Or else you'll
// loop making the same system call forever!
//
//  "which" is the kind of exception.  The list of possible exceptions 
//  are in machine.h.
//----------------------------------------------------------------------

void ExceptionHandler(ExceptionType which)
{
    int newProcessPID = 0;
    int otherProcessStatus = 0;
    int result = 0;
    // int arg1 = 0;
    //int arg2 = 0;
    //int arg3 = 0;
    char* filename = new char[MAX_FILENAME_LEN];

    int type = machine->ReadRegister(2);
    PCB* pcb = (currentThread->space)->getPCB();

    if (which == SyscallException) {

        switch (type) { /* Find out what type of syscall we're dealing with */

            case SC_Halt:
                fprintf(stderr, "System Call: %d invoked Halt\n", pcb->getPID());
                interrupt->Halt();
                break;
            case SC_Fork:
                fprintf(stderr, "System Call: %d invoked Fork\n", pcb->getPID());
                newProcessPID = forkImpl();
                machineLock->Acquire();
                machine->WriteRegister(2, newProcessPID);
                machineLock->Release();
                break;
            case SC_Yield:
                fprintf(stderr, "System Call: %d invoked Yield\n", pcb->getPID());
                yieldImpl();
                break;
            case SC_Exec:
                fprintf(stderr, "System Call: %d invoked Exec\n", pcb->getPID());
                readFilenameFromUsertoKernel(filename);
                newProcessPID = execImpl(filename);
                machineLock->Acquire();
                machine->WriteRegister(2, newProcessPID);
                machineLock->Release();
                break;
            case SC_Join:
                fprintf(stderr, "System Call: %d invoked Join\n", pcb->getPID());
                otherProcessStatus = joinImpl();
                machineLock->Acquire();
                machine->WriteRegister(2, otherProcessStatus);
                machineLock->Release();
                break;
            case SC_Exit:
                fprintf(stderr, "System Call: %d invoked Exit\n", pcb->getPID());
                exitImpl();
                break;
            case SC_Create:
                fprintf(stderr, "System Call: %d invoked Create\n", pcb->getPID());
                readFilenameFromUsertoKernel(filename);
                createImpl(filename);
                break;
            case SC_Open:
                fprintf(stderr, "System Call: [%d] invoked Open\n", pcb->getPID());
                readFilenameFromUsertoKernel(filename);
                copyString(filename); // copy bytes to a fresh piece of memory
                result = openImpl(filename); 
                machine->WriteRegister(2, result);
                break;
            case SC_Write:
                fprintf(stderr, "System Call: %d invoked Write\n", pcb->getPID());
                writeImpl();
                break;
            case SC_Read:
                fprintf(stderr, "System Call: %d invoked Read\n", pcb->getPID());
                result = readImpl();
                machine->WriteRegister(2, result);
                break;
            case SC_Close:
                fprintf(stderr, "System Call: %d invoked Close\n", pcb->getPID());
                closeImpl();
                break;
            default:
                fprintf(stderr, "System Call: %d invoked an unknown syscall!\n", 
                    pcb->getPID());
                ASSERT(FALSE);
        }
        
        IncrementPC();

    } else {
        fprintf(stderr, "Unexpected user mode exception %d %d\n", which, type);
        ASSERT(FALSE);
    }
}
//----------------------------------------------------------------------
// Fork system call implementation
//----------------------------------------------------------------------

int forkImpl() {

    // Create a new Nacho kernel thread that will execute the new user-level child process
    Thread* childThread = new Thread("user-level child process");

    int newProcessPC = machine->ReadRegister(4);

    // Find a new PID, and then construct new PCB.
    
    int newPID = processManager->getPID(); // -----(KC)-----
    int currPID = currentThread->space->getPCB()->getPID(); //---KC----
    PCB* newChildPCB = new PCB(newPID, currPID); // ----- KC ----
 
   // More hintes: Use processManager to get a new PID.

   // After finding out your own PID, call new AddrSpace() to create a new space
    // Make a copy of the address space as the child space, save its registers
   // Implement me

    AddrSpace* parentAddrSpace = processManager->getAddrSpace(currPID); // -- KC ---
    AddrSpace* childAddrSpace = new AddrSpace(parentAddrSpace, newChildPCB); // --KC---
    int childNumPages = childAddrSpace->getNumPages(); // ---KC---
    currentThread->SaveUserState(); //----KC----
    
    // Mandatory printout of the forked process
    PCB* parentPCB = currentThread->space->getPCB();
    PCB* childPCB = childThread->space->getPCB();
    fprintf(stderr, "Process %d Fork: start at address 0x%x with %d pages memory\n",
     	 currPID, newProcessPC, childNumPages);
      
    // Set up the function for the that new process will run and yield
    childThread->Fork(copyStateBack, newProcessPC);
    currentThread->Yield();
    return newPID;
}

//----------------------------------------------------------------------
// copyStateBack
//      The "dummy" function from 2.1.1 of NACHOS implementation that 
//      copies back the machine registers, PC and return registers saved
//      from before the yield was performed.
//      
//----------------------------------------------------------------------

void copyStateBack(int forkPC) {

    // Get fresh registers, but use copy of addr space
    currentThread->RestoreUserState();

    // Set the PC and run
    machineLock->Acquire();
    machine->WriteRegister(PCReg, forkPC);
    machine->WriteRegister(PrevPCReg, forkPC - 4);
    machine->WriteRegister(NextPCReg, forkPC + 4);
    machineLock->Release();

    currentThread->space->RestoreState();
    machine->Run();
    ASSERT(FALSE); // should never reach here
}

//----------------------------------------------------------------------
// Yield system call implementation.
//----------------------------------------------------------------------

void yieldImpl() {


    //Save the corresponding user process's register states.
    currentThread->SaveUserState();
    //This kernel thread yields
    currentThread->Yield();
    //Now this process is resumed for exectuion after yielding.
    //Restore the corresponding user process's states (both registers and page table)
    currentThread->RestoreUserState();
    currentThread->space->RestoreState();
    machine->Run();
   // Implement me
   // Essentially you use currentThread->Yield() to accomplish the context switch 
   // of corresponding user process. But before that, make sure you follow above comments.
   
}


//----------------------------------------------------------------------
// Exit system call implementation.
//----------------------------------------------------------------------

void exitImpl() {


    int status = machine->ReadRegister(4);
    int currPID = currentThread->space->getPCB()->getPID();
    
    fprintf(stderr, "Process %d exits with %d\n", currPID, status);
    //Implment me
    //Set the exist status in the PCB of this process   
    //Use  currentThread->space->getPCB() to find PCB. 
    PCB* temp = currentThread->space->getPCB(); //KC
    temp->status = P_BAD; //KC
    //Also let other processes  know this process  exits.
    //You can use processManager to do that.
     processManager->broadcast(currPID); //KC
    //Calling currentThread->Finish() at the end will trigger
    //the thread scheduler to find another thread. 
    //If none available, machine halting may happen.
    //Optionally you can check if I am the last process.
   
   //Clean up the space of this process
    delete currentThread->space;
    currentThread->space = NULL;
    processManager->clearPID(currPID);
    

    
    //Terminate the current Nacho thread
}
//--------------------------------------------------------
// joinImpl()
//-------------------------------------------------------
int joinImpl() {

    int otherPID = machine->ReadRegister(4);

   //Change the process state in its PCB as P_BLOCKED
    currentThread->space->getPCB()->status = P_BLOCKED;

   // If the other process has  already exited, then just return its status
   // Use proessManager to join otherPID
   // Implement me
    if( processManager->getStatus(otherPID) == 1 ){ //KC
      return processManager->getStatus(otherPID); //KC
    } // KC
    processManager->join(otherPID); //KC


   //Change the process state in its PCB as P_RUNNING
    // Implement me
    currentThread->space->getPCB()->status = P_RUNNING; //KC
    return processManager->getStatus(otherPID);
}


//----------------------------------------------------------------------
// Exec system call helper.
//----------------------------------------------------------------------

void execHelper(int uselessArg) {

    AddrSpace* currAddrSpace = currentThread->space;
    currAddrSpace->InitRegisters();
    currAddrSpace->SaveState();
    currAddrSpace->RestoreState();
    machine->Run();
}

//----------------------------------------------------------------------
// Exec system call implementation.
//----------------------------------------------------------------------

SpaceId execImpl(char* filename) {

    int currPID = currentThread->space->getPCB()->getPID();

    // Open the file
    OpenFile* fileToExecute = fileSystem->Open(filename);
    if (fileToExecute == NULL) {
        fprintf(stderr, "Unable to open file %s for execution. Terminating.\n", filename);
        return -1;
    }

    // Create a PCB for the new process, along with its running thread
    Thread* newThread = new Thread("Executing process");
    int newPID = processManager->getPID();
    PCB* pcb = new PCB(newPID, currPID);
    pcb->status = P_RUNNING;
    pcb->process = newThread;

    // Give it an address space
    AddrSpace* newSpace = new AddrSpace(fileToExecute, pcb);
    if (!newSpace->isValid()) {
        fprintf(stderr, "Exec Program: %d loading %s failed\n", currPID, filename);
        delete fileToExecute;
        delete newSpace;
        return -1;
    }
    newThread->space = newSpace;
    processManager->addProcess(pcb, newPID);

    // Close file and execute new process
    delete fileToExecute;
    fprintf(stderr, "Exec Program: %d loading %s\n", currPID, filename);
    newThread->Fork(execHelper, NULL);
    currentThread->Yield();
    return newPID;
}

//----------------------------------------------------------------------
// Helper function that brings bytes from memory to kernel space.
//----------------------------------------------------------------------

int moveBytesFromMemToKernel(int virtAddr, char* buffer, int size) {

    int physAddr = 0;
    int remainingFromPage = 0;
    int bytesCopied = 0;
    int bytesToCopy = 0;
    
    while (size > 0) {
        physAddr = currentThread->space->Translate(virtAddr);
        remainingFromPage = PageSize - physAddr % PageSize;
        bytesToCopy = remainingFromPage < size ? remainingFromPage : size;
        bcopy(machine->mainMemory + physAddr, buffer + bytesCopied, bytesToCopy);
        size -= bytesToCopy;
        bytesCopied += bytesToCopy;
        virtAddr += bytesToCopy;
    }

    return bytesCopied;
}

//----------------------------------------------------------------------
// Helper function to bring a string that represents a filename from 
// user space to kernel space.
//----------------------------------------------------------------------

void readFilenameFromUsertoKernel(char* filename) {

    int currPosition = 0;
    int filenameArg = machine->ReadRegister(4);

    do {
        moveBytesFromMemToKernel(filenameArg, filename + currPosition, 1);
        filenameArg++;
    } while (filename[currPosition++] != 0);

    filename[currPosition] = 0; // terminate C string
}

//----------------------------------------------------------------------
// Create file system call implementation.
//----------------------------------------------------------------------

void createImpl(char* filename) {
    //use fileSystem to create a file
   	// Implement me
  fileSystem->Create(filename,0);//KC
}

//----------------------------------------------------------------------
// Helper function to copy a string to a fresh piece of memory so we
// don't run into funky pointer stuff.
//----------------------------------------------------------------------

char* copyString(char* oldStr) {
    char* newStr = new char[MAX_FILENAME_LEN];
    for (int i = 0; i < MAX_FILENAME_LEN; i++) {
        newStr[i] = oldStr[i];
        if (oldStr[i] == NULL) {
            break;
        }
    }
    return newStr;
}
//----------------------------------------------------------------
// openImpl(char*filename)
//---------------------------------------------------------------
int openImpl(char* filename) {
    
    int index = 0;
    SysOpenFile* currSysFile = openFileManager->getFile(filename, index);

    if (currSysFile == NULL) { // if no process currently has it open
        OpenFile* openFile = fileSystem->Open(filename);
        if (openFile == NULL) {
            fprintf(stderr, "Unable to open the file %s\n", filename);
            return -1;
        }


        SysOpenFile currSysFile;

       // Setup this SysOpenFile data structure
       //Need to set currSysFile.file (pointing to OpenFile data structure)
       // also currSysFile.numProcessAccessing
      // and currSysFile.filename  
   	// Implement me
	currSysFile.file = openFile; // KC
	currSysFile.numProcessesAccessing = 0; // KC
	currSysFile.filename = filename; // KC
	
        index = openFileManager->addFile(currSysFile);
    }
    else { // the file is already open by another process
        currSysFile->numProcessesAccessing++;
    }

    // Either way, add it to the current PCB's open file list
    UserOpenFile currUserFile;
    
   //Set up this UserOpenFile data structure
   // currUserFile.indeInSysOpenFileList should point to the index from openFileManager.
   // currUserFile.currOffsetInFile is the offset position of the current file openned
   // Implement me
    //currUserFile.fileName = filename; // KC (necessary?)
    currUserFile.indexInSysOpenFileList = index; // KC
    currUserFile.currOffsetInFile = currSysFile->file->Length(); // KC (Length?)
    int currFileID = currentThread->space->getPCB()->addFile(currUserFile);//given
    return currFileID;
}


//----------------------------------------------------------------------
// Helper function that either reads from main mem into a buffer or 
// writes from a buffer into main mem.
//----------------------------------------------------------------------


int userReadWrite(int virtAddr, char* buffer, int size, int type) {

    int physAddr = 0;
    int numBytesFromPSLeft = 0;
    int numBytesCopied = 0;
    int numBytesToCopy = 0;

    if (type == USER_READ) { // Read and copy data from the system buffer to the user space in main memory
        while (size > 0) {
            //Translate the virtual address to phyiscal address physAddr 
           //Just one line of code
            //Implement me
	  physAddr = currentThread->space->Translate(virtAddr); //KC
            numBytesFromPSLeft = PageSize - physAddr % PageSize;
            numBytesToCopy = (numBytesFromPSLeft < size) ? numBytesFromPSLeft : size;
            bcopy(buffer + numBytesCopied, machine->mainMemory + physAddr, numBytesToCopy);
            numBytesCopied += numBytesToCopy;
            size -= numBytesToCopy;
            virtAddr += numBytesToCopy;
        }
    }
    else if (type == USER_WRITE) { // Copy data from the user's main memory to the system buffer
        while (size > 0) {
            //Translate the virtual address to phyiscal address physAddr 
           //Just one line of code
            //Implement me
	  physAddr = currentThread->space->Translate(virtAddr); //KC
            numBytesFromPSLeft = PageSize - physAddr % PageSize;
            numBytesToCopy = (numBytesFromPSLeft < size) ? numBytesFromPSLeft : size;
            bcopy(machine->mainMemory + physAddr, buffer + numBytesCopied, numBytesToCopy);
            numBytesCopied += numBytesToCopy;
            size -= numBytesToCopy;
            virtAddr += numBytesToCopy;
        }
    }
    else {
        //printf ("Invalid type passed in.\n");
        ASSERT(FALSE);
    }
    return numBytesCopied; 
}

//----------------------------------------------------------------------
// Write file system call implementation
//----------------------------------------------------------------------
void writeImpl() {
    
    int writeAddr = machine->ReadRegister(4);
    int size = machine->ReadRegister(5);
    int fileID = machine->ReadRegister(6);

    char* buffer = new char[size + 1];
    if (fileID == ConsoleOutput) {
        userReadWrite(writeAddr, buffer, size, USER_WRITE);
        buffer[size] = 0; // always terminate
        //printf("%s", buffer);
        openFileManager->consoleWriteLock->Acquire();
        for (int i = 0; i < size; ++i)
            UserConsolePutChar(buffer[i]);
        openFileManager->consoleWriteLock->Release();
    }
    else {
        //allocate some buffer space
        //Fetch data from the user space to this system buffer using  userReadWrite().
        //Just two line of code
        //Implement me
      userReadWrite(writeAddr, buffer, size, USER_WRITE);//KC
      UserOpenFile* userFile = currentThread->space->getPCB()->getFile(fileID); //given

	
	//Use openFileManager->getFile method  to find the openned file structure (SysOpenFile)
	//Use SysOpenFile->file's writeAt() to write out the above buffer with size listed.
	//Increment the current offset  by the actual number of bytes written.
        //Just 3 lines of code
        //Implement me
      SysOpenFile* sysFile = openFileManager->getFile(userFile->indexInSysOpenFileList); //GR
      sysFile->file->WriteAt(buffer, size, userFile->currOffsetInFile); // GR
      userFile->currOffsetInFile += size; //GR
    }
    delete [] buffer;
}
//----------------------------------------------------------------------
// Read file system call implementation.
//----------------------------------------------------------------------
int readImpl() {

    int readAddr = machine->ReadRegister(4);
    int size = machine->ReadRegister(5);
    int fileID = machine->ReadRegister(6);
    char* buffer = new char[size + 1];
    int numActualBytesRead = size;

    if (fileID == ConsoleInput) {//Read data from the console to the system buffer
        int bytesRead = 0;
        while (bytesRead < size) {
            //buffer[bytesRead] = getchar();
            buffer[bytesRead] = UserConsoleGetChar();
            bytesRead++;
        }
    }
    else {//Read data from the file to the system buffer
      // userReadWrite(readAddr, buffer, size, USER_READ); // KC
      UserOpenFile* userFile = currentThread->space->getPCB()->getFile(fileID); //given
	//Now from openFileManger, find the SystemOpenFile data structure for this userFile.
      
      SysOpenFile* sysFile = openFileManager->getFile(userFile->indexInSysOpenFileList); //KC
 
	//Use ReadAt() to read the file at selected offset to this system buffer buffer[]
      sysFile->file->ReadAt(buffer, size, userFile->currOffsetInFile); //KC
	// Adust the offset in userFile to reflect my current position.
      userFile->currOffsetInFile += size; //KC
       // The above few lines of code are very similar to ones in writeImpl()
	// Implement me
        
    }
    //Now copy data from the system buffer to the targted main memory space using userReadWrite()
    //Implement me
    userReadWrite(readAddr, buffer,  size, USER_READ); // KC
    delete [] buffer;
    return numActualBytesRead;
}

//----------------------------------------------------------------------
// Close file system call implementation.
//----------------------------------------------------------------------

void closeImpl() {

    int fileID = machine->ReadRegister(4);

    // Find the UserOpenFile data structure of this file openned in PCB based on this fileID descriptor
    // Use openFileManager's getFile method to get a pointer to the system-wide SysOpenFile  data structure
    // Call the close method in SysOpenFile 
    // Remove the file  in the open file list of this process PCB.
    // Implement me
    UserOpenFile* userFile = currentThread->space->getPCB()->getFile(fileID); //KC
    SysOpenFile* sysOpenFilePtr = openFileManager->getFile(userFile->fileName,
							   ( userFile->indexInSysOpenFileList));// KC
    sysOpenFilePtr->closedBySingleProcess(); // KC
    currentThread->space->getPCB()->removeFile(fileID); // KC
}

//----------------------------------------------------------------------
// IncrementPC
//      Helper function used to increment the values of PCreg, NextPCreg,
//      and PrevPCreg after a syscall.
//----------------------------------------------------------------------

void IncrementPC(void) {

    int currentReg = machine->ReadRegister(PCReg);
    int nextReg = machine->ReadRegister(NextPCReg);
    int prevReg = machine->ReadRegister(PrevPCReg);

    currentReg = currentReg + 4;
    nextReg = nextReg + 4;
    prevReg = prevReg + 4;

    machineLock->Acquire();
    machine->WriteRegister(PCReg, currentReg);
    machine->WriteRegister(NextPCReg, nextReg);
    machine->WriteRegister(PrevPCReg, prevReg);
    machineLock->Release();
}
