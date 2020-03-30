

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <assert.h>
#include <malloc.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <fcntl.h>
#include <pthread.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <getopt.h>

#include <SDL2/SDL.h>
#include <SDL2/SDL_events.h>
#include <linux/videodev2.h>

#include "camviewer.h"



static bool stop_frame_proc = false;

struct stream_handler {
	SDL_monitor    *monitor;
	struct camera  *cam;
	void (*framehandler)(SDL_monitor *monitor);
};

static void usage(FILE *fp, int argc, char **argv)
{
	fprintf(fp, "Usage: %s [options]\n\n"
			"Options:\n"
			"-d | --device name   Video device name [/dev/video]\n"
			"-h | --help          Print this message\n"
			"-x | --width         Video width\n"
			"-y | --height        Video height\n"
			"-f | --fps           Video frame rates\n"
			"\n"
			"operation keys:\n"
			"\tq and Esc: quit program.\n"
			"\tr:         revert to startup resolution.\n"
			"",
			argv[0]);
}

static const char short_options[] = "d:hmrux:y:f:";

static const struct option long_options[] = {
	{ "device", required_argument, NULL, 'd' },
	{ "help",   no_argument,       NULL, 'h' },
	{ "mmap",   no_argument,       NULL, 'm' },
	{ "width",  required_argument, NULL, 'x' },
	{ "height", required_argument, NULL, 'y' },
	{ "fps",    required_argument, NULL, 'f' },
	{ 0, 0, 0, 0 }
};

static void * frame_process(void *arg)
{
	int ret;
	fd_set fds;
	struct timeval tv;

	SDL_monitor *monitor = ((struct stream_handler *)(arg))->monitor;
	struct camera  *cam = ((struct stream_handler *)(arg))->cam;
	void (*handler)(SDL_monitor *monitor) = ((struct stream_handler *)(arg))->framehandler;

	while (!stop_frame_proc) {
		FD_ZERO(&fds);
		FD_SET(cam->fd, &fds);

		/* Timeout. */
		tv.tv_sec = 2;
		tv.tv_usec = 0;

		ret = select(cam->fd + 1, &fds, NULL, NULL, &tv);

		if (-1 == ret) {
			if (EINTR == errno)
				continue;

			errno_exit("select");
		}

		if (0 == ret) {
			fprintf(stderr, "select timeout\n");
			exit(EXIT_FAILURE);
		}

		if (FD_ISSET(cam->fd, &fds)) {
			frame_dqbuf(cam);
			monitor->pixels = cam->framebufs[cam->buf.index].start;
			if (handler)
				(*handler)(monitor);
			frame_qbuf(cam);
		}
	}
	return NULL;
}

static void process_image(SDL_monitor *monitor)
{
	SDL_FrameRenderer(monitor);

	// save pixel format to file. ?
	// save_pix_to_file(pframe);
}

static bool handle_events(SDL_monitor *monitor)
{
    SDL_Event event;

    while (SDL_PollEvent(&event)) {
        switch (event.type) {
        case SDL_KEYDOWN:
        	switch (event.key.keysym.sym) {
        	case SDLK_q:
        	case SDLK_ESCAPE: // press ESC the quit
        		return true;

        	case SDLK_r:
        		monitor->width  = monitor->r_width;
        		monitor->height = monitor->r_height;
        		SDL_SetWindowSize(monitor->window, monitor->r_width, monitor->r_height);
        		break;
        	//case other:
        	//
        	}

            break;

        case SDL_WINDOWEVENT:
        	SDL_GetWindowSize(monitor->window, &(monitor->width), &(monitor->height));
        	break;

        case SDL_QUIT:
            return true;
        }
    }

    return false;
}

int main(int argc, char **argv)
{
	int index, opt;
	int width = 640, height = 480, fps = 30;
	io_method io = IO_METHOD_MMAP;
	const char *devname = "/dev/video0";
    SDL_monitor *monitor = NULL;

	struct camera *camera = malloc(sizeof(*camera));
	memset(camera, 0, sizeof(*camera));

	while ((opt = getopt_long(argc, argv, short_options, long_options, &index)) != -1) {
		switch (opt) {
		case 0: /* getopt_long() flag */
			break;

		case 'd':
			devname = optarg;
			break;

		case 'h':
			usage(stdout, argc, argv);
			exit(EXIT_SUCCESS);

		case 'm':
			io = IO_METHOD_MMAP;
			break;

		case 'r':
			io = IO_METHOD_READ;
			break;

		case 'x':
			width = atoi(optarg);
			break;

		case 'y':
			height = atoi(optarg);
			break;

		case 'f':
			fps = atoi(optarg);
			break;

		case '?':
			usage(stderr, argc, argv);
			exit(EXIT_FAILURE);

		default:
			fprintf(stderr, "?? getopt returned character code 0%o ??\n", opt);
		}
	}

	atexit(SDL_Quit);

	set_camera(camera, devname, io, width, height, fps);

	open_device(camera);

	v4l2_querycap(camera);

	v4l2_setfmt(camera, V4L2_PIX_FMT_YUYV);

	v4l2_getfmt(camera);

	v4l2_setfps(camera);

	v4l2_init_mmap(camera);

	start_capturer(camera);

    monitor = SDL_CreateContext(handle_events, width, height);
    if (monitor == NULL) {
        fprintf(stderr, "Could not create the SDL context\n");
        return -1;
    }

	int err = 0;
	pthread_t frame_procces_id;

	struct stream_handler sh = {
		.monitor = monitor,
		.cam = camera,
		.framehandler = process_image
	};
	err = pthread_create(&frame_procces_id, NULL, frame_process, (void *)&sh);
	if (err != 0) {
		fprintf(stderr, "pthread_create failed...\n");
		exit(EXIT_FAILURE);
	}

	SDL_MainLoop(monitor);

	stop_frame_proc = true;
	pthread_join(frame_procces_id, NULL);

	stop_capturer(camera);

	v4l2_uninit_mmap(camera);

	close_device(camera);

	SDL_DeleteContext(monitor);

	return 0;
}

