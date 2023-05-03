/*
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include "device.h"

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



int main(int argc, char *argv[]) {
	int i;
	int numBlocksToDisplay;

	// char *damn = "oooooooooooooooooooooooooooooooooooooooooooooooooooooooooooo";
	// format(damn);

	sysinfo system;
	initSysinfo(&system);

	blockPointer def;
	createPointer(0, 0, &def);

	blockPointer current;
	blockPointer parent;
	dir currentDir;

	current = system.head;
	loadDir(current, &currentDir);
	printf("%s\n", currentDir.name);
	print_pointer(currentDir.parent);

	current = currentDir.next;
	loadDir(current, &currentDir);
	printf("%s\n", currentDir.name);
	print_pointer(currentDir.parent);

	current = currentDir.next;
	loadDir(current, &currentDir);
	printf("%s\n", currentDir.name);
	print_pointer(currentDir.parent);

	current = currentDir.next;
	loadDir(current, &currentDir);
	printf("%s\n", currentDir.name);
	print_pointer(currentDir.parent);

	current = currentDir.next;
	loadDir(current, &currentDir);
	printf("%s\n", currentDir.name);
	print_pointer(currentDir.parent);

	current = currentDir.next;
	loadDir(current, &currentDir);
	printf("%s\n", currentDir.name);
	if (strlen(currentDir.name) == 0) {
		printf("YES SIR");
	}
	if (currentDir.name[0] == '\0') {
		printf("OK I PULL");
	}

	if (pointerEquals(currentDir.next, def)) {
		printf("burh");
	}

	// initRoot();
	// current = system.head;
	// loadDir(current, &currentDir);
	// print_pointer(currentDir.next);
	// //current dir is now root

	// createPointer(current.blockNum, current.address, &parent);

	// current = currentDir.next;
	// initDir(current, "newfile", parent);
	// loadDir(current, &currentDir);
	// printf("%s\n", currentDir.name);
	// print_pointer(currentDir.next);

	// current = currentDir.next;
	// initDir(current, "bruhile", parent);
	// loadDir(current, &currentDir);
	// printf("%s\n", currentDir.name);
	// print_pointer(currentDir.next);

	// current = currentDir.next;
	// initDir(current, "asdasdasd", parent);
	// loadDir(current, &currentDir);
	// printf("%s\n", currentDir.name);
	// print_pointer(currentDir.next);

	// current = currentDir.next;
	// initDir(current, "aangerd", parent);
	// loadDir(current, &currentDir);
	// printf("%s\n", currentDir.name);
	// print_pointer(currentDir.next);


	if (argc < 2) {
		numBlocksToDisplay = numBlocks();
	} else {
		numBlocksToDisplay = atoi(argv[1]);
		if (numBlocksToDisplay < 1 || numBlocksToDisplay > numBlocks())
			numBlocksToDisplay = numBlocks();
	}
	for(i = 0; i < numBlocksToDisplay; ++i){
		displayBlock(i);
	}
    return EXIT_SUCCESS;
}
