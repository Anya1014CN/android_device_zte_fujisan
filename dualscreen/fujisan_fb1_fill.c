/*
 * Fujisan L1 helper: solid-fill secondary framebuffer and keep it alive.
 * Usage:
 *   fujisan_fb1_fill [argb]           # fill once, keep fd open forever
 *   fujisan_fb1_fill [argb] once      # fill + pan once, exit (panel stays on)
 */
#include <errno.h>
#include <fcntl.h>
#include <linux/fb.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>

#ifndef MSMFB_DISPLAY_COMMIT
#define MSMFB_IOCTL_MAGIC 'm'
#define MSMFB_DISPLAY_COMMIT _IOW(MSMFB_IOCTL_MAGIC, 164, void *)
#endif

struct mdp_display_commit {
	uint32_t flags;
	uint32_t wait_for_finish;
	struct fb_var_screeninfo var;
	struct {
		int32_t x;
		int32_t y;
		int32_t w;
		int32_t h;
	} l_roi, r_roi;
};

static int kick_display(int fd, struct fb_var_screeninfo *vinfo)
{
	struct mdp_display_commit commit;
	int ret;

	memset(&commit, 0, sizeof(commit));
	commit.flags = 0;
	commit.wait_for_finish = 1;
	commit.var = *vinfo;
	commit.var.activate = FB_ACTIVATE_VBL;
	ret = ioctl(fd, MSMFB_DISPLAY_COMMIT, &commit);
	if (ret == 0)
		return 0;

	/* Fallbacks */
	vinfo->activate = FB_ACTIVATE_NOW | FB_ACTIVATE_FORCE;
	ret = ioctl(fd, FBIOPAN_DISPLAY, vinfo);
	if (ret == 0)
		return 0;
	return ret;
}

int main(int argc, char **argv)
{
	const char *path = "/dev/graphics/fb1";
	unsigned argb = 0xFF00AA00; /* teal-ish opaque */
	int once = 0;
	int fd;
	struct fb_var_screeninfo vinfo;
	struct fb_fix_screeninfo finfo;
	size_t size, count, i;
	void *map;
	uint32_t *px;
	int blank;

	if (argc > 1 && argv[1][0] != '\0')
		argb = (unsigned)strtoul(argv[1], NULL, 0);
	if (argc > 2 && strcmp(argv[2], "once") == 0)
		once = 1;

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

	if (vinfo.xres == 0)
		vinfo.xres = 1080;
	if (vinfo.yres == 0)
		vinfo.yres = 1920;
	if (vinfo.bits_per_pixel == 0)
		vinfo.bits_per_pixel = 32;
	if (vinfo.xres_virtual < vinfo.xres)
		vinfo.xres_virtual = vinfo.xres;
	if (vinfo.yres_virtual < vinfo.yres * 2)
		vinfo.yres_virtual = vinfo.yres * 2;
	vinfo.xoffset = 0;
	vinfo.yoffset = 0;
	vinfo.activate = FB_ACTIVATE_NOW;
	ioctl(fd, FBIOPUT_VSCREENINFO, &vinfo);
	ioctl(fd, FBIOGET_FSCREENINFO, &finfo);
	ioctl(fd, FBIOGET_VSCREENINFO, &vinfo);

	blank = FB_BLANK_UNBLANK;
	if (ioctl(fd, FBIOBLANK, blank) < 0)
		perror("FBIOBLANK");

	size = finfo.smem_len;
	if (size == 0)
		size = (size_t)vinfo.xres_virtual * vinfo.yres_virtual *
		       (vinfo.bits_per_pixel / 8);

	map = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	if (map == MAP_FAILED) {
		perror("mmap");
		return 1;
	}

	px = map;
	count = size / 4;
	for (i = 0; i < count; i++)
		px[i] = argb;
	msync(map, size, MS_SYNC);

	if (kick_display(fd, &vinfo) < 0)
		perror("kick_display");

	/* Also paint second buffer page if double-buffered */
	if (vinfo.yres_virtual >= vinfo.yres * 2) {
		vinfo.yoffset = vinfo.yres;
		for (i = 0; i < count; i++)
			px[i] = argb;
		msync(map, size, MS_SYNC);
		if (kick_display(fd, &vinfo) < 0)
			perror("kick_display2");
		vinfo.yoffset = 0;
		kick_display(fd, &vinfo);
	}

	fprintf(stderr,
		"fb1 fill argb=0x%08x xres=%u yres=%u bpp=%u smem=%u once=%d\n",
		argb, vinfo.xres, vinfo.yres, vinfo.bits_per_pixel,
		(unsigned)finfo.smem_len, once);

	if (once) {
		munmap(map, size);
		/* leave fd close; kernel keeps secondary powered */
		close(fd);
		return 0;
	}

	/* Stay resident so MDP resources remain owned if needed. */
	for (;;) {
		/* Re-kick occasionally in case idle collapse clears the panel. */
		sleep(5);
		kick_display(fd, &vinfo);
	}

	return 0;
}
