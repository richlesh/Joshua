/* Chinook endgame database access - reentrant, thread-safe implementation.
 * Original algorithm: http://www.cs.ualberta.ca/~chinook/databases/code.c
 * University of Alberta Chinook project (public domain)
 *
 * After chinook_init(), all lookups are purely computational against
 * memory-mapped read-only data. No locks required.
 */

#include "chinook_db.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#ifndef _WIN32
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#endif
#include <sys/stat.h>

#ifndef O_BINARY
#define O_BINARY 0
#endif

/* ======================== Constants ======================== */

#define WHITE   0
#define BLACK   1
#define KINGS   2

#define TIE     0
#define WIN     1
#define LOSS    2
#define UNKNOWN 3

#define DB_UNKNOWN      (-9999)

#define BICOEF          8
#define MAX_PIECES      12
#define DISK_BLOCK      1024
#define DBTABLESIZE     (1 << 16)

#define DBINDEX(bk, wk, bp, wp) ((bk << 12) + (wk << 8) + (bp << 4) + wp)
#define Choose(n, k) (g_Bicoef[n][k])
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define MIN(a, b) ((a) < (b) ? (a) : (b))

/* ======================== Types ======================== */

typedef uint32_t WHERE_T;

#define BPBOARD 0
#define WPBOARD 1

typedef struct {
    int32_t rangeBK;
    int32_t rangeWK;
    int32_t firstBPIdx;
    int32_t nextBPIdx;
    int32_t numPos;
    unsigned *sidxbase;
    unsigned char *sidxindex;
    unsigned char nWP, nBP, nBK, nWK, rankBP, rankWP;
} DB_REC_T, *DB_REC_PTR_T;

typedef struct {
    unsigned char defaulttype;
    unsigned char value;
    unsigned short startbyte;
    uint32_t startaddr;
    uint32_t endaddr;
    DB_REC_PTR_T db;
} DBENTRY, *DBENTRYPTR;

/* ======================== Read-only global state (set once at init) ======================== */

static int g_Bicoef[33][BICOEF + 1];
static unsigned int g_ReverseByte[256];
static unsigned char g_NextBit[1 << 16];

static int g_Skip[14] = {0, 10, 15, 20, 25, 30, 40, 50, 60, 100, 200, 400, 800, 1600};
static int g_Div[7] = {0, 1, 3, 9, 27, 81, 243};

static DBENTRYPTR g_DBTable[DBTABLESIZE];

static const unsigned char *g_DBData;   /* mmap'd DB6 file */
static size_t g_DBSize;
static int32_t *g_DBIndex;             /* block -> first position index */
static int32_t g_MaxBlock;

