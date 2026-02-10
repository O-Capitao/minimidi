#ifndef MM_TUI_TRANSPORT
#define MM_TUI_TRANSPORT

typedef enum CommandType {
    MM_CMD_PLAY,
    MM_CMD_PAUSE,
    MM_CMD_STOP,
    MM_CMD_BACK_TO_BEGINING
} CommandType;

typedef struct MM_AudioCommand {
    CommandType cmd_type;
} MM_AudioCommand;

#endif