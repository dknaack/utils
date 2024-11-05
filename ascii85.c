#include <stdio.h>
#include <stdint.h>

int
main(void)
{
	for (;;) {
		char src[4] = {0};
		size_t count = fread(src, 1, sizeof(src), stdin);
		if (count == 0) {
			break;
		}

		uint32_t num = (src[0] << 24) | (src[1] << 16) | (src[2] << 8) | src[3];
		if (num == 0) {
			fputc('z', stdout);
		} else {
			char dst[5] = {0};
			dst[4] = num % 85 + 33;
			num /= 85;
			dst[3] = num % 85 + 33;
			num /= 85;
			dst[2] = num % 85 + 33;
			num /= 85;
			dst[1] = num % 85 + 33;
			num /= 85;
			dst[0] = num % 85 + 33;
			fwrite(dst, 1, count + 1, stdout);
		}
	}

	return 0;
}