static uint32_t g_RotBoard[256] = {
    0x00000000, 0x10000000, 0x00100000, 0x10100000,
    0x00001000, 0x10001000, 0x00101000, 0x10101000,
    0x00000010, 0x10000010, 0x00100010, 0x10100010,
    0x00001010, 0x10001010, 0x00101010, 0x10101010,
    0x01000000, 0x11000000, 0x01100000, 0x11100000,
    0x01001000, 0x11001000, 0x01101000, 0x11101000,
    0x01000010, 0x11000010, 0x01100010, 0x11100010,
    0x01001010, 0x11001010, 0x01101010, 0x11101010,
    0x00010000, 0x10010000, 0x00110000, 0x10110000,
    0x00011000, 0x10011000, 0x00111000, 0x10111000,
    0x00010010, 0x10010010, 0x00110010, 0x10110010,
    0x00011010, 0x10011010, 0x00111010, 0x10111010,
    0x01010000, 0x11010000, 0x01110000, 0x11110000,
    0x01011000, 0x11011000, 0x01111000, 0x11111000,
    0x01010010, 0x11010010, 0x01110010, 0x11110010,
    0x01011010, 0x11011010, 0x01111010, 0x11111010,
    0x00000100, 0x10000100, 0x00100100, 0x10100100,
    0x00001100, 0x10001100, 0x00101100, 0x10101100,
    0x00000110, 0x10000110, 0x00100110, 0x10100110,
    0x00001110, 0x10001110, 0x00101110, 0x10101110,
    0x01000100, 0x11000100, 0x01100100, 0x11100100,
    0x01001100, 0x11001100, 0x01101100, 0x11101100,
    0x01000110, 0x11000110, 0x01100110, 0x11100110,
    0x01001110, 0x11001110, 0x01101110, 0x11101110,
    0x00010100, 0x10010100, 0x00110100, 0x10110100,
    0x00011100, 0x10011100, 0x00111100, 0x10111100,
    0x00010110, 0x10010110, 0x00110110, 0x10110110,
    0x00011110, 0x10011110, 0x00111110, 0x10111110,
    0x01010100, 0x11010100, 0x01110100, 0x11110100,
    0x01011100, 0x11011100, 0x01111100, 0x11111100,
    0x01010110, 0x11010110, 0x01110110, 0x11110110,
    0x01011110, 0x11011110, 0x01111110, 0x11111110,
    0x00000001, 0x10000001, 0x00100001, 0x10100001,
    0x00001001, 0x10001001, 0x00101001, 0x10101001,
    0x00000011, 0x10000011, 0x00100011, 0x10100011,
    0x00001011, 0x10001011, 0x00101011, 0x10101011,
    0x01000001, 0x11000001, 0x01100001, 0x11100001,
    0x01001001, 0x11001001, 0x01101001, 0x11101001,
    0x01000011, 0x11000011, 0x01100011, 0x11100011,
    0x01001011, 0x11001011, 0x01101011, 0x11101011,
    0x00010001, 0x10010001, 0x00110001, 0x10110001,
    0x00011001, 0x10011001, 0x00111001, 0x10111001,
    0x00010011, 0x10010011, 0x00110011, 0x10110011,
    0x00011011, 0x10011011, 0x00111011, 0x10111011,
    0x01010001, 0x11010001, 0x01110001, 0x11110001,
    0x01011001, 0x11011001, 0x01111001, 0x11111001,
    0x01010011, 0x11010011, 0x01110011, 0x11110011,
    0x01011011, 0x11011011, 0x01111011, 0x11111011,
    0x00000101, 0x10000101, 0x00100101, 0x10100101,
    0x00001101, 0x10001101, 0x00101101, 0x10101101,
    0x00000111, 0x10000111, 0x00100111, 0x10100111,
    0x00001111, 0x10001111, 0x00101111, 0x10101111,
    0x01000101, 0x11000101, 0x01100101, 0x11100101,
    0x01001101, 0x11001101, 0x01101101, 0x11101101,
    0x01000111, 0x11000111, 0x01100111, 0x11100111,
    0x01001111, 0x11001111, 0x01101111, 0x11101111,
    0x00010101, 0x10010101, 0x00110101, 0x10110101,
    0x00011101, 0x10011101, 0x00111101, 0x10111101,
    0x00010111, 0x10010111, 0x00110111, 0x10110111,
    0x00011111, 0x10011111, 0x00111111, 0x10111111,
    0x01010101, 0x11010101, 0x01110101, 0x11110101,
    0x01011101, 0x11011101, 0x01111101, 0x11111101,
    0x01010111, 0x11010111, 0x01110111, 0x11110111,
    0x01011111, 0x11011111, 0x01111111, 0x11111111
};

static bool g_ready = false;

/* ======================== Helper macros (all use local vars) ======================== */

#define NEXT_BIT(POS, BITS) \
    POS = BITS >> 16; \
    if (POS == 0) POS = g_NextBit[BITS]; \
    else POS = g_NextBit[POS] + 16;

#define EXTRACT_PIECES(vec, nXX, XXpos) \
{ \
    nXX = 0; \
    while (vec) { \
        int _pos; \
        NEXT_BIT(_pos, vec); \
        XXpos[nXX] = _pos; \
        vec ^= (uint32_t)(1U << XXpos[(nXX)++]); \
    } \
}

#define CountHits(XXpos, nXX, YYpos, nYY, XXhits) \
{ \
    if (nYY > 0) { \
        for (int _x = 0; _x < nXX; _x++) { \
            if (XXpos[_x] < YYpos[nYY-1]) break; \
            XXhits[_x]++; \
            for (int _y = nYY - 2; _y >= 0; _y--) { \
                if (XXpos[_x] < YYpos[_y]) break; \
                XXhits[_x]++; \
            } \
        } \
    } \
}

