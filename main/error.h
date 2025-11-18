#ifndef ERROR_H
#define ERROR_H

/**
 * Initialize error handler (LED)
 */
void error_init(void);

/**
 * Halt system with error indication
 * 
 * @param message Error message to log
 */
void error_halt(const char *message) __attribute__((noreturn));

#endif // ERROR_H
