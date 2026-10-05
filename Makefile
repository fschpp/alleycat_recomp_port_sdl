CC      := cc
CFLAGS  := -std=c11 -D_POSIX_C_SOURCE=199309L -Wall -Wextra -O2 -Iinclude $(shell pkg-config --cflags sdl2)
LDFLAGS := $(shell pkg-config --libs sdl2)

SRC := src/main.c src/cga.c src/speaker.c src/sound.c src/audio.c src/video.c src/input.c src/cat_state.c src/animation.c src/movement.c src/game_setup.c src/alley.c src/alley_movement.c src/level_collision.c src/throw.c src/alley_drawing.c src/enemy.c src/cycle_objects.c src/jump_gravity.c src/fall_object.c src/score.c src/level_background.c src/level3_enemy.c src/level_objects.c src/level45_state.c src/level4.c src/level5.c src/level7_epilogue.c src/gen_cat_walk_frames.c src/gen_cat_alley_walk_frames.c src/gen_cat_gap1_sprites.c src/gen_death_sprite.c src/gen_object_sprites.c src/gen_level_geometry.c src/gen_enemy_sprites.c src/gen_obj_hit_sprites.c src/gen_enemy_verified_sprites.c src/gen_fall_sprite.c src/gen_digit_sprites.c src/gen_ds_pool.c
OBJ := $(SRC:src/%.c=build/%.o)
BIN := build/alleycat

all: $(BIN)

$(BIN): $(OBJ)
	$(CC) $(OBJ) -o $@ $(LDFLAGS)

build/%.o: src/%.c include/*.h
	@mkdir -p build
	$(CC) $(CFLAGS) -c $< -o $@

run: $(BIN)
	./$(BIN)

clean:
	rm -rf build

TEST_SRC := $(filter-out src/main.c src/video.c src/audio.c src/input.c,$(SRC))
build/test_alley: tests/test_alley_loop.c $(TEST_SRC) include/*.h
	@mkdir -p build
	$(CC) $(CFLAGS) tests/test_alley_loop.c $(TEST_SRC) -o $@

test-alley: build/test_alley
	./build/test_alley

build/test_l3doors: tests/test_level3_doors.c $(TEST_SRC) include/*.h
	@mkdir -p build
	$(CC) $(CFLAGS) -Isrc tests/test_level3_doors.c $(TEST_SRC) -o $@

test-l3doors: build/test_l3doors
	./build/test_l3doors

build/test_level4: tests/test_level4.c $(TEST_SRC) include/*.h
	@mkdir -p build
	$(CC) $(CFLAGS) -Wl,--wrap=restore_alley_buffer,--wrap=save_alley_buffer,--wrap=start_tone tests/test_level4.c $(TEST_SRC) -o $@

test-level4: build/test_level4
	./build/test_level4

build/test_level4_state: tests/test_level4_state.c $(TEST_SRC) include/*.h
	@mkdir -p build
	$(CC) $(CFLAGS) tests/test_level4_state.c $(TEST_SRC) -o $@

test-level4-state: build/test_level4_state
	./build/test_level4_state

build/test_level5: tests/test_level5.c $(TEST_SRC) include/*.h
	@mkdir -p build
	$(CC) $(CFLAGS) tests/test_level5.c $(TEST_SRC) -o $@

test-level5: build/test_level5
	./build/test_level5

test: test-alley test-l3doors test-level4 test-level4-state test-level5

.PHONY: all run clean test test-alley test-l3doors test-level4 test-level4-state test-level5
