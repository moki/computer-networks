TARGET = main.out
SRCS = $(wildcard ./*.c) $(wildcard ./*/*.c)
OBJS = $(SRCS: ./*/*.c=*.o)

CC = gcc

DEPDIR := .d
$(shell mkdir -p $(DEPDIR) >/dev/null)
DEPFLAGS = -MT $@ -MMD -MP -MF $(DEPDIR)/$*.Td
CFLAGS = -std=c99 -D_POSIX_C_SOURCE -Wshadow -Wall -pedantic -Wextra -g -O3 -flto -pthread
TARGET_ARCH =-march=native
LDFLAGS = -lpthread
# LDLIBS = -lm

$(TARGET): $(OBJS)
	$(CC) $(OBJS) $(LDFLAGS) $(LDLIBS) -o $@

COMPILE.c = $(CC) $(DEPFLAGS) $(CFLAGS) $(CPPFLAGS) $(TARGET_ARCH) -c
POSTCOMPILE = @mv -f $(DEPDIR)/$*.Td $(DEPDIR)/$*.d && touch $@

%.o : %.c
%.o : %.c $(DEPDIR)/%.d
	$(COMPILE.c) $(OUTPUT_OPTION) $<
	$(POSTCOMPILE)

clean:
	-rm -f ./*.o ./*/*.o
	-rm -f ./*.out

$(DEPDIR)/%.d: ;
.PRECIOUS: $(DEPDIR)/%.d

include $(wildcard $(patsubst %,$(DEPDIR)/%.d,$(basename $(SRCS))))
