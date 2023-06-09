#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct cacheLine {
    int valid;
    long long tag;
    int time;
    unsigned char* data;
} cacheLine;

typedef struct cacheSet {
    cacheLine* lines;
    int setId;
} cacheSet;

typedef struct cache {
    char* name;
    int setSize;
    int blockSize;
    int associativity;
    cacheSet* sets;

} cache;

cache constructCache(char* name, int s, int b, int e) {
    cache c = {name, 1<<s, 1<<b, 1<<e, NULL};
    c.sets = malloc(sizeof(cacheSet) * c.setSize);
    for (int i = 0; i < c.setSize; i++) {
        c.sets[i].setId = i;
        c.sets[i].lines = malloc(sizeof(cacheLine) * c.associativity);
        for (int j = 0; j < c.associativity; j++) {
            c.sets[i].lines[j].valid = 0;
            c.sets[i].lines[j].tag = 0;
            c.sets[i].lines[j].time = 0;
            c.sets[i].lines[j].data = malloc(sizeof(char) * c.blockSize);
        }
    }
    return c;
}

int time = 0;
cache L1I, L1D, L2;
unsigned char** ramData;
long ramSize;
FILE *output;
int L1s, L1E, L1b, L2s, L2E, L2b;
char* tracefile;


// hit - miss - evacuation
int L1D_counter[3] = {0, 0, 0};
int L1I_counter[3] = {0, 0, 0};
int L2_counter[3] = {0, 0, 0};

void readTrace(char* filepath, int s1, int b1, int s2, int b2);
unsigned char** readRam();
int load(cache* c, int setIndex, int tag, int ramIndex, unsigned char** ramData);
int storeCache(cache* c, int setIndex, int tag, int blockOffset, int ramIndex, int size, unsigned char** data);
void storeRam(int ramIndex, int size, unsigned char** data);
void printCache(cache* c);
void printRam();
void takeArguments(int argc, char *argv[]);

int main(int argc, char *argv[]){
    takeArguments(argc, argv);


    output = fopen("output.txt", "w");

    L1I = constructCache("L1I",L1s, L1b, L1E);
    L1D = constructCache("L1D", L1s, L1b, L1E);
    L2 = constructCache("L2", L2s, L2b, L2E);


    // TEST CODE
    char path[80] = "traces\\";
    char filename[50];
    
    //printf("Enter file name: ");
    //scanf("%s", filename);
    strcat(path, tracefile);
    ramData = readRam();



    readTrace(path, L1s, L1b, L2s, L2b);
    //printCache(&L1I);
    //printCache(&L1D);
    //printCache(&L2);

    printRam();
    fclose(output);
    
    return 0;
}

void takeArguments(int argc, char *argv[]) {
    // Check if the number of arguments is correct
    if (argc != 15) {
        printf("Invalid number of arguments. Please provide all required arguments.\n");
        exit(1);
    }

    for (int i = 1; i < argc; i +=2) {
        if (strcmp(argv[i], "-L1s") == 0)
            L1s = atoi(argv[i + 1]);
        else if (strcmp(argv[i], "-L1E") == 0)
            L1E = atoi(argv[i + 1]);
        else if (strcmp(argv[i], "-L1b") == 0)
            L1b = atoi(argv[i + 1]);
        else if (strcmp(argv[i], "-L2s") == 0)
            L2s = atoi(argv[i + 1]);
        else if (strcmp(argv[i], "-L2E") == 0)
            L2E = atoi(argv[i + 1]);
        else if (strcmp(argv[i], "-L2b") == 0)
            L2b = atoi(argv[i + 1]);
        else if (strcmp(argv[i], "-t") == 0)
            tracefile = argv[i + 1];
        else {
            printf("Invalid argument: %s\n", argv[i]);
            exit(1);
        }
    }
}

