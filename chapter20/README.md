A simple chat program.

Run chat_server.c and multiple chat_client.c to see the effect.

There is one thing interesting. It's needed to `undef max` to aviod the max(a, b) macro in minwindef.h:
```
#ifndef max
#define max(a, b) (((a) > (b)) ? (a) : (b))
#endif
```
Else getRandomName() would get a wrong length string(for example 0). It's because `max(3, rand() % NAME_ARR_SIZE)` is expanded to `(((3) > (rand() % 6)) ? (3) : (rand() % 6))`. Rand() is called twice. The conditional judgement phase rand() value is different from the final rand() value. This results from c macro characteristic. I feel that macro is not a good characteristic. This example showed the potential danger of macro in defining function.

To undef the max macro, add `undef max` in the head (`undef max(a, b)` also works but gets a compiler warning).

Need to run `chcp 936` in advance to set encoding to GBK and run with command line (like chapter 9). Else the Chinese characters in message will grable.