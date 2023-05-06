/*
 * fileSystem.c
 *
 *  Modified on: 
 *      Author: csun613
 * 
 * Complete this file.
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include "device.h"
#include <libgen.h>

typedef struct {
    int blockNum; // 2 bytes
    int address;  // 1 bytes
} blockPointer;

typedef struct {
    int freeBlock;  // 2 bytes
    blockPointer head;       // 3 bytes
    blockPointer current;
    int currentSize;
} sysinfo;

typedef struct {
    unsigned char name[8]; // 8
    blockPointer next; // 3
    blockPointer content; // 3
    blockPointer parent; // 3
    int size;
} dir;

typedef struct {
    unsigned char name[8];   // 7 bytes
    blockPointer next; // 3
    int size;        // 2 bytes
    unsigned char content;   // variable size (n - 13 bytes)
} file;

int print_block(block b) {
    int row;
    int col;
    const int number_per_row = 16;
    unsigned char *byte = b;
    for (row = 0; row < BLOCK_SIZE / number_per_row; ++row) {
        unsigned char *byte2 = byte;
        for (col = 0; col < number_per_row; ++col) {
            printf("%02x ", *byte++);
        }
        printf("\t");
        for (col = 0; col < number_per_row; ++col) {
            unsigned char c = *byte2++;
            if (c < 32 || c > 126)
                c = '-';
            printf("%c", c);
        }
        printf("\n");
    }
    return 0;
}

int print_pointer(blockPointer b) {
    printf("Block Number: %d\n", b.blockNum);
    printf("Address: %d\n", b.address);
    return 0;
}

int print_char(unsigned char *data, int len) {
    for( int i = 0; i < len; i++) {
        printf("%02X ", data[i]);
    }
    printf("\n");
    return 0;
}

int fromHex(unsigned char *hex, int arraySize) {
    if (hex == NULL) {
        return -1;
    }

    if (arraySize == 1) {
        return (int) hex[0];
    }
    else {
        return (int)((hex[0] << 8) | hex[1]);
    }
}

int toHex(int n, unsigned char *hexArray, int arraySize) {
    if (hexArray == NULL) {
        return -1;
    }

    if (arraySize == 2) {
        hexArray[0] = (n >> 8) & 0xff;
        hexArray[1] = n & 0xff;
    }
    else {
        hexArray[0] = n & 0xff;
    }

    return 0;
}

int editBlock(int blockNum, unsigned char *data, int start, int size) {
    unsigned char cblock[64];
    blockRead(blockNum, cblock);
    memcpy(&cblock[start], data, sizeof(unsigned char) * size);
    blockWrite(blockNum, cblock);
    return 0;
}

int readBlock(int blockNum, unsigned char *data, int start, int size) {
    unsigned char cblock[64];
    blockRead(blockNum, cblock);
    memcpy(data, &cblock[start], sizeof(unsigned char) * size);
    return 0;
}

int createRawPointer(int block, int address, unsigned char *data) {
    unsigned char bchar[2] = {"\0"};
    unsigned char achar[1] = {"\0"};

    toHex(block, bchar, 2);
    toHex(address, achar, 1);

    memcpy(&data[0], bchar, sizeof(unsigned char) * 2);
    memcpy(&data[2], achar, sizeof(unsigned char) * 1);

    return 0;
}

int convertRawPointer(unsigned char *data, blockPointer *pointer) {
    unsigned char block[2];
    unsigned char address[1];

    memcpy(block, &data[0], sizeof(unsigned char) * 2);
    memcpy(address, &data[2], sizeof(unsigned char) * 1);

    pointer->address = fromHex(address, sizeof(address));
    pointer->blockNum = fromHex(block, sizeof(block));
    return 0;
}

int createPointer(int block, int address, blockPointer *pointer) {
    unsigned char point[3];
    createRawPointer(block, address, point);
    convertRawPointer(point, pointer);
    return 0;
}

bool pointerEquals(blockPointer pointer1, blockPointer pointer2) {
    if (pointer1.blockNum == pointer2.blockNum && pointer1.address == pointer2.address) {
        return true;
    }
    return false;
}

int loadSysinfo(sysinfo *system) {
    unsigned char freeBlock[2];
    unsigned char h[3];
    unsigned char c[3];
    unsigned char s[2];

    readBlock(0, freeBlock, 0, 2);
    readBlock(0, h, 2, 3);
    readBlock(0, c, 5, 3);
    readBlock(0, s, 8, 2);
    blockPointer head;
    blockPointer current;

    convertRawPointer(h, &head);
    convertRawPointer(c, &current);

    system->freeBlock = fromHex(freeBlock, sizeof(freeBlock));
    system->head = head;
    system->current = current;
    system->currentSize = fromHex(s, sizeof(s));

    return 0;
}

int incrementFree() {
    sysinfo system;
    loadSysinfo(&system);

    system.freeBlock += 1;

    unsigned char freeHex[2];
    toHex(system.freeBlock, freeHex, 2);
    editBlock(0, freeHex, 0, 2);
    return 0;
}

int initRoot() {
    unsigned char data[19];
    unsigned char name[8] = "\0";
    unsigned char next[3];
    unsigned char content[3];
    unsigned char parent[3];
    unsigned char size[2];

    toHex(0, size, 2);
    createRawPointer(2, 19, next);
    createRawPointer(0, 0, content);
    createRawPointer(0, 0, parent);

    memcpy(&data[0], name, sizeof(unsigned char) * 8);
    memcpy(&data[8], next, sizeof(unsigned char) * 3);
    memcpy(&data[11], content, sizeof(unsigned char) * 3);
    memcpy(&data[14], parent, sizeof(unsigned char) * 3);
    memcpy(&data[17], size, sizeof(unsigned char) * 2);

    editBlock(2, data, 0, 19);
    return 0;
}

int allocDir(blockPointer current, dir *direc) {
    sysinfo system;
    loadSysinfo(&system);
    unsigned char content[3]= {'\x0'};

    blockPointer pointer;
    createRawPointer(system.freeBlock, 0, &content);
    convertRawPointer(content, &pointer);
    incrementFree();

    direc->content = pointer;

    editBlock(current.blockNum, content, current.address + 11, 3);
    return 0;
}

int incrementSizeDir(blockPointer current, dir *direc) {
    unsigned char rsize[2] = {'\x0'};
    int size;

    size = direc->size + 64;
    direc->size = size;
    toHex(size, rsize, 2);

    editBlock(current.blockNum, rsize, current.address + 17, 2);
    return 0;
}

int loadDir(blockPointer pointer, dir *direc) {
    unsigned char data[19] = {'\x0'};
    readBlock(pointer.blockNum, data, pointer.address, 19);
    unsigned char rnext[3] = {'\x0'};
    unsigned char rcontent[3] = {'\x0'};
    unsigned char rparent[3] = {'\x0'};
    unsigned char rsize[2] = {'\x0'};

    blockPointer next;
    blockPointer content;
    blockPointer parent;

    memcpy(direc->name, &data[0], sizeof(unsigned char) * 8);
    memcpy(rnext, &data[8], sizeof(unsigned char) * 3);
    memcpy(rcontent, &data[11], sizeof(unsigned char) * 3);
    memcpy(rparent, &data[14], sizeof(unsigned char) * 3);
    memcpy(rsize, &data[17], sizeof(unsigned char) * 2);

    convertRawPointer(rnext, &next);
    convertRawPointer(rcontent, &content);
    convertRawPointer(rparent, &parent);

    direc->next = next;
    direc->content = content;
    direc->parent = parent;
    direc->size = fromHex(rsize, 2);

    if (strlen(direc->name) == 0) {
        return -1;
    }
    return 0;
}

int initDir(blockPointer *current, unsigned char *name, blockPointer parent) {
    sysinfo system;
    loadSysinfo(&system);

    dir direc;
    loadDir(*current, &direc);
    while (strlen(direc.name) != 0) {
        if (strcmp(direc.name, name) == 0 && pointerEquals(parent, direc.parent)) {
            return 1;
        }
        *current = direc.next;
        loadDir(direc.next, &direc);
    }

    unsigned char data[19] = {'\x0'};
    unsigned char rname[8] = {'\x0'};
    unsigned char rnext[3] = {'\x0'};
    unsigned char rcontent[3] = {'\x0'};
    unsigned char rparent[3] = {'\x0'};
    unsigned char rsize[2] = {'\x0'};

    toHex(0, rsize, 2);
    createRawPointer(0, 0, rnext);
    createRawPointer(0, 0, rcontent);
    createRawPointer(parent.blockNum, parent.address, rparent);

    memcpy(rname, name, sizeof(unsigned char) * 8);
    memcpy(&data[0], rname, sizeof(unsigned char) * 8);
    memcpy(&data[11], rcontent, sizeof(unsigned char) * 3);
    memcpy(&data[14], rparent, sizeof(unsigned char) * 3);
    memcpy(&data[17], rsize, sizeof(unsigned char) * 2);

    if (current->address + 19 < 57) {
        createRawPointer(current->blockNum, current->address + 19, rnext);
        memcpy(&data[8], rnext, sizeof(unsigned char) * 3);
    } else {
        createRawPointer(system.freeBlock, 0, rnext);
        memcpy(&data[8], rnext, sizeof(unsigned char) * 3);
        incrementFree();
    }

    editBlock(current->blockNum, data, current->address, 19);
    return 0;
}

int loadFile(blockPointer pointer, file *cfile) {

    unsigned char data[13] = {'\x0'};
    readBlock(pointer.blockNum, data, pointer.address, 13);
    unsigned char rnext[3] = {'\x0'};
    unsigned char rsize[2] = {'\x0'};

    blockPointer next;

    memcpy(cfile->name, &data[0], sizeof(unsigned char) * 8);
    memcpy(rnext, &data[8], sizeof(unsigned char) * 3);
    memcpy(rsize, &data[11], sizeof(unsigned char) * 2);

    convertRawPointer(rnext, &next);

    cfile->next = next;
    cfile->size = fromHex(rsize, 2);

    return 0;
}

int initFile(blockPointer current, unsigned char *name) {
    sysinfo system;
    loadSysinfo(&system);

    file cfile;
    blockPointer def;
    unsigned char rnext[3] = {'\x0'};
    createPointer(0, 0, &def);
    loadFile(current, &cfile);

    while (strlen(cfile.name) != 0) {
        if (strcmp(cfile.name, name) == 0) {
            return 1;
        }
        if (pointerEquals(cfile.next, def)) {
            createRawPointer(system.freeBlock, 0, rnext);
            editBlock(current.blockNum, rnext, current.address + 8, 3);
            convertRawPointer(rnext, &current);
            incrementFree();
            break;
        } else {
            current = cfile.next;
        }
        loadFile(current, &cfile);
    }

    unsigned char data[13] = {'\x0'};
    unsigned char next[3] = {'\x0'};
    unsigned char size[2] = {'\x0'};

    createRawPointer(0, 0, next);
    toHex(0, size, 2);

    memcpy(&data[0], name, sizeof(unsigned char) * 8);
    memcpy(&data[8], next, sizeof(unsigned char) * 3);
    memcpy(&data[11], size, sizeof(unsigned char) * 2);

    editBlock(current.blockNum, data, current.address, 13);
    return 0;
}

int systemReadUpdate(blockPointer current, int size) {
    unsigned char rcurrent[3];
    unsigned char rsize[2];

    createRawPointer(current.blockNum, current.address, rcurrent);
    toHex(size, rsize, 2);

    editBlock(0, rcurrent, 5, 3);
    editBlock(0, rsize, 8, 2);
}

int readFile(blockPointer start, unsigned char *data, int fileEnd, int length) {
    unsigned char rpointer[2];
    int writable = 62 - start.address;
    int startRead = start.address;
    int written = 0;
    unsigned char result[length];
    blockPointer pointer = start;


    while (length > 0) {
        if (writable > length) {
            writable = length;
            length = 0;
        }
        unsigned char current[writable];
        readBlock(pointer.blockNum, current, startRead, writable);
        memcpy(&result[written], current, writable);
        readBlock(start.blockNum, rpointer, 62, 2);
        if (fromHex(rpointer, 2) != 0) {
            createPointer(fromHex(rpointer, 2), 0, &pointer);
            startRead = 0;
        }
        written += writable;
        length -= writable;
        fileEnd -= writable;
        writable = 62;
    }

    memcpy(data, result, written);

    if (fileEnd > 0) {
        createPointer(pointer.blockNum, startRead + written, &pointer);
        systemReadUpdate(pointer, fileEnd);
    } else {
        createPointer(0, 0, &pointer);
        systemReadUpdate(pointer, 0);
    }
    return 0;
}

int writeFile(blockPointer start, unsigned char *data, file *cfile, int length) {
    unsigned char bsize[2];
    int written = 0;
    int currentWritable;
    int fileStart = cfile->size;
    unsigned char rpointer[2];
    blockPointer pointer;
    readBlock(start.blockNum, rpointer, 62, 2);
    if (fromHex(rpointer, 2) != 0) {
        fileStart -= 49;
        createPointer(fromHex(rpointer, 2), 0, &pointer);
        readBlock(pointer.blockNum, rpointer, 62, 2);
    } else {
        pointer = start;
        fileStart += 13;
    }

    while (fromHex(rpointer, 2) != 0) {
        fileStart -= 62;
        createPointer(fromHex(rpointer, 2), 0, &pointer);
        readBlock(pointer.blockNum, rpointer, 62, 2);
    }

    currentWritable = 62 - fileStart;

    while (length > 0) {
        if (currentWritable > length) {
            currentWritable = length;
        }
        unsigned char writableChunk[currentWritable];
        memcpy(writableChunk, &data[written], currentWritable);
        editBlock(pointer.blockNum, writableChunk, fileStart, currentWritable);
        cfile->size += currentWritable;
        toHex(cfile->size, bsize, 2);
        editBlock(start.blockNum, bsize, 11, 2);
        length -= currentWritable;
        written += currentWritable;
        fileStart = 0;
        if (length > 0) {
            currentWritable = 62;
            sysinfo system;
            loadSysinfo(&system);
            unsigned char free[2];
            toHex(system.freeBlock, free, 2);
            editBlock(pointer.blockNum, free, 62, 2);
            createPointer(system.freeBlock, 0, &pointer);
            incrementFree();
        }
    }
    return 0;
}

int split_string(char *str, char *delim, char ***paths, int *numPaths) {
    *paths = (char **)malloc(1024 * sizeof(char *));

    int len = strlen(str);
    char *ptr = str;
    char *end = str + len;
    int i = 0;

    while (ptr < end) {
        char *path_end = strstr(ptr, delim);
        if (path_end == NULL) {
            path_end = end;
        }
        int path_len = path_end - ptr;

        (*paths)[i] = (char *)malloc((path_len + 1) * sizeof(char));

        strncpy((*paths)[i], ptr, path_len);

        (*paths)[i][path_len] = '\0';
		
        ptr = path_end + strlen(delim);
        i++;
    }

    *numPaths = i;
    return 0;
}

/* The file system error number. */
int file_errno = 0;

