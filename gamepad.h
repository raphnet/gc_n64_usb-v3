#ifndef _gamepad_h__
#define _gamepad_h__

typedef struct {
	int num_reports;

	int reportDescriptorSize;
	void *reportDescriptor; // must be in flash

	int deviceDescriptorSize; // if 0, use default
	void *deviceDescriptor; // must be in flash
	
	void (*init)(void);
	char (*update)(void);
	char (*changed)(int id);
	int (*buildReport)(unsigned char *buf, int id);
	void (*setVibration)(int value);

	/* Check for the controller */
	char (*probe)(void); /* return true if found */
} Gamepad;

#endif // _gamepad_h__


