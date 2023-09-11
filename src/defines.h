#ifndef NBX_DEFINES_H_
#define NBX_DEFINES_H_

#define likely(x)   __builtin_expect(!!(x),1)
#define unlikely(x) __builtin_expect(!!(x),0)

//= log
#define MAX_LENGTH_OF_ONE_LOG                4095 
#define MAX_FILE_NAME_LENGTH                 255

#define CR                  '\r'
#define LF                  '\n'
#define LOWER(c)            (unsigned char)(c | 0x20)
#define IS_ALPHA(c)         (LOWER(c) >= 'a' && LOWER(c) <= 'z')
#define IS_NUM(c)           ((c) >= '0' && (c) <= '9')
#define IS_ALPHANUM(c)      (IS_ALPHA(c) || IS_NUM(c))
#define IS_HEX(c)           (IS_NUM(c) || (LOWER(c) >= 'a' && LOWER(c) <= 'f'))
#define TOKEN(c)            tokens[(unsigned char)c]

#endif // NBX_DEFINES_H_