/*
 * Formats the device for use by this file system.
 * The volume name must be < 64 bytes long.
 * All information previously on the device is lost.
 * Also creates the root directory "/".
 * Returns 0 if no problem or -1 if the call failed.
 */
int format(char *volumeName) {
    int blocks = numBlocks();

    // Normal Block
    unsigned char cblock[64] = {'\x0'};

    unsigned char head[3];
    unsigned char current[3];
    unsigned char freeBlock[2];

    // Normal Block Init
    for (int i = 0; i < blocks; i++) {
        editBlock(i, cblock, 0, sizeof(cblock));
    }

    // System Block Init
    toHex(3, freeBlock, 2);
    editBlock(0, freeBlock, 0, sizeof(freeBlock));

    createRawPointer(2, 0, head);
    editBlock(0, head, 2, sizeof(head));

    createRawPointer(0, 0, current);
    editBlock(0, current, 5, sizeof(current));

    toHex(0, freeBlock, 2);
    editBlock(0, freeBlock, 8, sizeof(freeBlock));

    editBlock(1, (unsigned char*)volumeName, 0, strlen(volumeName));

    initRoot();
    return 0;
}

/*
 * Returns the volume's name in the result.
 * Returns 0 if no problem or -1 if the call failed.
 */
int volumeName(char *result) {
	readBlock(1, result, 0, 64);
	return 0;
}

