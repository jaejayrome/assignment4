CC= gcc800
OBJS = dynarray.o snush.o token.o execute.o util.o lexsyn.o
TARGET = snush
CFLAGS = -D_GNU_SOURCE -g -O3 -Wall -DNDEBUG --static
SUBDIRS = tools

.SUFFIXES : .c .o

all : $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS)
	$(foreach dir, $(SUBDIRS), $(MAKE) -C $(dir);)

clean :
	rm -f $(OBJS) $(TARGET)
	$(foreach dir, $(SUBDIRS), $(MAKE) -C $(dir) clean;)

upload:
	sshpass -p 'C^JvZP.J39' scp  -r -P 2222 ./ sp202481390@sp04.snucse.org:~/assignment4

enter:
	sshpass -p 'C^JvZP.J39' ssh sp202481390@sp04.snucse.org -p 2222