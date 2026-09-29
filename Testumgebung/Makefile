SRC=../Waage
TST=.
LV=lvgl
CFL=-O1 -g -I. -I$(LV) -I$(SRC) -DLV_CONF_INCLUDE_SIMPLE -DSIM_WAAGE=1 -DUSE_BLE_KBD=0 -w
LVSRC=$(shell find $(LV)/src -name '*.c')
LVOBJ=$(patsubst $(LV)/src/%.c,obj/lvgl/%.o,$(LVSRC))
APPC=$(filter-out hal_board.cpp net.cpp web.cpp update_online.cpp settings.cpp storage.cpp voice.cpp mic.cpp,$(notdir $(wildcard $(SRC)/*.cpp)))
APPO=$(patsubst %.cpp,obj/%.o,$(APPC)) $(patsubst $(SRC)/%.c,obj/%.o,$(wildcard $(SRC)/font_*.c))
all: sim
obj/lvgl/%.o: $(LV)/src/%.c lv_conf.h
	@mkdir -p $(dir $@); gcc $(CFL) -c $< -o $@
obj/%.o: $(SRC)/%.cpp $(wildcard $(SRC)/*.h)
	@mkdir -p obj; g++ $(CFL) -c $< -o $@
obj/%.o: $(SRC)/%.c
	@mkdir -p obj; gcc $(CFL) -c $< -o $@
obj/stubs.o: $(TST)/stubs_host.cpp $(wildcard $(SRC)/*.h)
	g++ $(CFL) -c $< -o $@
obj/hal_host.o: hal_host.cpp $(wildcard $(SRC)/*.h)
	g++ $(CFL) -c $< -o $@
obj/main_tour.o: main_tour.cpp $(wildcard $(SRC)/*.h)
	g++ $(CFL) -c $< -o $@
sim: $(LVOBJ) $(APPO) obj/stubs.o obj/hal_host.o obj/main_tour.o
	g++ -o $@ $^ -lm
