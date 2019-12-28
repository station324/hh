
cc -Wall -Werror -Wno-unused-variable -Wno-unused-but-set-variable -Wno-unused-function -DHANDMADE_SLOW=1 -DHANDMADE_INTERNAL=1 -std=gnu11 `sdl2-config --cflags --libs` -lm -ldl -g -o exec sd.c
cc -Wall -Werror -Wno-unused-variable -Wno-unused-but-set-variable -Wno-unused-function -DHANDMADE_SLOW=1 -DHANDMADE_INTERNAL=1 -std=gnu11 -fpic -c -lm -g  hm.c
cc -shared -o hm.so hm.o
