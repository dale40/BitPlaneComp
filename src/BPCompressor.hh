#ifndef __BP_COMPRESSOR_HH__
#define __BP_COMPRESSOR_HH__

#include "common.hh"

class BPCompressor : public Compressor {
public:
    BPCompressor(const string name) : Compressor(name) {}
    unsigned compressLine(CACHELINE_DATA* line, UINT64 line_addr) {
        INT64 deltas[31];
        for (int i=1; i<_MAX_DWORDS_PER_LINE; i++) {
            deltas[i-1] = ((INT64) line->s_dword[i]) - ((INT64) line->s_dword[i-1]);
        }

        INT32 prevDBP;
        INT32 DBP[33];
        INT32 DBX[33];
        for (int j=63; j>=0; j--) {
            INT32 buf = 0;
            for (int i=30; i>=0; i--) {
                buf <<= 1;
                buf |= ((deltas[i]>>j)&1);
            }
            if (j==63) {
                DBP[32] = buf;
                DBX[32] = buf;
                prevDBP = buf;
            } else if (j<32) {
                DBP[j] = buf;
                DBX[j] = buf^prevDBP;
                prevDBP = buf;
            } else {
                assert(buf==prevDBP);
                prevDBP = buf;
            }
        }
        
        // first 32-bit word in original form
        unsigned blkLength = encodeFirst(line->dword[0]);
        blkLength += encodeDeltas(DBP, DBX);

        countLineResult(blkLength);
        return blkLength;
    }

    virtual unsigned encodeFirst(INT32 sym) {
        if (sym==0) {
            countPattern(256);
            return 3;
        } else if (sign_extended(sym, 4)) {
            countPattern(257);
            return (3+4);
        } else if (sign_extended(sym, 8)) {
            countPattern(258);
            return (3+8);
        } else if (sign_extended(sym, 16)) {
            countPattern(259);
            return (3+16);
        } else {
            countPattern(263);
            return (1+32);
        }
    }
    virtual unsigned encodeDeltas(INT32* DBP, INT32* DBX) {
        static const unsigned ZRL_CODE_SIZE[34] = {0, 3, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7};
        static const unsigned singleOneSize = 10;
        static const unsigned consecutiveDoubleOneSize = 10;
        static const unsigned allOneSize = 5;
        static const unsigned zeroDBPSize = 5;

        unsigned length = 0;
        unsigned run_length = 0;
        bool firstNZDBX = false;
        bool secondNZDBX = false;
        for (int i=32; i>=0; i--) {
            if (DBX[i]==0) {
                run_length++;
            }
            else {
                if (run_length>0) {
                    countPattern(run_length-1);
                    assert(run_length!=33);
                    length += ZRL_CODE_SIZE[run_length];
                }
                run_length = 0;

                if (DBP[i]==0) {
                    length += zeroDBPSize;
                    countPattern(34);
                } else if (DBX[i]==0x7fffffff) {
                    length += allOneSize;
                    countPattern(35);
                } else {
                    int oneCnt = 0;
                    for (int j=0; j<32; j++) {
                        if ((DBX[i]>>j)&1) {
                            oneCnt++;
                        }
                    }
                    unsigned two_distance = 0;
                    int firstPos = -1;
                    if (oneCnt<=2) {
                        for (int j=0; j<32; j++) {
                            if ((DBX[i]>>j)&1) {
                                if (firstPos==-1) {
                                    firstPos = j;
                                } else {
                                    two_distance = j - firstPos;
                                }
                            }
                        }
                    }
                    if (oneCnt==1) {
                        length += singleOneSize;
                        countPattern(37+firstPos);
                    } else if ((oneCnt==2) && (two_distance==1)) {
                        length += consecutiveDoubleOneSize;
                        countPattern(69+firstPos);
                    } else {
                        length += 32;
                        countPattern(36);
                    }
                }
            }
        }
        if (run_length>0) {
            length += ZRL_CODE_SIZE[run_length];
            countPattern(run_length-1);
            assert(run_length<=33);
        }
        return length;
    }
};

#endif /* __BP_COMPRESSOR_HH__ */