#define CountRevHits(XXpos, nXX, YYpos, nYY, XXhits) \
{ \
    if (nYY > 0) { \
        for (int _x = nXX - 1; _x >= 0; _x--) { \
            if (XXpos[_x] > YYpos[0]) break; \
            XXhits[_x]++; \
            for (int _y = 1; _y < nYY; _y++) { \
                if (XXpos[_x] > YYpos[_y]) break; \
                XXhits[_x]++; \
            } \
        } \
    } \
}

/* ======================== Pure functions (no global mutable state) ======================== */

static uint32_t RotateBoard(uint32_t vecrot) {
    uint32_t vec;
    vec  = g_RotBoard[(vecrot >> 24) & 0xFF];
    vec |= g_RotBoard[(vecrot >> 16) & 0xFF] << 1;
    vec |= g_RotBoard[(vecrot >>  8) & 0xFF] << 2;
    vec |= g_RotBoard[ vecrot        & 0xFF] << 3;
    return vec;
}

static int32_t DBrevindex(int *pos, int k) {
    int32_t offset = 0;
    while (k > 0) offset += Choose(*pos++, k--);
    return offset;
}

static int32_t DBindex(int *pos, int k) {
    int32_t offset = 0;
    pos = &pos[k - 1];
    while (k > 0) offset += Choose(*pos--, k--);
    return offset;
}

/* Compute position index from board vectors (all local state) */
static uint32_t dbLocbvToSubIdx(uint32_t locbv_white, uint32_t locbv_black,
                                 uint32_t locbv_kings, int turn,
                                 DBENTRYPTR *dbentry) {
    int nbk, nwk, nbp, nwp, rankbp, rankwp;
    int bppos[MAX_PIECES], wppos[MAX_PIECES];
    int bkpos[MAX_PIECES], wkpos[MAX_PIECES];
    int XXhits[MAX_PIECES];
    int bpidx, wpidx, bkidx, wkidx, firstidx;
    uint32_t vec, Blackbv, Whitebv, Kingbv;

    union { uint32_t U; unsigned char C[4]; } a, b;

    /* If white to move, reverse board and swap colors */
    if (turn == WHITE) {
        a.U = locbv_white;
        b.C[3] = (unsigned char)g_ReverseByte[a.C[0]];
        b.C[2] = (unsigned char)g_ReverseByte[a.C[1]];
        b.C[1] = (unsigned char)g_ReverseByte[a.C[2]];
        b.C[0] = (unsigned char)g_ReverseByte[a.C[3]];
        Blackbv = b.U;
        a.U = locbv_black;
        b.C[3] = (unsigned char)g_ReverseByte[a.C[0]];
        b.C[2] = (unsigned char)g_ReverseByte[a.C[1]];
        b.C[1] = (unsigned char)g_ReverseByte[a.C[2]];
        b.C[0] = (unsigned char)g_ReverseByte[a.C[3]];
        Whitebv = b.U;
        a.U = locbv_kings;
        b.C[3] = (unsigned char)g_ReverseByte[a.C[0]];
        b.C[2] = (unsigned char)g_ReverseByte[a.C[1]];
        b.C[1] = (unsigned char)g_ReverseByte[a.C[2]];
        b.C[0] = (unsigned char)g_ReverseByte[a.C[3]];
        Kingbv = b.U;
    } else {
        Blackbv = locbv_black;
        Whitebv = locbv_white;
        Kingbv = locbv_kings;
    }

    /* Extract black pawns */
    vec = RotateBoard(Blackbv & ~Kingbv);
    EXTRACT_PIECES(vec, nbp, bppos);
    if (nbp) { bpidx = DBrevindex(bppos, nbp); rankbp = bppos[0] >> 2; }
    else { bpidx = 0; rankbp = 0; }

    /* Extract white pawns */
    vec = RotateBoard(Whitebv & ~Kingbv);
    EXTRACT_PIECES(vec, nwp, wppos);
    if (nwp) {
        for (int i = nwp - 1; i >= 0; i--) XXhits[i] = 0;
        CountRevHits(wppos, nwp, bppos, nbp, XXhits);
        for (int i = nwp - 1; i >= 0; i--) XXhits[i] = 31 - (wppos[i] + XXhits[i]);
        wpidx = DBindex(XXhits, nwp);
        rankwp = (31 - wppos[nwp - 1]) >> 2;
    } else { wpidx = 0; rankwp = 0; }

    /* Extract black kings */
    vec = RotateBoard(Blackbv & Kingbv);
    EXTRACT_PIECES(vec, nbk, bkpos);
    if (nbk) {
        for (int i = nbk - 1; i >= 0; i--) XXhits[i] = 0;
        CountHits(bkpos, nbk, wppos, nwp, XXhits);
        CountHits(bkpos, nbk, bppos, nbp, XXhits);
        for (int i = nbk - 1; i >= 0; i--) XXhits[i] = bkpos[i] - XXhits[i];
        bkidx = DBrevindex(XXhits, nbk);
    } else bkidx = 0;

    /* Extract white kings */
    vec = RotateBoard(Whitebv & Kingbv);
    EXTRACT_PIECES(vec, nwk, wkpos);
    if (nwk) {
        for (int i = nwk - 1; i >= 0; i--) XXhits[i] = 0;
        CountHits(wkpos, nwk, bkpos, nbk, XXhits);
        CountHits(wkpos, nwk, wppos, nwp, XXhits);
        CountHits(wkpos, nwk, bppos, nbp, XXhits);
        for (int i = nwk - 1; i >= 0; i--) XXhits[i] = wkpos[i] - XXhits[i];
        wkidx = DBrevindex(XXhits, nwk);
    } else wkidx = 0;

    /* Look up in DBTable */
    *dbentry = g_DBTable[DBINDEX(nbk, nwk, nbp, nwp)];
    if (*dbentry == nullptr || *dbentry == (DBENTRYPTR)-1)
        return (uint32_t)DB_UNKNOWN;

    if (nwp == 0) *dbentry += rankbp;
    else *dbentry += (rankbp * 7 + rankwp);

    if ((*dbentry)->value != UNKNOWN)
        return (uint32_t)(-((*dbentry)->value + 1));

    DB_REC_PTR_T db = (*dbentry)->db;
    if (db == nullptr)
        return (uint32_t)(DB_UNKNOWN - 1);

    unsigned *baseptr = db->sidxbase;
    unsigned char *indexptr = db->sidxindex;
    bpidx -= db->firstBPIdx;

    firstidx = Choose((indexptr[bpidx] >> 3), (indexptr[bpidx] & 07));
    bpidx = baseptr[bpidx] + wpidx - firstidx;
    int Index = (((bpidx * db->rangeBK) + bkidx) * db->rangeWK) + wkidx;
    return (uint32_t)Index;
}

