/*
 * display.c
 *
 *  Modified on: 23/03/2023
 *      Author: Robert Sheehan
 *
 *      This is used to display the blocks in the device.
 *      If run from the command line without a parameter it
 *      displays all blocks in the device.
 *      If run with an integer parameter it shows that many
 *      blocks from the device.
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "device.h"

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

int fromHex(unsigned char hex[]) {
	return (int)((hex[0] << 8) | hex[1]);
} 

int toHex(int n, unsigned char *hexArray) {
    hexArray[0] = (n >> 8) & 0xff;
    hexArray[1] = n & 0xff;
	return 0;
}

typedef struct {
    int blockNum; // 2 bytes
    int address;  // 1 bytes
} blockPointer;

typedef struct {
    int numBlocks;  // 2 bytes
    int freeBlock;  // 2 bytes
    blockPointer head;       // 3 bytes
	blockPointer next; 		// 3 bytes
	blockPointer current; // 3 bytes
} sysinfo;

typedef struct dirNode {
    char *name;              // 7 bytes
    blockPointer pointer;    // 3 bytes
    struct dirNode *next;    // 3 bytes 
} dir;

typedef struct {
    unsigned char data[64];  // 64 bytes
    int nextFree;            // 1 bytes
    int nextBlock;           // 2 bytes
} currentBlock;

typedef struct {
    char *name;      // 7 bytes
    int size;        // 2 bytes
    char *content;   // variable size (n - 9 bytes)
} file;


int incrementFree() {
	return 0;
}


int format(char *volumeName) {
	int blocks = numBlocks();
	// Normal Block
	unsigned char cblock[64] = {'\x0'};
	// System Block
	unsigned char sblock[64] = {'\x0'};
	unsigned char start[1];
	unsigned char numBlocks[2];
	unsigned char freeBlock[2];

	// Normal Block Init
	toHex(1, start);
	memcpy(&cblock[0], start, sizeof(start));
	for (int i = 1; i < blocks; i++) {
		editBlock(i, cblock, 0);
	}
	
	// System Block Init
	toHex(blocks, numBlocks);
	toHex(1, freeBlock);
	editBlock(0, strcat(numBlocks, freeBlock), 0);
	
	editBlock(1, volumeName, 0);
	return 0;
}

int initDir(unsigned char *cblock[64]) {

	return 0;
}

int memRemaining(unsigned char *cblock[64]) {
	return 61 - fromHex(cblock[0]);
}

int editBlock(int blockNum, unsigned char data, int start) {
	unsigned char cblock[64];
	blockRead(blockNum, cblock);
	memcpy(&cblock[start], data, sizeof(data));
	blockWrite(blockNum, cblock);
	return 0;
}

int writeToBlock(unsigned char *cblock[64], unsigned char *data, int length) {
	int i = 0;
	int valid = memRemaining(cblock);
	
	return 0;
}

int initSysinfo(unsigned char *cblock[64]) {

	return 0;
}



int main(int argc, char *argv[]) {
	int i;
	int numBlocksToDisplay;

	char *damn = "aw man";
	format(damn);

	unsigned char yes[64] = {"\0"};

	char test[22] = "Whereas I am in fileB";

	editBlock(0, test, 4);
	
	blockRead(0, yes);
	print_block(yes);

	printf("\033[1mBold text\033[0m\n");	
	
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
