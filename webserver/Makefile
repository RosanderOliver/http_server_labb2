CC = gcc
LD = gcc
INC_DIR = include
SRC_DIR = src
OBJ_DIR = objects
CFLAGS = -g -I$(INC_DIR)
LDFLAGS = 
SRCS = $(SRC_DIR)/main.c $(SRC_DIR)/configparser.c $(SRC_DIR)/setpageinfo.c $(SRC_DIR)/netio.c
OBJS = $(OBJ_DIR)/main.o $(OBJ_DIR)/configparser.o $(OBJ_DIR)/setpageinfo.o $(OBJ_DIR)/netio.o
PROG = server.exe
RM = /bin/rm

all: $(PROG)

$(PROG): $(OBJS)
	$(LD) $(LDFLAGS) $(OBJS) -o $(PROG)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	$(RM) $(PROG) $(OBJS)
