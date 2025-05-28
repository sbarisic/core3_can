#include <core3.h>
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

int main()
{
	core3_opsys_init();

	while (true) {

	}
}