/*
 * Makes a file with a fully qualified pathname starting with "/".
 * It automatically creates all intervening directories.
 * Pathnames can consist of any printable ASCII characters (0x20 - 0x7e)
 * including the space character.
 * The occurrence of "/" is interpreted as starting a new directory
 * (or the file name).
 * Each section of the pathname must be between 1 and 7 bytes long (not
 * counting the "/"s).
 * The pathname cannot finish with a "/" so the only way to create a directory
 * is to create a file in that directory. This is not true for the root
 * directory "/" as this needs to be created when format is called.
 * The total length of a pathname is limited only by the size of the device.
 * Returns 0 if no problem or -1 if the call failed.
 */
int create(char *pathName) {
    sysinfo system;
    loadSysinfo(&system);

    blockPointer current;
    blockPointer previous;
    blockPointer def;
    dir currentDir;

    createPointer(0, 0, &def);
    current = system.head;
    previous = def;
    loadDir(current, &currentDir);

    char *delim = "/";
    char **paths;
    int numPaths;

    if (split_string(pathName, delim, &paths, &numPaths) == 0) {
        for (int i = 1; i < numPaths; i++) {
            unsigned char cpath[8] = {'\0'};
            memcpy(cpath, paths[i], 8);

            if (i == numPaths - 1) {
                if (pointerEquals(currentDir.content, def)) {
                    allocDir(current, &currentDir);
                    incrementSizeDir(current, &currentDir);
                }
                loadDir(current, &currentDir);
                initFile(currentDir.content, cpath);
            } else {
                previous = current;
                current = currentDir.next;
                initDir(&current, cpath, previous);
                loadDir(current, &currentDir);
            }
            free(paths[i]);
        }
        free(paths);
    }
    return 0;
}