void readTrace(char* filepath, int s1, int b1, int s2, int b2) {
    FILE *trace = fopen(filepath, "r"); // open trace file

    char instr = fgetc(trace);
    while (instr != EOF) {
        // address and size are common for every instruction
        int address;
        int size;
        fscanf(trace, "%x, %d", &address, &size);

        // claculate the index of data in RAM from address
        int ramIndex = address / 8;

        // calculate set index, block offset and tag for L1 and L2 cahce
        int L1_setIndex = (address >> b1) & ((1 << s1) - 1);
        int L1_blockOffset = address & ((1 << b1) - 1);
        int L1_tag = address >> (s1 + b1);

        int L2_setIndex = (address >> b2) & ((1 << s2) - 1);
        int L2_blockOffset = address & ((1 << b2) - 1);
        int L2_tag = address >> (s2 + b2);

        // for instruction S and M
        char data[size*2 + 1];
        unsigned char* dataSplit[size];

        switch (instr) {
            case 'I':
                fprintf(output, "I %08x, %d\n", address, size);
                    
                int L1State = load(&L1I, L1_setIndex, L1_tag, ramIndex, ramData);
                L1State ? fprintf(output, "L1I hit, ") : fprintf(output, "L1I miss, ");
                int L2State = load(&L2, L2_setIndex, L2_tag, ramIndex, ramData);
                L2State ? fprintf(output, "L2 hit\n") : fprintf(output, "L2 miss\n");

                if (L1State && L2State) {
                    // L2 hit L1I hit
                    L2_counter[0]++;
                    L1I_counter[0]++;
                }
                else if(L1State && !L2State) {
                    // L2 miss L1I hit
                    L2_counter[1]++;
                    L1I_counter[0]++;
                    fprintf(output, "Place in L2 set %d\n", L2_setIndex);
                }
                else if(!L1State && L2State) {
                    // L2 hit L1I miss
                    L2_counter[0]++;
                    L1I_counter[1]++;
                    fprintf(output, "Place in L1I set %d\n", L1_setIndex);
                }
                else {
                    // L2 miss L1I miss
                    L2_counter[1]++;
                    L1I_counter[1]++;
                    fprintf(output, "Place in L2 set %d, L1I set %d\n", L2_setIndex, L1_setIndex);
                }
                fprintf(output,"\n");
                break;

            case 'L':
                fprintf(output, "D %08x, %d\n", address, size);

                L1State = load(&L1D, L1_setIndex, L1_tag, ramIndex, ramData);
                L1State ? fprintf(output, "L1D hit, ") : fprintf(output, "L1D miss, ");
                L2State = load(&L2, L2_setIndex, L2_tag, ramIndex, ramData);
                L2State ? fprintf(output, "L2 hit\n") : fprintf(output, "L2 miss\n");

                if (L1State && L2State) {
                    // L2 hit L1D hit
                    L2_counter[0]++;
                    L1D_counter[0]++;
                }
                else if(L1State && !L2State) {
                    // L2 miss L1D hit
                    L2_counter[1]++;
                    L1D_counter[0]++;
                    fprintf(output, "Place in L2 set %d\n", L2_setIndex);
                }
                else if(!L1State && L2State) {
                    // L2 hit L1D miss
                    L2_counter[0]++;
                    L1D_counter[1]++;
                    fprintf(output, "Place in L1D set %d\n", L1_setIndex);
                }
                else {
                    // L2 miss L1D miss
                    L2_counter[1]++;
                    L1D_counter[1]++;
                    fprintf(output, "Place in L2 set %d, L1D set %d\n", L2_setIndex, L1_setIndex);
                }

                fprintf(output,"\n");
                break;

            case 'S':
                // skip ',' and ' '
                for (int i = 0; i < size*2; i++) {
                    char c = fgetc(trace);
                    if (c != ',' && c != ' ')
                        data[i] = c;
                    else 
                        i--;
                }
                data[size*2] = '\0';
                
                fprintf(output, "S %08x, %d, %s\n", address, size, data);
                
                // split data value into bytes
                for (int i = 0; i < size; i++) {
                    dataSplit[i] = malloc(sizeof(unsigned char) * 3);
                    strncpy(dataSplit[i], &data[i*2], 2);
                    dataSplit[i][2] = '\0';
                }

                storeRam(ramIndex, size, dataSplit);

                L1State = storeCache(&L1D, L1_setIndex, L1_tag, L1_blockOffset, ramIndex, size, dataSplit);
                L2State = storeCache(&L2, L2_setIndex, L2_tag, L2_blockOffset, ramIndex, size, dataSplit);

                L1State ? fprintf(output, "L1D hit, ") : fprintf(output, "L1D miss, ");
                L2State ? fprintf(output, "L2 hit\n") : fprintf(output, "L2 miss\n");

                fprintf(output,"Store in ");

                if (L1State) {
                    // L1D hit
                    L1D_counter[0]++;
                    fprintf(output, "L1D, ");
                } else {
                    // L1D miss
                    L1D_counter[1]++;
                }
                if (L2State) {
                    // L2 hit
                    L2_counter[0]++;
                    fprintf(output, "L2, ");
                } else {
                    // L2 miss
                    L2_counter[1]++;
                }
                fprintf(output,"RAM\n\n");
            break;    

            case 'M':
                // skip ',' and ' '
                for (int i = 0; i < size*2; i++) {
                    char c = fgetc(trace);
                    if (c != ',' && c != ' ')
                        data[i] = c;
                    else
                        i--;
                }
                data[size*2] = '\0';

                fprintf(output, "M %08x, %d, %s\n", address, size, data);

                // split data value into bytes
                for (int i = 0; i < size; i++) {
                    dataSplit[i] = malloc(sizeof(unsigned char) * 3);
                    strncpy(dataSplit[i], &data[i*2], 2);
                    dataSplit[i][2] = '\0';
                }

                L1State=load(&L1D, L1_setIndex, L1_tag, ramIndex, ramData);
                L1State ? fprintf(output, "L1D hit, ") : fprintf(output, "L1D miss, ");
                L2State=load(&L2, L2_setIndex, L2_tag, ramIndex, ramData);
                L2State ? fprintf(output, "L2 hit\n") : fprintf(output, "L2 miss\n");

                if (L1State && L2State) {
                    // L1D hit L2 hit
                    L1D_counter[0]++;
                    L2_counter[0]++;
                    fprintf(output, "L1D, ");
                }
                else if(L1State && !L2State) {
                    // L1D hit L2 miss
                    L1D_counter[0]++;
                    L2_counter[1]++;
                    fprintf(output, "Place in L2 set %d\n", L2_setIndex);
                }
                else if(!L1State && L2State) {
                    // L1D miss L2 hit
                    L1D_counter[1]++;
                    L2_counter[0]++;
                    fprintf(output, "Place in L1D set %d\n", L1_setIndex);
                }
                else {
                    // L1D miss L2 miss
                    L1D_counter[1]++;
                    L2_counter[1]++;
                    fprintf(output, "Place in L2 set %d, L1D set %d\n", L2_setIndex, L1_setIndex);
                }

                // storeRam(L2_blockOffset, ramIndex, size, dataSplit);
                L1State = storeCache(&L1D, L1_setIndex, L1_tag, L1_blockOffset, ramIndex, size, dataSplit);
                L2State = storeCache(&L2, L2_setIndex, L2_tag, L2_blockOffset, ramIndex, size, dataSplit);
                L1State ? fprintf(output, "L1D hit, ") : fprintf(output, "L1D miss, ");
                L2State ? fprintf(output, "L2 hit\n") : fprintf(output, "L2 miss\n");
                storeRam(ramIndex, size, dataSplit);

                fprintf(output,"Store in ");

                if (L1State && L2State) {
                    // L1D hit L2 hit
                    L1D_counter[0]++;
                    L2_counter[0]++;
                    fprintf(output, "L1D, L2, ");
                }
                else if(L1State && !L2State) {
                    // L1D hit L2 miss
                    L1D_counter[0]++;
                    L2_counter[1]++;
                    fprintf(output, "L1D, ");
                }
                else if(!L1State && L2State) {
                    // L1D miss L2 hit
                    L1D_counter[1]++;
                    L2_counter[0]++;
                    fprintf(output, "L2, ");
                }
                else {
                    // L1D miss L2 miss
                    L1D_counter[1]++;
                    L2_counter[1]++;
                }

                fprintf(output,"RAM\n\n");
                break;
        }
        // read next instruction
        fgetc(trace);
        instr = fgetc(trace);

        time++;
    }
}

