
#ifndef _BUFFER_H_
#define _BUFFER_H_
// general routines to handle a single fifo buffer
// just book-keeping. accessing the databuffer needs to be done manually!
// => works for all kind of elements

// WARNING: not reentrant. if used from multiple threads/interrupts make sure
// only one of them is writing and only one of them is reading (no mixing!)
// if in doubt, use a buffer for each direction! and CRITICAL_SECTION_*

/* naming scheme for a circular buffer called <BUFFER>

    declaration in .h files:
    BUFFER_H(<BUFFER>, TYPE, SIZE)

    definition in one .c file
    BUFFER(<BUFFER>, TYPE, SIZE)

    this will declare/define the following elements

    variables:
    <BUFFER>_head, <BUFFER>_tail store indices (max 8bit wide!)
    <BUFFER>_data the data storage

    functions:
    void <BUFFER>_init(void) sets <BUFFER>_head and <BUFFER>_tail to 0
    bool <BUFFER>_is_empty(void) returns TRUE if buffer is empty, else FALSE
    bool <BUFFER>_is_full(void)    returns TRUE if buffer is full, else FALSE
    bool <BUFFER>_can_read(void) returns !<BUFFER>_is_empty()
    bool <BUFFER>_can_write(void) returns !<BUFFER>_is_full()
    void <BUFFER>_push(void) advances head by one (write your data to index <BUFFER>_head first)
                             this will block if there is no free index
    void <BUFFER>_pop(void) advances tail by one (read your data from index <BUFFER>_tail first!)
                            also only call this if <BUFFER>_can_read() is TRUE!
                            (If you over-read, the whole buffer of stale data is seen as valid again!)
    void <BUFFER>_used(void) returns the number of used indices.
    TYPE <BUFFER>_get(void) blocks while buffer is empty, then pops one element and returns it.
                            may only work correctly where TYPE is copyable by assignment
    void <BUFFER>_put(TYPE) Just writes an element into the buffer and push (so it may block)

 */

// define allowed buffer sizes
#define BUFFER_SIZE_2    2
#define BUFFER_SIZE_4    4
#define BUFFER_SIZE_8    8
#define BUFFER_SIZE_16    16
#define BUFFER_SIZE_32    32
#define BUFFER_SIZE_64    64
#define BUFFER_SIZE_128    128
#define BUFFER_SIZE_256    256
#define BUFFER_SIZE_512    512
#define BUFFER_SIZE_1024   1024
#define BUFFER_SIZE_2048   2048

// call this while waiting....
extern void idle(void);

// anlegen eines buffers in C-files (Data + Functions)
#define BUFFER(NAME, TYPE, SIZE) \
    volatile unsigned int NAME ## _head = 0; \
    volatile unsigned int NAME ## _tail = 0; \
    TYPE    NAME ## _data[SIZE]; \
    void NAME ## _init(void){NAME ## _head = NAME ## _tail = 0;}\
    bool NAME ## _is_empty(void) {return NAME ## _head == NAME ## _tail;} \
    bool NAME ## _can_read(void) {return ! NAME ## _is_empty();} \
    bool NAME ## _is_full(void) {return ((NAME ## _head +1) & (SIZE-1)) == NAME ## _tail;} \
    bool NAME ## _can_write(void) {return ! NAME ## _is_full();} \
    void NAME ## _push(void) {while ( NAME ## _is_full()) {idle();}; NAME ## _head = ((NAME ## _head + 1) & (SIZE-1));} \
    void NAME ## _pop(void) {NAME ## _tail = (( NAME ## _tail +1) & (SIZE-1));} \
    uint8_t NAME ## _used(void) { return (NAME ## _head - NAME ## _tail) & (SIZE-1);} \
    TYPE NAME ## _get(void){TYPE c; while ( NAME ## _is_empty()) {idle();};c= NAME ## _data[ NAME ## _tail]; NAME ## _pop(); return c;} \
    void NAME ## _put(TYPE c){ NAME ## _data[ NAME ## _head]=c; NAME ## _push();}

// anlegen eines buffers in H-files (extern Data refs. + protos)
#define BUFFER_H(NAME, TYPE, SIZE) \
    void NAME ## _init(void);\
    bool NAME ## _is_empty(void); \
    bool NAME ## _can_read(void); \
    bool NAME ## _is_full(void); \
    bool NAME ## _can_write(void); \
    void NAME ## _push(void); \
    void NAME ## _pop(void); \
    uint8_t NAME ## _used(void); \
    TYPE NAME ## _get(void); \
    void NAME ## _put(TYPE c);


/*
BUFFER(BNAME, BTYPE, BSIZE) ==>

volatile uint8_t BNAME_head = 0;
volatile uint8_t BNAME_tail = 0;
BTYPE BNAME_data[BSIZE];


void BNAME_init(void) {
    BNAME_head = BNAME_tail = 0;
}

bool BNAME_is_empty(void) {
    return BNAME_head == BNAME_tail;
}

bool BNAME_can_read(void) {
    return ! BNAME_is_empty();
}

bool BNAME_is_full(void) {
    return ((BNAME_head +1) & (BSIZE-1)) == BNAME_tail;
}

bool BNAME_can_write(void) {
    return ! BNAME_is_full();
}

void BNAME_push(void) {
    while (BNAME_is_full())
        idle('B');
    BNAME_head = ((BNAME_head + 1) & (BSIZE-1));
}

void BNAME_pop(void) {
    BNAME_tail = ((BNAME_tail +1) & (BSIZE-1));
}

uint8_t BNAME_used(void) {
    return (BNAME_head - BNAME_tail) & (BSIZE-1);
}

BTYPE BNAME_get(void) {
    BTYPE c;
    while (BNAME_is_empty())
        idle('B');
    c= BNAME_data[BNAME_tail];
    BNAME_pop();
    return c;
}

void BNAME_put(BTYPE c) {
    BNAME_data[BNAME_head]=c;
    BNAME_push();
}

*/


#endif
