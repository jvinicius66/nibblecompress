/*************************************************************************************
 * main.c
 * Copyrights (c) 2018. All rights reserved.
 *
 * NOTICE:  All information contained herein, the intellectual and technical concepts
 * the property of João Vinícius Ramos Borges.
 * Patents in process are protected by trade secret or copyright law and reproduction
 * of this material is strictly forbidden unless prior written permission is obtained
 * of the author.
 *
*************************************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHAR_RANGE  0x10
#define CHAR_BITS   8

#define SETBIT(ADDRESS,BIT) (ADDRESS |= (1<<BIT))
#define CLEARBIT(ADDRESS,BIT) (ADDRESS &= ~(1<<BIT))
#define FLIPBIT(ADDRESS,BIT) (ADDRESS ^= (1<<BIT))
#define CHECKBIT(ADDRESS,BIT) (ADDRESS & (1<<BIT))
#define MSN(ADRESS) (ADRESS >> 4)
#define LSN(ADRESS) (ADRESS & 0xF)

#ifdef DEBUG
#define FMT_NIBBLE "%c%c%c%c"
#define PRINTNIBBLE(byte)  \
  (byte & 0x08 ? '1' : '0'), \
  (byte & 0x04 ? '1' : '0'), \
  (byte & 0x02 ? '1' : '0'), \
  (byte & 0x01 ? '1' : '0')
#endif // DEBUG

static unsigned char word = 0x0;
static unsigned short bitCount = 0;
static char offSet = -1;
static unsigned short int fileSize = 0;

typedef struct bTree {
    struct bTree *left;
    struct bTree *right;
    unsigned char letter;
    unsigned short int freq;
} bTree;

#ifdef DEBUG
void PrintTable(unsigned char len, bTree **table) {
    unsigned char i;
    for (i=0; i < len; i++) {
        bTree *node = (bTree *)table[i];
        printf("\n%2x  [%3d]", node->letter, node->freq);
    }
    printf("\n");
}

void PrintNode(bTree *node, unsigned char isRight, char ident[0xff]){
    char s[0xff];
    if (node->right) {
        sprintf(s, "%s%s", ident, (isRight ?  "        ": "|       ") );
        PrintNode(node->right, 1, s);
    }
    sprintf(s, "%s+----- %d", ident, node->freq);
    if ( (!node->left) && (!node->right) )
        sprintf(s, "%s ["FMT_NIBBLE"]", s, PRINTNIBBLE(node->letter));
    printf("%s\n", s);
    if (node->left) {
        sprintf(s, "%s%s", ident, (isRight ?  "|       ": "        ") );
        PrintNode(node->left, 0, s);
    }
}

void PrintTree(bTree *root){
    printf("\n\n");
    if (root->right)
        PrintNode(root->right, 1, "");
    printf("%d\n", root->freq);
    if (root->left)
        PrintNode(root->left, 0, "");
}
#endif // DEBUG

void Sort(unsigned char len, bTree **table) {
    unsigned char  i,j;
    for (i=0; i<len; i++)
        for (j=0; j<len; j++) {
            bTree *x = (bTree*)table[j], *y = (bTree *)table[i];
            if ( ( x->freq < y->freq) || ( (x->freq == y->freq) && (x->letter > y->letter) )) {
                bTree *_tmp = (bTree *)calloc(1, sizeof(bTree));
                *_tmp = *x;
                *x = *y;
                *y = *_tmp;
                free(_tmp);
            }
        }
}

char *Concat(char *prefix, char letter) {
    char *result = (char *)malloc(strlen(prefix) + 2);
    sprintf(result, "%s%c", prefix, letter);
    return result;
}

void Error(const char *msg) {
    fprintf(stderr, "Error: %s\n", msg);
    exit(1);
}

bTree *BuildTree(int frequencies[]) {
    int i, len = 0;
    bTree *queue[CHAR_RANGE];

    for(i = 0; i < CHAR_RANGE; i++) {
        if (frequencies[i]) {
            bTree *toadd = (bTree *)calloc(1, sizeof(bTree));
            toadd->letter = i;
            toadd->freq = frequencies[i];
            queue[len++] = toadd;
        }
    }

    while(len > 1) {
        bTree *toadd = (bTree *)malloc(sizeof(bTree));
        Sort(len, queue);
        #ifdef DEBUG
        //PrintTable(len, queue);
        #endif // DEBUG
        toadd->left = queue[--len];
        toadd->right = queue[--len];
        toadd->freq = toadd->left->freq + toadd->right->freq;
        toadd->letter = toadd->left->letter + toadd->right->letter;

        queue[len++] = toadd;
    }
    return queue[0];
}

void FreeTree(bTree *tree) {
    if(tree) {
        FreeTree(tree->left);
        FreeTree(tree->right);
        free(tree);
    }
}

void PathTree(bTree *tree, char **table, char *prefix) {
    if(!tree->left && !tree->right) {
        table[tree->letter] = prefix;
        #ifdef DEBUG
        printf("\n%2d ["FMT_NIBBLE"] %d = %s", tree->letter, PRINTNIBBLE(tree->letter),  tree->freq, prefix);
        #endif // DEBUG
    } else {
        if(tree->left)
            PathTree(tree->left, table, Concat(prefix, '0'));
        if(tree-> right)
            PathTree(tree->right, table, Concat(prefix, '1'));
        free(prefix);
    }
}

char **BuildTable(int frequencies[]) {
    static char *table[CHAR_RANGE];
    char *prefix = (char *)calloc(1, sizeof(char));
    bTree *tree = BuildTree(frequencies);
    PathTree(tree, table, prefix);
    #ifdef DEBUG
    PrintTree(tree);
    #endif // DEBUG
    FreeTree(tree);

    return table;
}

void FreeTable(char *table[]) {
    int i;
    for(i = 0; i < CHAR_RANGE; i++)
        if(table[i])
            free(table[i]);
}

void WriteHeader(int const count, int frequencies[], FILE *out) {
    char *prefix = (char *)calloc(1, sizeof(char));
    prefix = Concat(prefix, (count != 16 ? '0': '1'));
    int i;
    for(i=0;i<=2;i++)
        prefix = Concat(prefix, '0');
    if (count != 16)
        for (i=0; i<CHAR_RANGE; i++)
            prefix = Concat(prefix, (frequencies[i]? '1' : '0'));
    WriteBits(prefix, out);
}

int *ReadHeader(FILE *in) {
    static int frequencies[CHAR_RANGE];
    int i;
    unsigned char full;

    full = ReadBit(in);

    offSet = 0x0;
    for (i=0; i<3; i++)
        if (ReadBit(in))
            SETBIT(offSet, 2-i);

    for (i=0; i < CHAR_RANGE; i++)
        if (full || ReadBit(in) )
            frequencies[i] = 1;

    return frequencies;
}

void WriteBits(const char *encoding, FILE *out) {
    int i;
    for (i=0; i<strlen(encoding); i++) {
        if (encoding[i] == '1')
            SETBIT(word, 7-bitCount);
        else
            CLEARBIT(word, 7-bitCount);
        if (bitCount == 7) {
            bitCount = 0;
            fputc(word, out);
            if (offSet == -1)
                offSet = word;
            word = 0x0;
        } else
            bitCount++;
    }
}

void FlushBits(FILE *out) {
    if (bitCount != 0) {
        fputc(word, out);
        unsigned char diff = (8-bitCount) << 4;
        offSet |= diff;
        rewind(out);
        fputc(offSet, out);
    }
}

int ReadBit(FILE *in) {
    static int bits = 0, cntBit = 0;
    static char lastBits = -1;
    int nextbit;

    if (cntBit == 0) {
        bits = fgetc(in);
        fileSize--;
        cntBit = (1 << (CHAR_BITS - 1));
        if (fileSize==0 && lastBits == -1)
            lastBits = (CHAR_BITS-offSet);
    }

    if (lastBits != -1)
        if (lastBits == 0)
            return -1;
        else
            lastBits--;

    nextbit = bits / cntBit;
    bits %= cntBit;
    cntBit /= 2;

    return nextbit;
}

int DecodeChar(FILE *in, bTree *tree) {
    int path;
    while(tree->left || tree->right) {
        path = ReadBit(in);
        if (path == -1)
            return -1;
        if(path)
            tree = tree->right;
        else
            tree = tree->left;

        if(!tree)
            Error("invalid input file.");
    }
    return tree->letter;
}

void Decode(FILE *in, FILE *out) {
    int *frequencies, c;
    bTree *tree;
    char byte = -1;

    fseek(in, 0, SEEK_END);
    fileSize = ftell(in);
    fseek(in, 0, SEEK_SET);

    frequencies = ReadHeader(in);
    tree = BuildTree(frequencies);

    while ((c = DecodeChar(in, tree))>-1 ) {
        if (byte==-1)
            byte = c << 4;
        else {
            byte = byte | c;
            fputc(byte, out);
            byte = -1;
        }
        frequencies[c]++;
        tree = BuildTree(frequencies);
    }
    FreeTree(tree);
}

void Encode(FILE *in, FILE *out) {
    int c, frequencies[CHAR_RANGE] = { 0 };
    char **table;

    unsigned char count = 0;
    while((c = fgetc(in)) != EOF) {
        unsigned char msn = MSN(c);
        unsigned char lsn = LSN(c);
        if (!frequencies[msn]) {
            frequencies[msn]++;
            count++;
        }
        if (!frequencies[lsn]) {
            frequencies[lsn]++;
            count++;
        }
        if (count == CHAR_RANGE)
            break;
    }

    #ifdef DEBUG
    int i =0;
    for (i=0; i<0x10;i++) printf("%x ", i);
    printf("\n");
    for (i=0; i<0x10;i++) printf("%s ", (frequencies[i] ? "^" : " ") );
    unsigned short cicle = 0;
    #endif // DEBUG

    WriteHeader( count, frequencies, out);
    rewind(in);
    table = BuildTable(frequencies);

    while((c = fgetc(in)) != EOF) {
        unsigned char msn = MSN(c);
        #ifdef DEBUG
        printf("\nCICLE: %d", ++cicle);
        printf("\nMSN  : "FMT_NIBBLE, PRINTNIBBLE(msn));
        printf("\nPath : %s", table[msn]);
        printf("\n=============================================================\n");
        #endif // DEBUG
        WriteBits(table[msn], out);
        frequencies[msn]++;
        table = BuildTable(frequencies);

        unsigned char lsn = LSN(c);
        #ifdef DEBUG
        printf("\nCICLE: %d", ++cicle);
        printf("\nLSN : "FMT_NIBBLE, PRINTNIBBLE(lsn));
        printf("\nPath : %s", table[lsn]);
        printf("\n=============================================================\n");
        #endif // DEBUG
        WriteBits(table[lsn], out);
        frequencies[lsn]++;
        table = BuildTable(frequencies);
    }
    FlushBits(out);

    FreeTable(table);
}

int main(int argc, char *argv[]) {
    FILE *in, *out;

    if(argc != 4 || (strcmp(argv[1], "-c") && strcmp(argv[1], "-d"))) {
        fprintf(stderr, "Usage: %s [-c,-d] infile outfile\n", argv[0]);
        exit(0);
    }

    if(!(in = fopen(argv[2], "rb")))
        Error("input file couldn't be opened.");
    else if(!(out = fopen(argv[3], "wb")))
        Error("output file couldn't be opened.");

    if(!strcmp(argv[1], "-c"))
        Encode(in, out);
    else
        Decode(in, out);

    fclose(in);
    fclose(out);

    return 0;
}
