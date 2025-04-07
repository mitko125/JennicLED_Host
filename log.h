#define LOG_EMERG	0
#define LOG_ALERT	1
#define LOG_CRIT	2
#define LOG_ERR		3
#define LOG_WARNING	4
#define LOG_NOTICE	5
#define LOG_INFO	6
#define LOG_DEBUG	7

/* The log macros filter out log messages that have a higher log level than that set in LOG_LEVEL */
#ifdef UART_DEBUG
/* When logging via UART, we don't print the level */
#define daemon_log(STREAM, LEVEL, FORMAT, ARGS...)  DBG_vPrintf((STREAM && (LEVEL <= LOG_LEVEL)), FORMAT, ##ARGS)

#else
/* When logging via Serial link to host syslog, send the log level as a char integer at the start of the message */
#define QUOTE(A) #A
#define CHAR(A) QUOTE(\x##A)
#define daemon_log( LEVEL, FORMAT,...) if (verbosity >= LEVEL) {putchar(LEVEL+'0');printf( FORMAT, ##__VA_ARGS__); putchar('\n');}
#endif

extern int verbosity;