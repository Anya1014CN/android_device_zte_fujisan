/*
 * Fujisan bring-up helper: solid-fill secondary framebuffer (fb1).
 * Proves B panel can show content even before HWC multi-display exists.
 */
#include <errno.h>
#include <stdint.h>
#include <fcntl.h>
#include <linux/fb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>

int main(int argc, char **argv)
{
	const char *path = "/dev/graphics/fb1";
	unsigned argb = 0xFF00FF00; /* green opaque */
	int fd;
	struct fb_var_screeninfo vinfo;
	struct fb_fix_screeninfo finfo;
	size_t size;
	void *map;
	uint32_t *px;
	size_t i, count;

	if (argc > 1)
		argb = (unsigned)strtoul(argv[1], NULL, 0);

	fd = open(path, O_RDWR);
	if (fd < 0) {
		perror("open fb1");
		return 1;
	}

	if (ioctl(fd, FBIOGET_VSCREENINFO, &vinfo) < 0) {
		perror("FBIOGET_VSCREENINFO");
		return 1;
	}
	if (ioctl(fd, FBIOGET_FSCREENINFO, &finfo) < 0) {
		perror("FBIOGET_FSCREENINFO");
		return 1;
	}

	/* Prefer a known panel size if driver reports 0. */
	if (vinfo.xres == 0)
		vinfo.xres = 1080;
	if (vinfo.yres == 0)
		vinfo.yres = 1920;
	if (vinfo.bits_per_pixel == 0)
		vinfo.bits_per_pixel = 32;
	vinfo.activate = FB_ACTIVATE_NOW;
	if (ioctl(fd, FBIOPUT_VSCREENINFO, &vinfo) < 0)
		perror("FBIOPUT_VSCREENINFO");

	if (ioctl(fd, FBIOGET_FSCREENINFO, &finfo) < 0) {
		perror("FBIOGET_FSCREENINFO2");
		return 1;
	}

	size = finfo.smem_len;
	if (size == 0)
		size = (size_t)vinfo.xres * vinfo.yres * 4;

	map = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	if (map == MAP_FAILED) {
		perror("mmap");
		/* Fallback: write() path */
		{
			uint32_t *buf = malloc(size);
			if (!buf)
				return 1;
			count = size / 4;
			for (i = 0; i < count; i++)
				buf[i] = argb;
			if (write(fd, buf, size) < 0)
				perror("write");
			free(buf);
		}
	} else {
		px = map;
		count = size / 4;
		for (i = 0; i < count; i++)
			px[i] = argb;
		msync(map, size, MS_SYNC);
		/* Pan to force update on some MDSS builds */
		if (ioctl(fd, FBIOPAN_DISPLAY, &vinfo) < 0)
			perror("FBIOPAN_DISPLAY");
		munmap(map, size);
	}

	/* Unblank */
	{
		int blank = FB_BLANK_UNBLANK;
		if (ioctl(fd, FBIOBLANK, blank) < 0)
			perror("FBIOBLANK");
	}

	fprintf(stderr, "fb1 fill argb=0x%08x xres=%u yres=%u bpp=%u smem=%u\n",
		argb, vinfo.xres, vinfo.yres, vinfo.bits_per_pixel,
		(unsigned)finfo.smem_len);
	close(fd);
	return 0;
}
