/*
 * fileSystem.c
 *
 *  Modified on: 
 *      Author: csun613
 * 
 * Complete this file.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "device.h"
#include "fileSystem.h"

typedef struct {
    int blockNum; // 2 bytes
    int address;  // 1 bytes
} blockPointer;

typedef struct {
   	int freeBlock;  // 2 bytes
    blockPointer head;       // 3 bytes
	blockPointer current;
} sysinfo;

typedef struct {
    unsigned char name[8]; // 8
    blockPointer next; // 3
	blockPointer content; // 3
	blockPointer parent; // 3
} dir;

typedef struct {
    unsigned char name[8];   // 7 bytes
	blockPointer next; // 3
    int size;        // 2 bytes
    unsigned char content;   // variable size (n - 9 bytes)
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


int initSysinfo(sysinfo *system) {
	unsigned char freeBlock[2];
	unsigned char h[3];
	unsigned char c[3];

	readBlock(0, freeBlock, 0, 2);
	readBlock(0, h, 2, 3);
	readBlock(0, c, 5, 3);

	blockPointer head;
	blockPointer current;

	convertRawPointer(h, &head);
	convertRawPointer(c, &current);

	system->freeBlock = fromHex(freeBlock, sizeof(freeBlock));
	system->head = head;
	system->current = current;

	return 0;
}

int incrementFree() {
	sysinfo system;
	initSysinfo(&system);

	system.freeBlock += 1;

	unsigned char freeHex[2];
	toHex(system.freeBlock, freeHex, 2);
	editBlock(0, freeHex, 0, 2);
	return 0;
}


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

	createRawPointer(2, 0, current);
	editBlock(0, current, 5, sizeof(current));

	editBlock(1, (unsigned char*)volumeName, 0, strlen(volumeName));

	initRoot();
	return 0;
}

int initRoot() {
	unsigned char data[17];
    unsigned char name[8] = "/";
    unsigned char next[3];
	unsigned char content[3];
	unsigned char parent[3];

	createRawPointer(2, 17, next);
	createRawPointer(0, 0, content);
	createRawPointer(0, 0, parent);

	memcpy(&data[0], name, sizeof(unsigned char) * 8);
	memcpy(&data[8], next, sizeof(unsigned char) * 3);
	memcpy(&data[11], content, sizeof(unsigned char) * 3);
	memcpy(&data[14], parent, sizeof(unsigned char) * 3);

	editBlock(2, data, 0, 17);
	return 0;
}

int allocDir(dir *direc) {
	sysinfo system;
	initSysinfo(&system);

	blockPointer pointer;
	
	createPointer(system.freeBlock, 0, &pointer);
	direc->content = pointer;
}

int loadDir(blockPointer pointer, dir *direc) {
	unsigned char data[17];
	print_pointer(pointer);
	readBlock(pointer.blockNum, data, pointer.address, 17);
	unsigned char name[8];
	unsigned char rnext[3];
	unsigned char rcontent[3];
	unsigned char rparent[3];

	blockPointer next;
	blockPointer content;
	blockPointer parent;

	memcpy(direc->name, &data[0], sizeof(unsigned char) * 8);
	memcpy(rnext, &data[8], sizeof(unsigned char) * 3);
	memcpy(rcontent, &data[11], sizeof(unsigned char) * 3);
	memcpy(rparent, &data[14], sizeof(unsigned char) * 3);

	convertRawPointer(rnext, &next);
	convertRawPointer(rcontent, &content);
	convertRawPointer(rparent, &parent);

	direc->next = next;
	direc->content = content;
	direc->parent = parent;
}

int updateCurrent(blockPointer pointer) {
	sysinfo system;
	initSysinfo(&system);
	unsigned char location[3];
	createRawPointer(pointer.blockNum, pointer.address, location);
	editBlock(5, location, 0, 3);
	return 0;
}

bool checkDir(unsigned char *dirName, blockPointer parent, dir currentDir) {
	if (dirName == currentDir.name && pointerEquals(parent, currentDir.parent)) {
		return true;
	}
	return false;
}


int initDir(blockPointer current, unsigned char *name, blockPointer parent) {
	sysinfo system;
	initSysinfo(&system);

	unsigned char data[17];
	unsigned char rnext[3];
	unsigned char rcontent[3];
	unsigned char rparent[3];

	createRawPointer(0, 0, rcontent);
	createRawPointer(parent.blockNum, parent.address, rparent);

	memcpy(&data[0], name, sizeof(unsigned char) * 8);
	memcpy(&data[11], rcontent, sizeof(unsigned char) * 3);
	memcpy(&data[14], rparent, sizeof(unsigned char) * 3);

	if (current.address + 17 < 51) {
		createRawPointer(current.blockNum, current.address + 17, rnext);
		memcpy(&data[8], rnext, sizeof(unsigned char) * 3);
	} else {
		createRawPointer(system.freeBlock, 0, rnext);
		incrementFree();
		memcpy(&data[8], rnext, sizeof(unsigned char) * 3);
	}
	editBlock(current.blockNum, data, current.address, 17);
	return 0;
}

int initFile(blockPointer current, unsigned char *name) {
	sysinfo system;
	initSysinfo(&system);

	unsigned char data[13];
	unsigned char next[3];
	unsigned char size[2];

	createRawPointer(0, 0, next);
	toHex(0, size, 2);

	memcpy(&data[0], name, sizeof(unsigned char) * 8);
	memcpy(&data[8], next, sizeof(unsigned char) * 3);
	memcpy(&data[11], size, sizeof(unsigned char) * 2);

	editBlock(current.blockNum, data, current.address, 13);
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

	createRawPointer(2, 0, current);
	editBlock(0, current, 5, sizeof(current));

	editBlock(1, (unsigned char*)volumeName, 0, strlen(volumeName));
	return 0;
}


/*
 * Returns the volume's name in the result.
 * Returns 0 if no problem or -1 if the call failed.
 */
int volumeName(char *result) {
	readBlock(1, result, 0, 64);
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
}

/*
 * Writes data onto the end of the file.
 * Copies "length" bytes from data and appends them to the file.
 * The filename is a full pathname.
 * The file must have been created before this call is made.
 * Returns 0 if no problem or -1 if the call failed.
 */
int a2write(char *fileName, void *data, int length) {
	return -1;
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
	return -1;
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
