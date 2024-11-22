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