unsigned char** readRam() {
    FILE *ram = fopen("RAM.dat", "rb"); // open ram file

    fseek(ram, 0, SEEK_END);
    ramSize = ftell(ram);
    rewind(ram);

    unsigned char **ramData = malloc(sizeof(unsigned char) * ramSize);
    for (int i = 0; i < ramSize/8; i++) {
        ramData[i] = malloc(sizeof(unsigned char) * 8);
        fread(ramData[i], sizeof(unsigned char), 8, ram);
    }
    return ramData;
}

int load(cache* c, int setIndex, int tag, int ramIndex, unsigned char** ramData) {
    //Check for hit
    for (int i = 0; i < c->associativity; i++) {
        if(c->sets[setIndex].lines[i].valid == 1 && c->sets[setIndex].lines[i].tag == tag){
            //Hit
            c->sets[setIndex].lines[i].time = time;
            return 1;
        }
    }
    int lineIndex = -1;

    //Find the first empty line
    for (int i = 0; i < c->associativity; i++) {
        if (c->sets[setIndex].lines[i].valid == 0){
            lineIndex = i;
            break;
        }
    }
    if (lineIndex == -1) {
        int minTime = c->sets[setIndex].lines[0].time;
        for (int i = 0; i < c->associativity; i++) {
            if(c->sets[setIndex].lines[i].time < minTime){
                minTime = c->sets[setIndex].lines[i].time;
                lineIndex = i;
            }
        }
        if (strcmp(c->name, "L1D")==0){
            L1D_counter[2]++;
        }
        else if (strcmp(c->name, "L1I")==0){
            L1I_counter[2]++;
        }
        else if (strcmp(c->name, "L2")==0){
            L2_counter[2]++;
        }
    }

    c->sets[setIndex].lines[lineIndex].valid = 1;
    c->sets[setIndex].lines[lineIndex].tag = tag;
    c->sets[setIndex].lines[lineIndex].time = time;
    
    for (int i = 0; i < c->blockSize; i++) {
        c->sets[setIndex].lines[lineIndex].data[i] = ramData[ramIndex][i];
    }
    return 0;
}

