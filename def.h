struct in6_addr {
	union {
		uint8_t  Byte[16];
		uint16_t Word[8];
	} u;
};

#ifndef true
#define true 1
#define false 0
#endif


#ifndef WIN32

//#ifdef __BYTE_ORDER__
//  #if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    #define HTONS(x) ((uint16_t) (((uint16_t) (x) << 8) | ((uint16_t) (x) >> 8)))
    #define HTONL(x) ((uint32_t) (((uint32_t) HTONS(x) << 16) | HTONS((uint32_t) (x) >> 16)))
//  #elif __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
 //   #define HTONS(x) ((uint16_t) (x))
 //   #define HTONL(x) ((uint32_t) (x))
 // #else
 //   #error Byte order not supported!
 // #endif
//#else
//  #error Byte order not defined!
//#endif
 
#define NTOHS(x) HTONS(x)
#define NTOHL(x) HTONL(x)
 
static inline uint16_t htons(uint16_t x) {
  return HTONS(x);
}
static inline uint32_t htonl(uint32_t x) {
  return HTONL(x);
}
static inline uint16_t ntohs(uint16_t x) {
  return htons(x);
}
static inline uint32_t ntohl(uint32_t x) {
  return htonl(x);
}

/*
#if __BYTE_ORDER == __BIG_ENDIAN
#define htons(a)	(a)
#define htonl(a)	(a)
#define ntohs(a)	(a)
#define ntohl(a)	(a)
#else
#error Little endian not implemented
#endif /// BIG ENDIAN 
*/


typedef uint32_t time_t;
#define difftime(A,B) (A-B)
extern uint32_t time_1s;
#define time(A) time_1s
#endif	//WIN32

#ifdef __GNUC__
#define PACKED( class_to_pack ) class_to_pack __attribute__((__packed__))
#else
#define PACKED( class_to_pack ) __pragma( pack(push, 1) ) class_to_pack __pragma( pack(pop) )
#endif

#ifndef INET6_ADDRSTRLEN
#define INET6_ADDRSTRLEN 46
#endif
