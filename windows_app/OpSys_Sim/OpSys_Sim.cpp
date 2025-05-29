#include <core3.h>
#include <core3_map.h>
#include <core3_opsys.h>
#include <opsys_sim.h>

#include <Windows.h>

int pdMS_TO_TICKS(int ms) {
	return ms;
}

void vTaskDelay(size_t ticks) {
	Sleep(ticks);
}

uint32_t esp_random() {
	return 0;
}

core3_map_t* create_map(uint8_t element_size, int x, int y) {
	uint16_t* xaxis = (uint16_t*)malloc(x * sizeof(uint16_t));
	uint16_t* yaxis = (uint16_t*)malloc(y * sizeof(uint16_t));

	for (size_t xx = 0; xx < x; xx++) {
		xaxis[xx] = xx + 1;
	}

	for (size_t yy = 0; yy < y; yy++) {
		yaxis[yy] = yy + 1;
	}

	core3_map_t* map = core3_map_create(element_size, xaxis, x, yaxis, y);
	return map;
}

int main() {
	size_t memory_len = 4096;
	uint8_t* memory = (uint8_t*)malloc(memory_len);
	memset(memory, 0xFF, memory_len);

	FILE* maps_file = fopen("maps.bin", "rb");
	if (maps_file != NULL) {
		fread((void*)memory, sizeof(uint8_t), memory_len, maps_file);
		fclose(maps_file);
		maps_file = NULL;
	}

	const int maps_count = 16;
	core3_map_t* maps[maps_count];
	size_t maps_used = 0;
	core3_map_deserialize_all((const void*)memory, maps, maps_count, &maps_used);

	//maps_used = 0;
	if (maps_used != 3) {
		maps[maps_used++] = create_map(sizeof(int16_t), 2, 2);
		maps[maps_used++] = create_map(sizeof(int32_t), 6, 1);
		maps[maps_used++] = create_map(sizeof(uint8_t), 8, 8);
	}

	for (size_t i = 0; i < maps_used; i++) {
		core3_map_t* mp = maps[i];

		for (size_t y = 0; y < mp->y.len; y++) {
			for (size_t x = 0; x < mp->x.len; x++) {
				uint32_t val = core3_map_get_element(mp, x + 1, y + 1);

				val = 0x78563412;

				core3_map_set_element(mp, x + 1, y + 1, val);
			}
		}
	}


	memset(memory, 0xFF, 4096);
	size_t bytes_written = core3_map_serialize_all((void*)memory, maps, maps_used);

	maps_file = fopen("maps.bin", "wb");
	fwrite((const void*)memory, sizeof(uint8_t), memory_len, maps_file);
	fflush(maps_file);
	fclose(maps_file);
	maps_file = NULL;

	//core3_opsys_init();
}