/* Look up value from mmap'd data (no mutable state) */
static int32_t DBLookup(uint32_t locbv_white, uint32_t locbv_black,
                         uint32_t locbv_kings, int turn) {
    DBENTRYPTR dbentry;
    uint32_t index = dbLocbvToSubIdx(locbv_white, locbv_black, locbv_kings, turn, &dbentry);

    /* Check if value is already known */
    if ((int32_t)index < 0) {
        if (index == (uint32_t)(-(WIN + 1))) return WIN;
        if (index == (uint32_t)(-(LOSS + 1))) return LOSS;
        if (index == (uint32_t)(-(TIE + 1))) return TIE;
        return DB_UNKNOWN;
    }

    /* Binary search for the block containing this position */
    int32_t start = (int32_t)dbentry->startaddr;
    int32_t end = (int32_t)dbentry->endaddr;
    int32_t byte = dbentry->startbyte;

    while (start < end) {
        int32_t middle = (start + end + 1) / 2;
        if (g_DBIndex[middle] <= (int32_t)index)
            start = middle;
        else
            end = middle - 1;
    }
    if (start != end) return DB_UNKNOWN;

    int32_t block = start;

    /* Read directly from mmap'd data */
    size_t offset = (size_t)block * DISK_BLOCK;
    if (offset >= g_DBSize) return DB_UNKNOWN;
    const unsigned char *buffer = g_DBData + offset;

    /* Determine starting byte and position counter */
    int i;
    uint32_t cindex;
    if (block == (int32_t)dbentry->startaddr) {
        i = byte;
        cindex = 0;
    } else {
        i = 0;
        cindex = g_DBIndex[block];
    }

    int def = dbentry->defaulttype;
    int32_t result = def;

    /* Scan through block to find position value */
    int blockLen = (block == g_MaxBlock) ? (int)(g_DBSize - offset) : DISK_BLOCK;
    for (; i < blockLen; i++) {
        uint32_t c = buffer[i] & 0xff;
        if (c > 242) {
            cindex += g_Skip[c - 242];
            if (index < cindex) { result = def; break; }
        } else {
            if (cindex + 5 <= index)
                cindex += 5;
            else {
                int diff = index - cindex;
                result = c % g_Div[5 - diff + 1];
                result = result / g_Div[5 - diff];
                break;
            }
        }
    }

    if (i >= blockLen) return DB_UNKNOWN;

    return result;
}

