#ifndef _gamepads_h__
#define _gamepads_h__

#define PAD_TYPE_NONE       0
#define PAD_TYPE_N64        4
#define PAD_TYPE_GAMECUBE   5

#define N64_RAW_SIZE        4
#define GC_RAW_SIZE         8

typedef struct _n64_pad_data {
	unsigned char pad_type; // PAD_TYPE_N64
	char x,y;
	unsigned short buttons;
	unsigned char raw_data[N64_RAW_SIZE];
} n64_pad_data;

#define N64_BTN_A			0x8000
#define N64_BTN_B			0x4000
#define N64_BTN_Z			0x2000
#define N64_BTN_START		0x1000
#define N64_BTN_DPAD_UP		0x0800
#define N64_BTN_DPAD_DOWN	0x0400
#define N64_BTN_DPAD_LEFT	0x0200
#define N64_BTN_DPAD_RIGHT	0x0100
#define N64_BTN_RSVD_LOW1	0x0080
#define N64_BTN_RSVD_LOW2	0x0040
#define N64_BTN_L			0x0020
#define N64_BTN_R			0x0010
#define N64_BTN_C_UP		0x0008
#define N64_BTN_C_DOWN		0x0004
#define N64_BTN_C_LEFT		0x0002
#define N64_BTN_C_RIGHT		0x0001


typedef struct _gc_pad_data {
	unsigned char pad_type; // PAD_TYPE_GAMECUBE
	char x,y,cx,cy;
	unsigned char lt,rt;
	unsigned short buttons;
	unsigned char raw_data[GC_RAW_SIZE];
} gc_pad_data;

#define GC_BTN_A			0x0001
#define GC_BTN_B			0x0002
#define GC_BTN_X			0x0004
#define GC_BTN_Y			0x0008

#define GC_BTN_START		0x0010
#define GC_BTN_RSVD0		0x0020
#define GC_BTN_RSVD1		0x0040
#define GC_BTN_RSVD2		0x0080

#define GC_BTN_DPAD_LEFT	0x0100
#define GC_BTN_DPAD_RIGHT	0x0200
#define GC_BTN_DPAD_DOWN	0x0400
#define GC_BTN_DPAD_UP		0x0800

#define GC_BTN_Z			0x1000
#define GC_BTN_R			0x2000
#define GC_BTN_L			0x4000
#define GC_BTN_RSVD3		0x8000

#define GC_ALL_BUTTONS      (GC_BTN_START|GC_BTN_Y|GC_BTN_X|GC_BTN_B|GC_BTN_A|GC_BTN_L|GC_BTN_R|GC_BTN_Z|GC_BTN_DPAD_UP|GC_BTN_DPAD_DOWN|GC_BTN_DPAD_RIGHT|GC_BTN_DPAD_LEFT)

typedef struct _gamepad_data {
	union {
		unsigned char pad_type; // PAD_TYPE_*
		n64_pad_data n64;
		gc_pad_data gc;
	};
} gamepad_data;

typedef struct {
	void (*init)(void);
	char (*update)(void);
	char (*changed)(void);
	void (*getReport)(gamepad_data *dst);
	void (*setVibration)(char enable);
	char (*probe)(void); /* return true if found */
} Gamepad;

#endif // _gamepads_h__


