#ifndef MM_TUI_TRANSPORT
#define MM_TUI_TRANSPORT

typedef enum CommandType {
    MM_CMD_PLAY,
    MM_CMD_PAUSE,
    MM_CMD_STOP,
    MM_CMD_JUMP_TO_TICK,
    MM_CMD_SET_BPM
} CommandType;

typedef struct MM_AudioCommand {
    CommandType cmd_type;
    unsigned int cmd_data;
} MM_AudioCommand;

#endif