/* ======================== sidxCreate and helpers (init-time only) ======================== */

static int32_t Nsq(int32_t rank, WHERE_T *Ppos, int32_t nP) {
    int32_t pawnhit = ((7 - rank) << 2);
    int32_t nsquares = ((rank + 1) << 2);
    while (nP--) { if ((int32_t)*Ppos++ >= pawnhit) nsquares--; }
    return nsquares;
}

/* Index enumeration state (used only during init) */
static WHERE_T s_BoardPos[4][12];
static WHERE_T *s_BPK[4];
static WHERE_T s_BPsave[12];
static WHERE_T *s_BPKsave;

static void InitIndex(int32_t off, int32_t k, int32_t n) {
    WHERE_T *BPP, *BPE;
    s_BPK[off] = &s_BoardPos[off][k];
    for (BPP = s_BoardPos[off], BPE = &s_BPK[off][-1]; BPP < BPE; BPP++)
        *BPP = (WHERE_T)(BPP - s_BoardPos[off]);
    *BPE = n;
}

static void NextIndex(int32_t off) {
    WHERE_T *BPP, *BPE = &s_BPK[off][-1];
    for (BPP = s_BoardPos[off]; BPP < BPE && *BPP == BPP[1] - 1; BPP++)
        *BPP = (WHERE_T)(BPP - s_BoardPos[off]);
    *BPP += 1;
}

static void SaveIndex(int32_t off) {
    s_BPKsave = s_BPK[off];
    memcpy(s_BPsave, s_BoardPos[off], 12 * sizeof(WHERE_T));
}

static void LoadIndex(int32_t off) {
    s_BPK[off] = s_BPKsave;
    memcpy(s_BoardPos[off], s_BPsave, 12 * sizeof(WHERE_T));
}

static void sidxCreate(DB_REC_PTR_T p, int32_t nBP, int32_t nWP,
                       int32_t rankBPL, int32_t rankWPL,
                       int32_t *numPos, int32_t *firstBPLIdx, int32_t *nextBPLIdx) {
    uint32_t firstWPLIdx = 0, nextWPLIdx = 0, Base = 0;
    int32_t fsq = 0, fpc = 0;

    if (nBP == 0) { *firstBPLIdx = 0; *nextBPLIdx = 1; }
    else {
        *firstBPLIdx = Choose(rankBPL << 2, nBP);
        *nextBPLIdx = Choose((rankBPL + 1) << 2, nBP);
    }

    p->sidxbase = (unsigned *)malloc((1 + *nextBPLIdx - *firstBPLIdx) * sizeof(unsigned));
    p->sidxindex = (unsigned char *)malloc((1 + *nextBPLIdx - *firstBPLIdx) * sizeof(unsigned char));

    if (nWP == 0) { firstWPLIdx = 0; fpc = BICOEF - 1; nextWPLIdx = 1; }
    if (nBP) { SaveIndex(BPBOARD); InitIndex(BPBOARD, nBP, MAX(rankBPL << 2, nBP - 1)); }

    unsigned *baseptr = (unsigned *)p->sidxbase;
    unsigned char *indexptr = p->sidxindex;

    for (uint32_t Maximum = *nextBPLIdx - *firstBPLIdx; Maximum; Maximum--) {
        if (nWP) {
            int32_t nsqr1, nsqr2;
            if (nBP) {
                nsqr1 = Nsq(rankWPL - 1, s_BoardPos[BPBOARD], nBP);
                nsqr2 = Nsq(rankWPL, s_BoardPos[BPBOARD], nBP);
            } else {
                nsqr1 = rankWPL << 2;
                nsqr2 = (rankWPL + 1) << 2;
            }
            firstWPLIdx = Choose(nsqr1, nWP);
            nextWPLIdx = Choose(nsqr2, nWP);
            fsq = nsqr1;
            fpc = nWP;
        }
        *baseptr++ = Base;
        *indexptr++ = (unsigned char)((fsq << 3) | fpc);
        Base += (nextWPLIdx - firstWPLIdx);
        if (nBP) NextIndex(BPBOARD);
    }
    if (nBP) LoadIndex(BPBOARD);
    *numPos = Base;
}

