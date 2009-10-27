#ifndef __IMPULSE_H__
#define __IMPULSE_H__


// ZImpulseArg - Extra argument passed to impulse handler, makes it possible to define a single
// handler shared to some related impulses (move_left, move_right, etc).
typedef enum ZImpulseArg
{
    Z_IMPARG_NONE,
    Z_IMPARG_LEFT,
    Z_IMPARG_RIGHT,
    Z_IMPARG_UP,
    Z_IMPARG_DOWN,
    Z_IMPARG_FORWARD,
    Z_IMPARG_BACK,
    Z_IMPARG_IN,
    Z_IMPARG_OUT,
    Z_NUM_IMPULSEARGS

} ZImpulseArg;

// When an impulse handler is called, it gets a start or stop state.
#define Z_IMPULSE_START 1
#define Z_IMPULSE_STOP  0



typedef void (*ZImpulseHandler)(ZImpulseArg arg, unsigned int state);


// ZImpulse - a simple action exposed to the user. When a key bound to an impulse is pressed or
// released, the handler is called and passed arg. If the impulse is marked as PRESS_ONLY, the
// handler is called only for key presses (saves the handler from having to check state).
typedef struct ZImpulse
{
    char *name;
    unsigned int press_only;
    ZImpulseHandler handler;
    ZImpulseArg arg;
    char *desc;

} ZImpulse;

extern ZImpulse impulses[];



ZImpulse *zLookupImpulse(unsigned int n);

unsigned int zImpulseCount(void);

#endif