/*
 * Returns a list of all files in the named directory.
 * The "result" string is filled in with the output.
 * The result looks like this

/dir1:
file1	42
file2	0

 * The fully qualified pathname of the directory followed by a colon and
 * then each file name followed by a tab "\t" and then its file size.
 * Each file on a separate line.
 * The directoryName is a full pathname.
 */
void list(char *result, char *directoryName) {
    sysinfo system;
    loadSysinfo(&system);

    blockPointer current;
    blockPointer previous;
    blockPointer def;
    dir checkDir;
    dir currentDir;
    file currentFile;

    createPointer(0, 0, &def);
    current = system.head;
    previous = def;
    loadDir(current, &currentDir);

    char *delim = "/";
    char **paths;
    int numPaths;

    if (split_string(directoryName, delim, &paths, &numPaths) == 0) {
        for (int i = 0; i < numPaths; i++) {
            printf("Current path is: /%s:\n", paths[i]);
            if (i == numPaths - 1) {
                loadDir(current, &currentDir);

                sprintf(result, "/%s:\n", currentDir.name);
                loadFile(currentDir.content, &currentFile);

                if (strlen(currentFile.name) != 0) {
                    sprintf(result + strlen(result), "%s:\t%d\n",currentFile.name, currentFile.size);
                }

                while(!pointerEquals(currentFile.next, def)) {
                    loadFile(currentFile.next, &currentFile);
                    sprintf(result + strlen(result), "%s:\t%d\n",currentFile.name, currentFile.size);
                }

                loadDir(currentDir.next, &checkDir);
                while(strlen(checkDir.name) != 0) {
                    sprintf(result, "/%s:\n", currentDir.name);
                    loadDir(currentDir.next, &currentDir);
                    loadDir(currentDir.next, &checkDir);
                    //sprintf(result + strlen(result), "%s:\t%d\n",currentFile.name, currentFile.size);
                }
            } else {
                if(!pointerEquals(currentDir.next, def)) {
                    previous = current;
                    current = currentDir.next;
                    loadDir(current, &currentDir);
                }
            }
            free(paths[i]);
        }
        free(paths);
    }
}

