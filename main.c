#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct cacheLine{
    int valid;
    long long tag;
    int time;
    char* data;
} cacheLine;

typedef struct cacheSet{
    cacheLine* lines;
    int setId;
} cacheSet;

typedef struct cache{
    int setSize;
    int blockSize;
    int associativity;
    cacheSet* sets;

}cache;

cache constructCache(int s, int b, int e){

    cache c = {1<<s,1<<b,1<<e,NULL};
    c.sets = malloc(sizeof(cacheSet)*c.setSize);
    for (int i = 0; i < c.setSize; i++)
    {
        c.sets[i].setId = i;
        c.sets[i].lines = malloc(sizeof(cacheLine)*c.associativity);
        for (int j = 0; j < c.associativity; j++)
        {
            c.sets[i].lines[j].valid = 0;
            c.sets[i].lines[j].tag = 0;
            c.sets[i].lines[j].time = 0;
            c.sets[i].lines[j].data = malloc(sizeof(char)*c.blockSize);
        }
    }
    return c;
}

void readTrace(char* filepath, int s1, int b1, int s2, int b2);

int main(){
    cache L1I = constructCache(0,2,3);
    cache L1D = constructCache(0,2,3);
    cache L2 = constructCache(1,2,3);


    // TEST CODE
    char path[30] = "traces\\";
    char filename[20];
    printf("Enter file name: ");
    scanf("%s", filename);
    strcat(path, filename);

    readTrace(path, 2, 3 ,1, 5);
}

void readTrace(char* filepath, int s1, int b1, int s2, int b2) {
    FILE *trace = fopen(filepath, "r"); // opean trace file

    char instr = fgetc(trace);
    while (instr != EOF) {
        // address and size are common for every instruction
        int address;
        int size;
        fscanf(trace, "%x, %d", &address, &size);

        // calculate set index, block offset and tag for L1 and L2 cahce
        int L1_setIndex = (address >> b1) & ((1 << s1) - 1);
        int L1_blockOffset = address & ((1 << b1) - 1);
        int L1_tag = address >> (s1 + b1);

        int L2_setIndex = (address >> b2) & ((1 << s2) - 1);
        int L2_blockOffset = address & ((1 << b2) - 1);
        int L2_tag = address >> (s2 + b2);

        switch (instr) {
            case 'I':
                // call func
                break;

            case 'L':
                // call func
                break;

            case 'S':
                // read data value for S instr
                int data;
                fscanf(trace, "%x", &data);

                // call func
                break;

            case 'M':
                // read data value for M instr
                fscanf(trace, "%x", &data);
    
                // call func
                break;
        }
        // read next instruction
        fgetc(trace); // skip '\n' character
        instr = fgetc(trace);
    }
}