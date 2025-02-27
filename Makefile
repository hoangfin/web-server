
################################################################################
# COMPILATION
################################################################################
CXX				=	g++
CXX_STRICT		=	-Wall -Wextra -Werror -std=c++20 -Wno-pessimizing-move
DB_FLAGS		=	-g
HEADERS			=	-I $(INCLUDES)
CXX_FULL		=	$(CXX) $(CXX_STRICT) $(DB_FLAGS) $(HEADERS)

################################################################################
# MANDATORY
################################################################################
NAME			=	webserv
INCLUDES		=	./include
M_HEADERS		=	$(INCLUDES)/Config.hpp \
					$(INCLUDES)/Error.hpp \
					$(INCLUDES)/Router.hpp \
					$(INCLUDES)/ServerManager.hpp \
					$(INCLUDES)/utils/common.hpp \
					$(INCLUDES)/utils/index.hpp \
					$(INCLUDES)/utils/Payload.hpp \
					$(INCLUDES)/utils/socket.hpp \
					$(INCLUDES)/http/Connection.hpp \
					$(INCLUDES)/http/constants.hpp \
					$(INCLUDES)/http/data_types.hpp \
					$(INCLUDES)/http/index.hpp \
					$(INCLUDES)/http/parser.hpp \
					$(INCLUDES)/http/Request.hpp \
					$(INCLUDES)/http/Response.hpp \
					$(INCLUDES)/http/utils.hpp \
					$(INCLUDES)/Server.hpp \
					$(INCLUDES)/SignalHandle.hpp
 # Add more headers here

OBJ_DIR			=	obj
SRC_DIR			=	src
SRCS			=	main.cpp \
					Connection.cpp \
					parser.cpp \
					Request.cpp \
					Response.cpp \
					utils.cpp \
					Config.cpp \
					Server.cpp \
					ServerManager.cpp \
					common.cpp \
					FilePayload.cpp \
					Payload.cpp \
					socket.cpp \
					StringPayload.cpp \
					Router.cpp \
					Handlers.cpp \
					IsCGI.cpp \
					LocationMapper.cpp \
					SignalHandler.cpp

OBJECTS		:=	$(SRCS:%.cpp=$(OBJ_DIR)/%.o)

################################################################################
# RULES
################################################################################
vpath %.cpp	$(SRC_DIR) \
			$(SRC_DIR)/http \
			$(SRC_DIR)/parser \
			$(SRC_DIR)/server \
			$(SRC_DIR)/signal \
			$(SRC_DIR)/utils \
			$(SRC_DIR)/router

# Add more paths here

all: $(OBJ_DIR) $(NAME)

$(OBJ_DIR):
	@mkdir -p $(OBJ_DIR)

$(OBJ_DIR)/%.o: %.cpp $(M_HEADERS)
	@echo "Compiling $< to $@"
	@$(CXX_FULL) -c $< -o $@
	@echo "$(G)Compiled: $< $(RC)"

$(NAME): $(OBJ_DIR) $(OBJECTS)
	@echo "--------------------------------------------"
	@$(CXX_FULL) $(OBJECTS) -o $(NAME)
	@echo "[$(NAME)] $(B)Built target $(NAME)$(RC)"
	@echo "--------------------------------------------"

clean:
	@rm -rf $(NAME).dSYM/ $(OBJ_DIR)/
	@echo "[$(NAME)] Object files cleaned."

fclean: clean
	@rm -f $(NAME) $(LIB_NAME) $(TEST_NAME)
	@echo "[$(NAME)] Everything deleted."

re: fclean all
	@echo "[$(NAME)] Everything rebuilt."

################################################################################
# TEST
################################################################################
LIB_NAME        =   libwebserv.a
TEST_NAME       =   test_runner
TEST_DIR        =   ./test
TEST_SRCS       =   $(wildcard $(TEST_DIR)/*.cpp)
TEST_OBJECTS    =   $(TEST_SRCS:$(TEST_DIR)/%.cpp=$(OBJ_DIR)/%.o)

# Build the static library
$(LIB_NAME): $(OBJECTS)
	@echo "--------------------------------------------"
	@ar rcs $(LIB_NAME) $(OBJECTS)
	@echo "[$(LIB_NAME)] $(B)Built static library $(LIB_NAME)$(RC)"
	@echo "--------------------------------------------"

# Detect the operating system
UNAME_S := $(shell uname -s)

# Set Google Test paths based on the operating system
ifeq ($(UNAME_S), Darwin)
    GTEST_DIR := $(shell brew --prefix googletest 2>/dev/null || echo "/usr/local/opt/googletest")
else
    GTEST_DIR := /usr/local
endif

GTEST_HEADERS   =   -I$(GTEST_DIR)/include
GTEST_LIBS      =   -L$(GTEST_DIR)/lib -lgtest -lgtest_main -pthread

# If Google Test is not found, download and build it
ifeq ($(wildcard $(GTEST_DIR)/include/gtest/gtest.h),)
GTEST_DIR = ./googletest
GTEST_HEADERS = -I$(GTEST_DIR)/googletest/include
GTEST_LIBS = -L$(GTEST_DIR)/build/lib -lgtest -lgtest_main -pthread

$(GTEST_DIR):
	@echo "Google Test not found. Downloading and building Google Test..."
	@git clone https://github.com/google/googletest.git $(GTEST_DIR)
	@mkdir -p $(GTEST_DIR)/build
	@cd $(GTEST_DIR)/build && cmake .. && make
endif

test: $(GTEST_DIR) $(TEST_NAME)

$(TEST_NAME): $(TEST_OBJECTS) $(LIB_NAME)
	@echo "--------------------------------------------"
	@$(CXX) $(TEST_OBJECTS) -L. -lwebserv -o $(TEST_NAME) $(GTEST_LIBS)
	@echo "[$(TEST_NAME)] $(B)Built test target $(TEST_NAME)$(RC)"
	@echo "--------------------------------------------"

$(OBJ_DIR)/%.o: $(TEST_DIR)/%.cpp
	@mkdir -p $(OBJ_DIR)
	@echo "Compiling $< to $@"
	@$(CXX) $(CXX_STRICT) $(DB_FLAGS) $(GTEST_HEADERS) -c $< -o $@
	@echo "$(G)Compiled: $< $(RC)"

clean_test:
	@rm -rf $(TEST_NAME).dSYM/ $(OBJ_DIR)/test/
	@echo "[$(TEST_NAME)] Object files cleaned."

fclean_test: clean_test
	@rm -f $(TEST_NAME) $(LIB_NAME) $(TEST_NAME).dSYM/
	@echo "[$(TEST_NAME)] Everything deleted."

re_test: fclean_test test
	@echo "[$(TEST_NAME)] Everything rebuilt."

################################################################################
# PHONY
################################################################################
.PHONY: all re clean fclean
.PHONY: test clean_test fclean_test re_test

################################################################################
# Colors
################################################################################
# Green, Blue
R = \033[0;31m
B = \033[0;34m
# Reset Color
RC = \033[0m
