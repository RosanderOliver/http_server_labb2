CC = gcc
LD = gcc
INC_DIR = include
SRC_DIR = src
OBJ_DIR = objects
CFLAGS = -g -Wall -I$(INC_DIR)
LDFLAGS = 
SRCS = $(SRC_DIR)/main.c $(SRC_DIR)/configparser.c $(SRC_DIR)/setpageinfo.c $(SRC_DIR)/netio.c $(SRC_DIR)/logger.c
OBJS = $(OBJ_DIR)/main.o $(OBJ_DIR)/configparser.o $(OBJ_DIR)/setpageinfo.o $(OBJ_DIR)/netio.o $(OBJ_DIR)/logger.o
PROG = server.exe
RM = /bin/rm

all: $(PROG)

$(PROG): $(OBJS)
	$(LD) -D_GNU_SOURCE $(LDFLAGS) $(OBJS) -o $(PROG)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	$(CC) -D_GNU_SOURCE $(CFLAGS) -c $< -o $@

clean:
	$(RM) $(PROG) $(OBJS)