static DB_REC_PTR_T dbCreate(int32_t nbk, int32_t nwk, int32_t nbp, int32_t nwp,
                             int32_t rankbp, int32_t rankwp) {
    DB_REC_PTR_T p = (DB_REC_PTR_T)malloc(sizeof(DB_REC_T));
    p->rangeBK = Choose(32 - nbp - nwp, nbk);
    p->rangeWK = Choose(32 - nbp - nwp - nbk, nwk);
    sidxCreate(p, nbp, nwp, rankbp, rankwp, &p->numPos, &p->firstBPIdx, &p->nextBPIdx);
    p->numPos *= p->rangeBK * p->rangeWK;
    p->nBP = (unsigned char)nbp; p->nWP = (unsigned char)nwp;
    p->nBK = (unsigned char)nbk; p->nWK = (unsigned char)nwk;
    p->rankBP = (unsigned char)rankbp; p->rankWP = (unsigned char)rankwp;
    return p;
}

/* ======================== Init ======================== */

static char g_dbDirPath[2048];

static const char *dbPath(const char *filename) {
    static char buf[2048];
    snprintf(buf, sizeof(buf), "%s/%s", g_dbDirPath, filename);
    return buf;
}

int chinook_init(const char *db_dir) {
    int i, j;

    strncpy(g_dbDirPath, db_dir, sizeof(g_dbDirPath) - 1);
    g_dbDirPath[sizeof(g_dbDirPath) - 1] = '\0';

    /* Initialize NextBit table */
    unsigned int c = 0;
    g_NextBit[0] = 0;
    for (i = 1; i < (1 << 16); i++) {
        if ((i & (1 << c)) == 0) c++;
        g_NextBit[i] = (unsigned char)c;
    }

    /* ReverseByte */
    for (i = 0; i < 256; i++) {
        g_ReverseByte[i] = 0;
        for (j = 0; j < 8; j++)
            if (i & (1 << j)) g_ReverseByte[i] |= (1 << (7 - j));
    }

    /* Binomial coefficients */
    for (i = 0; i <= 32; i++) {
        int m = MIN(BICOEF, i);
        for (j = 0; j <= m; j++) {
            if (j == 0 || i == j) g_Bicoef[i][j] = 1;
            else g_Bicoef[i][j] = g_Bicoef[i - 1][j] + g_Bicoef[i - 1][j - 1];
        }
    }
    g_Bicoef[0][BICOEF] = 0;

    /* Initialize DBTable */
    for (i = 0; i < DBTABLESIZE; i++) g_DBTable[i] = (DBENTRYPTR)-1;

    int DBPieces = 8;
    for (int p = 2; p <= DBPieces; p++) {
        for (int b = 1; b <= p; b++) {
            int w = p - b;
            if (w == 0) continue;
            for (int bk = b; bk >= 0; bk--) {
                for (int wk = w; wk >= 0; wk--) {
                    int bp = b - bk, wp = w - wk;
                    int number;
                    if (bp == 0 && wp == 0) number = 1;
                    else if (bp == 0 || wp == 0) number = 7;
                    else number = 49;
                    int idx = DBINDEX(bk, wk, bp, wp);
                    DBENTRYPTR ptr = (DBENTRYPTR)malloc(sizeof(DBENTRY) * number);
                    if (!ptr) return 0;
                    g_DBTable[idx] = ptr;
                    for (int k = 0; k < number; k++) {
                        ptr[k].value = UNKNOWN;
                        ptr[k].defaulttype = UNKNOWN;
                        ptr[k].startaddr = 0;
                        ptr[k].startbyte = 0;
                        ptr[k].endaddr = 0;
                        ptr[k].db = nullptr;
                    }
                }
            }
        }
    }

    /* mmap the database file */
#ifdef _WIN32
    FILE *dbfp = fopen(dbPath("DB6"), "rb");
    if (!dbfp) return 0;
    fseek(dbfp, 0, SEEK_END);
    g_DBSize = (size_t)ftell(dbfp);
    fseek(dbfp, 0, SEEK_SET);
    void *mapped = malloc(g_DBSize);
    if (!mapped) { fclose(dbfp); return 0; }
    fread(mapped, 1, g_DBSize, dbfp);
    fclose(dbfp);
    g_DBData = (const unsigned char *)mapped;
#else
    int fd = open(dbPath("DB6"), O_RDONLY);
    if (fd < 0) return 0;

    struct stat st;
    if (fstat(fd, &st) < 0) { close(fd); return 0; }
    g_DBSize = (size_t)st.st_size;

    void *mapped = mmap(nullptr, g_DBSize, PROT_READ, MAP_PRIVATE, fd, 0);
    close(fd);
    if (mapped == MAP_FAILED) return 0;
    g_DBData = (const unsigned char *)mapped;
#endif

    int32_t blocks = (int32_t)(g_DBSize / DISK_BLOCK);
    g_MaxBlock = blocks++;
    g_DBIndex = (int32_t *)malloc(blocks * sizeof(int32_t));
    if (!g_DBIndex) return 0;

    /* Parse index file */
    FILE *idxfp = fopen(dbPath("DB6.idx"), "r");
    if (!idxfp) return 0;

    int32_t dbidx = -1;
    int limit = DBPieces;
    DBPieces = 0;

    for (;;) {
        int bk, wk, bp, wp, br, wr;
        char dt, dtt;
        int stat2 = fscanf(idxfp, "BASE%1d%1d%1d%1d.%1d%1d %c%c",
                          &bk, &wk, &bp, &wp, &br, &wr, &dt, &dtt);
        if (stat2 <= 0) break;
        if (stat2 < 8) break;

        int pc = bk + wk + bp + wp;
        if (pc > limit) break;
        DBPieces = pc;

        int idx = DBINDEX(bk, wk, bp, wp);
        DBENTRYPTR ptr = g_DBTable[idx];
        if (!ptr) break;

        int number = (wp == 0) ? br : br * 7 + wr;
        ptr += number;

        if (dtt != '\n') {
            if (dtt == '+') ptr->value = WIN;
            else if (dtt == '-') ptr->value = LOSS;
            else if (dtt == '=') ptr->value = TIE;
            fscanf(idxfp, "\n");
            continue;
        }

        ptr->db = dbCreate((int32_t)bk, (int32_t)wk, (int32_t)bp,
                           (int32_t)wp, (int32_t)br, (int32_t)wr);

        int32_t deftype;
        if (dt == '=') deftype = TIE;
        else if (dt == '+') deftype = WIN;
        else if (dt == '-') deftype = LOSS;
        else break;
        ptr->defaulttype = (unsigned char)deftype;

        char next = (char)getc(idxfp);
        if (next != 'S') break;

        int32_t index2, addr, byte;
        if (fscanf(idxfp, "%ld%ld/%ld\n", &index2, &addr, &byte) != 3) break;
        ptr->startaddr = addr;
        ptr->startbyte = (unsigned short)byte;

        if (addr != dbidx) { g_DBIndex[++dbidx] = index2; }

        for (;;) {
            char c;
            if (fscanf(idxfp, "%c", &c) != 1) break;
            if (c == 'E') break;
            int32_t idx3, addr2;
            if (fscanf(idxfp, "%ld%ld\n", &idx3, &addr2) != 2) break;
            if (addr2 != dbidx) { g_DBIndex[++dbidx] = idx3; }
        }
        int32_t idx4, addr3, byte3;
        if (fscanf(idxfp, "%ld%ld/%ld\n", &idx4, &addr3, &byte3) != 3) break;
        ptr->endaddr = addr3;
    }
    fclose(idxfp);

    g_ready = true;
    return 1;
}

/* ======================== Public API ======================== */

int chinook_lookup(uint32_t locbv_white, uint32_t locbv_black,
                   uint32_t locbv_kings, int turn) {
    if (!g_ready) return CHINOOK_UNKNOWN;

    int32_t result = DBLookup(locbv_white, locbv_black, locbv_kings, turn);

    if (result == WIN) return CHINOOK_WIN;
    if (result == LOSS) return CHINOOK_LOSS;
    if (result == TIE) return CHINOOK_TIE;
    return CHINOOK_UNKNOWN;
}

void chinook_cleanup(void) {
    if (g_DBData) {
        munmap((void *)g_DBData, g_DBSize);
        g_DBData = nullptr;
    }
    g_ready = false;
}
