#ifndef _C2C_PLATFORM_H_
#define _C2C_PLATFORM_H_

class C2CPlatform
{
public:
    C2CPlatform(){};
    ~C2CPlatform(){};
    static constexpr int REQ_SOUR_ID    = 0;
    static constexpr int REQ_DEST_ID    = 1;
    static constexpr int REQ_IS_ACK     = 2;
    static constexpr int REQ_IS_LAST    = 3;
    static constexpr int REQ_SIZE       = 4;
    static constexpr int REQ_WRITE      = 5;
    static constexpr int REQ_PORT_ID    = 6;
    static constexpr int REQ_NB_ARGS    = 7;
};

#endif
