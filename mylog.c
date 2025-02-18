#include "mylog.h"
#include "stdlib.h"
#include "fcntl.h"
#include "string.h"
#include "stdarg.h"
#include "stdbool.h"

MyLogger *MyLogger__create(char *filename)
{
    printf("Creating Logger -> sizeof = %lu bytes\n", sizeof( MyLogger ) );
    MyLogger *_logger = malloc( sizeof( MyLogger ));
    _logger->logfile = fopen( filename, "w" );
    return _logger;
}

void MyLogger_flush( MyLogger *self )
{
    // self->buffer[self->buffer_ind] = '\0';
    fputs(self->buffer, self->logfile );
    // snprintf( self->buffer, LOG_BUFFER_LENGTH, "%s", self->buffer );
    self->buffer_ind = 0;
}

void MyLogger_error( MyLogger *self, char *msg )
{
    // concat to buffer, by shifting pointer to index
    // should overwrite the null termination
    char * cursor = self->buffer + self->buffer_ind;
    self->buffer_ind += snprintf( cursor, 
        LOG_BUFFER_LENGTH - self->buffer_ind,
        RED "ERROR:" RESET "%s\n", msg );
}

void MyLogger_success( MyLogger *self, char *msg )
{
    // concat to buffer, by shifting pointer to index
    // should overwrite the null termination
    char * cursor = self->buffer + self->buffer_ind;
    self->buffer_ind += snprintf( cursor, 
        LOG_BUFFER_LENGTH - self->buffer_ind,
        GREEN "OK:" RESET "%s\n", msg );
}

void MyLogger_trace( MyLogger *self, char *msg )
{
    // concat to buffer, by shifting pointer to index
    // should overwrite the null termination
    char * cursor = self->buffer + self->buffer_ind;
    self->buffer_ind += snprintf( cursor, 
        LOG_BUFFER_LENGTH - self->buffer_ind,
        TAB "%s\n", msg );
}

void MyLogger_logInt( MyLogger *self, char *msg, int val )
{
    // concat to buffer, by shifting pointer to index
    // should overwrite the null termination
    char * cursor = self->buffer + self->buffer_ind;
    self->buffer_ind += snprintf( cursor, 
        LOG_BUFFER_LENGTH - self->buffer_ind,
        TAB "%s : %i\n", msg, val );
}

void MyLogger_logStr( MyLogger *self, char *msg, char *val )
{
    // concat to buffer, by shifting pointer to index
    // should overwrite the null termination
    char * cursor = self->buffer + self->buffer_ind;
    self->buffer_ind += snprintf( cursor, 
        LOG_BUFFER_LENGTH - self->buffer_ind,
        TAB "%s : %s\n", msg, val );
}

void MyLogger_destroy( MyLogger *self )
{
    MyLogger_flush(self);
    fclose(self->logfile);
    free(self);
}