/*
 * Writes data onto the end of the file.
 * Copies "length" bytes from data and appends them to the file.
 * The filename is a full pathname.
 * The file must have been created before this call is made.
 * Returns 0 if no problem or -1 if the call failed.
 */
int a2write(char *fileName, void *data, int length) {
    sysinfo system;
    loadSysinfo(&system);

    blockPointer current;
    blockPointer previous;
    blockPointer def;
    dir currentDir;
    dir previousDir;
    file currentFile;

    createPointer(0, 0, &def);
    current = system.head;
    previous = def;
    loadDir(current, &currentDir);


    char *delim = "/";
    char **paths;
    int numPaths;
    int currentWritable;

    if (split_string(fileName, delim, &paths, &numPaths) == 0) {
        for (int i = 0; i < numPaths; i++) {
            if (i == numPaths - 1) {
                current = currentDir.content;
                loadFile(current, &currentFile);
                while (strcmp(currentFile.name, paths[i]) != 0) {
                    if (pointerEquals(currentFile.next, def)){
                        printf("error!");
                    }
                    current = currentFile.next;
                    loadFile(current, &currentFile);
                }
                writeFile(current, data, &currentFile, length);

            } else {
                while (strcmp(currentDir.name, paths[i]) != 0 || !pointerEquals(currentDir.parent, previous)) {
                    current = currentDir.next;
                    loadDir(current, &currentDir);
                }
                previous = current;
            }
            free(paths[i]);
        }
        free(paths);
    }
}


