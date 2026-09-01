# Makefile - C17/Win32 Markdown Editor build.
# Toolchain: MinGW-w64 (UCRT) located in the repository "toolchain/" directory.
# Override TC to point elsewhere if needed.

TC      := D:/0901-workbuddy-markdown-editor/toolchain/mingw64
CC      := $(TC)/bin/gcc.exe
AR      := $(TC)/bin/ar.exe
WINDRES := $(TC)/bin/windres.exe

BUILD   := build
SRC     := src

CFLAGS  := -std=gnu17 -O2 -Wall -Wextra -Wno-unused-parameter -Wno-sign-compare -Wno-redeclaration -Wno-shadow \
           -D_WIN32_WINNT=0x0A00 -DUNICODE -D_UNICODE \
           -I$(SRC)/core -I$(SRC)/engine -I$(SRC)/app -I$(SRC)/tools

# Windows subsystem + Unicode entry for the GUI app.
GUI_CFLAGS := $(CFLAGS) -municode -mwindows

# Console utilities also use -municode to receive Unicode argv via wmain.
CLI_CFLAGS := $(CFLAGS) -municode

LIBS     := -lgdi32 -luser32 -lole32 -loleaut32 -lwindowscodecs -lshell32 \
            -lshlwapi -limm32 -lcomdlg32 -luuid -ladvapi32

# ---- source collection ----
CORE_SRC  := $(wildcard $(SRC)/core/*.c)
ENGINE_SRC:= $(wildcard $(SRC)/engine/*.c)
CORE_OBJ  := $(patsubst $(SRC)/core/%.c,$(BUILD)/core/%.o,$(CORE_SRC))
ENGINE_OBJ:= $(patsubst $(SRC)/engine/%.c,$(BUILD)/engine/%.o,$(ENGINE_SRC))

# ---- phony ----
.PHONY: all clean tools tests editor core

all: tools editor

# ---------------------------------------------------------------- core lib
$(BUILD)/core/%.o: $(SRC)/core/%.c
	@mkdir -p $(BUILD)/core
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/libcore.a: $(CORE_OBJ)
	$(AR) rcs $@ $(CORE_OBJ)

# ---------------------------------------------------------------- engine lib
$(BUILD)/engine/%.o: $(SRC)/engine/%.c
	@mkdir -p $(BUILD)/engine
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/libengine.a: $(ENGINE_OBJ)
	$(AR) rcs $@ $(ENGINE_OBJ)

core: $(BUILD)/libcore.a
engine: $(BUILD)/libengine.a

# ---------------------------------------------------------------- tools
$(BUILD)/%.o: $(SRC)/tools/%.c
	@mkdir -p $(BUILD)
	$(CC) $(CLI_CFLAGS) -c $< -o $@

$(BUILD)/locscan.exe: $(BUILD)/locscan.o $(BUILD)/libcore.a
	$(CC) $(CLI_CFLAGS) -o $@ $^ $(LIBS)

$(BUILD)/fixturegen.exe: $(BUILD)/fixturegen.o $(BUILD)/libcore.a
	$(CC) $(CLI_CFLAGS) -o $@ $^ $(LIBS)

$(BUILD)/evidencecheck.exe: $(BUILD)/evidencecheck.o $(BUILD)/libcore.a
	$(CC) $(CLI_CFLAGS) -o $@ $^ $(LIBS)

tools: $(BUILD)/locscan.exe $(BUILD)/fixturegen.exe $(BUILD)/evidencecheck.exe

# ---------------------------------------------------------------- editor
$(BUILD)/app/%.o: $(SRC)/app/%.c
	@mkdir -p $(BUILD)/app
	$(CC) $(GUI_CFLAGS) -c $< -o $@

APP_SRC := $(wildcard $(SRC)/app/*.c)
APP_OBJ := $(patsubst $(SRC)/app/%.c,$(BUILD)/app/%.o,$(APP_SRC))

$(BUILD)/mdeditor.exe: $(APP_OBJ) $(BUILD)/libengine.a $(BUILD)/libcore.a
	$(CC) $(GUI_CFLAGS) -o $@ $^ $(LIBS)

editor: $(BUILD)/mdeditor.exe

# ---------------------------------------------------------------- tests
$(BUILD)/tests/%.o: tests/%.c
	@mkdir -p $(BUILD)/tests
	$(CC) $(CLI_CFLAGS) -c $< -o $@

TEST_SRC := $(wildcard tests/*.c)
TEST_OBJ := $(patsubst tests/%.c,$(BUILD)/tests/%.o,$(TEST_SRC))

tests: $(BUILD)/testrunner.exe

$(BUILD)/testrunner.exe: $(TEST_OBJ) $(BUILD)/libengine.a $(BUILD)/libcore.a
	$(CC) $(CLI_CFLAGS) -o $@ $^ $(LIBS)

clean:
	rm -rf $(BUILD)