void storeRam(int ramIndex, int size, unsigned char** data) {
    for (int i = 0; i < size; i++){
        ramData[ramIndex][i] = strtol(data[i], NULL, 16);
    }
}

int storeCache(cache* c, int setIndex, int tag, int blockOffset, int ramIndex, int size, unsigned char** data) {
    //Check for hit
    int hit = 0;
    for (int i = 0; i < c->associativity; i++) {
        if(c->sets[setIndex].lines[i].valid == 1 && c->sets[setIndex].lines[i].tag == tag){
            hit = 1;
            //Hit, print data to ram
            for (int j = 0; j < size; j++) {
                c->sets[setIndex].lines[i].data[blockOffset+j] = strtol(data[j], NULL, 16);
            }
            break;
        }
    }
    return hit;
}

void printCache(cache* c) {
    //Iterate through sets
    for (int i = 0; i < c->setSize; i++) {
        //Set
        printf("Set %d\n", i);
        cacheSet* set = &c->sets[i];
        //Iterating through lines
        for (int j = 0; j < c->associativity; j++) {
            //Line
            cacheLine* line = &set->lines[j];
            printf("Line %d: ", j);

            //Line attributes
            printf("Valid: %d, ", line->valid);
            printf("Tag: %d, ", line->tag);
            printf("Time: %d, ", line->time);
            printf("Data: ");
            for (int k = 0; k < c->blockSize; k++) {
                k == c->blockSize-1 ? printf("%02x", line->data[k]) : printf("%02x ", line->data[k]);
            }
            printf("\n");
        }
    }
}

void printRam() {
    FILE *ram = fopen("RAM_output.dat", "wb"); // open ram file

    for (int i = 0; i < ramSize; i++) {
        fwrite(ramData[i], sizeof(unsigned char), 8, ram);
    }
    fclose(ram);
}