/*
 * Reads data from the start of the file.
 * Maintains a file position so that subsequent reads continue
 * from where the last read finished.
 * The filename is a full pathname.
 * The file must have been created before this call is made.
 * Returns 0 if no problem or -1 if the call failed.
 */
int a2read(char *fileName, void *data, int length) {
    sysinfo system;
    loadSysinfo(&system);

    blockPointer current;
    blockPointer previous;
    blockPointer def;
    dir currentDir;
    dir previousDir;
    file currentFile;

    createPointer(0, 0, &def);
    current = system.head;
    previous = def;
    loadDir(current, &currentDir);

    char *delim = "/";
    char **paths;
    int numPaths;
    int currentWritable;

    if (split_string(fileName, delim, &paths, &numPaths) == 0) {
        for (int i = 0; i < numPaths; i++) {
            if (i == numPaths - 1) {
                current = currentDir.content;
                loadFile(current, &currentFile);
                while (strcmp(currentFile.name, paths[i]) != 0) {
                    if (pointerEquals(currentFile.next, def)){
                        printf("error!");
                    }
                    current = currentFile.next;
                    loadFile(current, &currentFile);
                }
                if (!pointerEquals(system.current, def)) {
                    readFile(system.current, data, system.currentSize, length);
                    return 0;
                }
                blockPointer start;
                createPointer(current.blockNum, current.address + 13, &start);
                readFile(start, data, currentFile.size, length);
            } else {
                while (strcmp(currentDir.name, paths[i]) != 0 || !pointerEquals(currentDir.parent, previous)) {
                    current = currentDir.next;
                    loadDir(current, &currentDir);
                }
                previous = current;
            }
            free(paths[i]);
        }
        free(paths);
    }
    return 0;
}

/*
 * Repositions the file pointer for the file at the specified location.
 * All positive integers are byte offsets from the start of the file.
 * 0 is the beginning of the file.
 * If the location is after the end of the file, move the location
 * pointer to the end of the file.
 * The filename is a full pathname.
 * The file must have been created before this call is made.
 * Returns 0 if no problem or -1 if the call failed.
 */
int seek(char *fileName, int location) {
	return -1;
}
