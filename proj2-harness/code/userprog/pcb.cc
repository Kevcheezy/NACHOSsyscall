// pcb.cc
/*
 * PCB implementation
 *
 * A process control block stores all the necessary information about a process.
*/

#include "pcb.h"
#include "utility.h"
#include "openfilemanager.h"
#include "system.h"
//-----------------------------------------------------------------------------
// PCB::PCB
//
//     Constructor
//-----------------------------------------------------------------------------

PCB::PCB(int pid1, int parentPID1) : openFilesBitMap(MAX_NUM_FILES_OPEN) {

  this->pid = pid1;
  this->parentPID = parentPID1;
  this->process = NULL;
  //openFilesBitMap maintains what has been opened.
  // Account for files that are already opened, including descriptor  0 and 1
  // Child process should inherit the file descriptors openned in the parent process
  // Copy parent's openFilesBitMap
  PCB* parentPCB = processManager->getPCB(parentPID);
  BitMap parentBitMap = parentPCB->getOpenFilesBitMap();
  // Copy parent's userOpenFileList
  UserOpenFile* parentUserFileOpenList = parentPCB->getUserOpenFileList();
  unsigned int parentBit;
  for(int i = 0; i < MAX_NUM_FILES_OPEN ; i++){
    if(parentBitMap.Test(i) == TRUE){
      openFilesBitMap.Mark(1);
    }
    else{
      openFilesBitMap.Mark(0);
    }
    this->userOpenFileList[i] = parentUserFileOpenList[i];
  }
}

//-----------------------------------------------------------------------------
// PCB::~PCB
//
//     Destructor
//-----------------------------------------------------------------------------

PCB::~PCB() {}

//-----------------------------------------------------------------------------
// PCB::getPID
//
//     Returns the process ID assocated with this PCB.
//-----------------------------------------------------------------------------

int PCB::getPID() {
    return this->pid;
}

//-----------------------------------------------------------------------------
// PCB::addFile
//
//     Adds an open file to this PCB's open file list.
//-----------------------------------------------------------------------------

int PCB::addFile(UserOpenFile file) {

    int fileIndex = openFilesBitMap.Find();
    if (fileIndex == -1) {
        printf("No more room for open files.\n");
        return -1;
    } else {
        userOpenFileList[fileIndex] = file;
        return fileIndex;
    }
}

//-----------------------------------------------------------------------------
// PCB::getFile
//
//     Returns the open file associated with this PCB with the specified fileID.
//-----------------------------------------------------------------------------

UserOpenFile* PCB::getFile(int fileID) {
    
    if (openFilesBitMap.Test(fileID)) {
        return userOpenFileList + fileID;
    } else {
        return NULL;
    }
}

//-----------------------------------------------------------------------------
// PCB::removeFile
//
//     Removes the open file associated with this PCB with the specified fileID.
//-----------------------------------------------------------------------------

void PCB::removeFile(int fileID) {
    openFilesBitMap.Clear(fileID);
}


//-----------------------------------------
// PCB::getOpenFilesBitMap()
// - Returns openFilesBitMap of PCB
//----------------------------------------
BitMap PCB::getOpenFilesBitMap(){
  return this->openFilesBitMap;
}

//------------------------------------
// PCB:: getUserOpenFileList()
// - Returns userOpenFileList of PCB
//-----------------------------------
UserOpenFile* PCB::getUserOpenFileList(){
  return this->userOpenFileList;
}
