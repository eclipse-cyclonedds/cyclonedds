publisher_NAME := HelloworldPublisher
publisher_SRCS := publisher.c generated/HelloWorldData.c
publisher_OBJS := ${publisher_SRCS:.c=.o}

subscriber_NAME := HelloworldSubscriber
subscriber_SRCS := subscriber.c generated/HelloWorldData.c
subscriber_OBJS := ${subscriber_SRCS:.c=.o}

# Make sure that the location where dds.h is installed is known to the
# compiler. If dds.h is not installed in a system default spot, then
# it can be introduced by adding it to the CFLAGS, like:
#CFLAGS += -I../../../../include

# Make sure that the location where libddsc.so is installed is known
# to the compiler. If the lib is not installed in a system default
# location, then it can be introduced by adding it to the LDFLAGS:
#LDFLAGS += -L../../../../lib

CFLAGS += -Igenerated
LDFLAGS += -lddsc

.PHONY: all clean distclean

all: $(publisher_NAME) $(subscriber_NAME)

$(publisher_NAME): $(publisher_OBJS)
	gcc $(CFLAGS)  $(publisher_OBJS) $(LDFLAGS) -o $(publisher_NAME)

$(subscriber_NAME): $(subscriber_OBJS)
	gcc $(CFLAGS) $(subscriber_OBJS) $(LDFLAGS) -o $(subscriber_NAME)

clean:
	@- $(RM) $(publisher_NAME)
	@- $(RM) $(publisher_OBJS)
	@- $(RM) $(subscriber_NAME)
	@- $(RM) $(subscriber_OBJS)

distclean: clean



# Make sure that the idlc jar file is available and java can find its
# location. In this example, it is assumed that the jar is located at
# a specific relative directory. Change the classpath variable if
# this is not the case on your system.
classpath:= ../../idlc/idlc-jar-with-dependencies.jar
ifneq ($(CLASSPATH),)
        classpath:= $(CLASSPATH):$(classpath)
endif
export CLASSPATH:=$(classpath)

datatype:
	java org.eclipse.cyclonedds.compilers.Idlc HelloWorldData.idl

