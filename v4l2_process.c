
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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

#include <linux/videodev2.h>

#include "camviewer.h"

void set_camera(struct camera *cam, const char *devname, io_method io,
		unsigned int width, unsigned int height, unsigned int fps)
{
	cam->name       = devname;
	cam->fd         = -1;
	cam->width      = width;
	cam->height     = height;
	cam->n_buffers  = 0;
	cam->n_frames   = 4;
	cam->fps        = fps;
	cam->framebufs  = NULL;
	cam->io         = io;
}

void frame_dqbuf(struct camera *cam)
{
	memset(&cam->buf, 0, sizeof(cam->buf));

	cam->buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	cam->buf.memory = V4L2_MEMORY_MMAP;

	if (-1 == xioctl(cam->fd, VIDIOC_DQBUF, &(cam->buf))) {
		switch (errno) {
		case EAGAIN:
			return ;

		case EIO:
			/* Could ignore EIO, see spec. */

			/* fall through */

		default:
			errno_exit("VIDIOC_DQBUF");
		}
	}

	assert(cam->buf.index < cam->n_buffers);

#ifdef DEBUG
	printf("deque buffer %d\n", cam->buf.index);
#endif
}


void frame_qbuf(struct camera *cam)
{
	cam->buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	cam->buf.memory = V4L2_MEMORY_MMAP;

	if (-1 == xioctl(cam->fd, VIDIOC_QBUF, &(cam->buf)))
		errno_exit("VIDIOC_QBUF");

#ifdef DEBUG
      printf("queue buffer %d\n", cam->buf.index);
#endif
}

void open_device(struct camera *cam)
{
	struct stat st;

	if (-1 == stat(cam->name, &st)) {
		fprintf(stderr, "Cannot identify '%s': %d, %s\n",
				cam->name, errno, strerror(errno));
		exit(EXIT_FAILURE);
	}

	if (!S_ISCHR(st.st_mode)) {
		fprintf(stderr, "%s is no device\n", cam->name);
		exit(EXIT_FAILURE);
	}

	cam->fd = open(cam->name, O_RDWR | O_NONBLOCK, 0);

	if (-1 == cam->fd) {
		fprintf(stderr, "Cannot open '%s': %d, %s\n",
				cam->name, errno, strerror(errno));
		exit(EXIT_FAILURE);
	}
}

void close_device(struct camera *cam)
{
	if (-1 == close(cam->fd)) {
		errno_exit("close");
	}

	cam->fd = -1;
}

void start_capturer(struct camera *cam)
{
	unsigned int i;
	enum v4l2_buf_type type;
	struct v4l2_buffer buf;

	for (i = 0; i < cam->n_buffers; ++i) {
		memset(&cam->buf, 0, sizeof(cam->buf));

		buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		buf.memory = V4L2_MEMORY_MMAP;
		buf.index = i;

		if (-1 == xioctl(cam->fd, VIDIOC_QBUF, &buf))
			errno_exit("VIDIOC_QBUF");
	}

	type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

	if (-1 == xioctl(cam->fd, VIDIOC_STREAMON, &type))
		errno_exit("VIDIOC_STREAMON");
}

void stop_capturer(struct camera *cam)
{
	enum v4l2_buf_type type;

	type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

	if (-1 == xioctl(cam->fd, VIDIOC_STREAMOFF, &type))
		errno_exit("VIDIOC_STREAMOFF");
}

void v4l2_init_mmap(struct camera *cam)
{
	struct v4l2_requestbuffers req;
	struct v4l2_buffer buf;

	memset(&req, 0, sizeof(req));
	memset(&buf, 0, sizeof(buf));

	req.count = cam->n_frames;
	req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	req.memory = V4L2_MEMORY_MMAP;

	if (-1 == xioctl(cam->fd, VIDIOC_REQBUFS, &req)) {
		if (EINVAL == errno) {
			fprintf(stderr, "%s does not support "
					"memory mapping\n", cam->name);
			exit(EXIT_FAILURE);
		} else {
			errno_exit("VIDIOC_REQBUFS");
		}
	}

	if (req.count < 2) {
		fprintf(stderr, "Insufficient buffer memory on %s\n", cam->name);
		exit(EXIT_FAILURE);
	}

	cam->framebufs = (struct frame_buffer *)calloc(req.count, sizeof(*(cam->framebufs)));

	if (!cam->framebufs) {
		fprintf(stderr, "Out of memory\n");
		exit(EXIT_FAILURE);
	}

	for (cam->n_buffers = 0; cam->n_buffers < req.count; ++cam->n_buffers) {
		memset(&buf, 0, sizeof(buf));

		buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		buf.memory = V4L2_MEMORY_MMAP;
		buf.index = cam->n_buffers;

		if (-1 == xioctl(cam->fd, VIDIOC_QUERYBUF, &buf))
			errno_exit("VIDIOC_QUERYBUF");

		cam->framebufs[cam->n_buffers].length = buf.length;
		cam->framebufs[cam->n_buffers].start = mmap(NULL, buf.length,
				PROT_READ | PROT_WRITE, MAP_SHARED,
				cam->fd, buf.m.offset);

		if (MAP_FAILED == cam->framebufs[cam->n_buffers].start)
			errno_exit("mmap");
	}
}

void v4l2_uninit_mmap(struct camera *cam)
{
	unsigned int i;

	for (i = 0; i < cam->n_buffers; ++i) {
		if (-1 == munmap(cam->framebufs[i].start, cam->framebufs[i].length)) {
			errno_exit("munmap");
		}
	}

	free(cam->framebufs);
}

void v4l2_querycap(struct camera *cam)
{
	struct v4l2_capability cap;

	if (-1 == xioctl(cam->fd, VIDIOC_QUERYCAP, &cap)) {
		if (EINVAL == errno) {
			fprintf(stderr, "%s is no V4L2 device\n", cam->name);
			exit(EXIT_FAILURE);
		} else {
			errno_exit("VIDIOC_QUERYCAP");
		}
	}

	if (!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE)) {
		fprintf(stderr, "%s is no video capture device\n", cam->name);
		exit(EXIT_FAILURE);
	}

	if (!(cap.capabilities & V4L2_CAP_STREAMING)) {
		fprintf(stderr, "%s does not support streaming i/o\n", cam->name);
		exit(EXIT_FAILURE);
	}

//	memcpy(cam->name, cap.card, sizeof(cap.card));
}

void v4l2_setfmt(struct camera *cam, unsigned int pixfmt)
{
	struct v4l2_format fmt;

	memset(&fmt, 0, sizeof(fmt));;

	fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	fmt.fmt.pix.width = cam->width;
	fmt.fmt.pix.height = cam->height;
	fmt.fmt.pix.pixelformat = pixfmt;
	fmt.fmt.pix.field = V4L2_FIELD_INTERLACED;

	if (-1 == xioctl(cam->fd, VIDIOC_S_FMT, &fmt))
		errno_exit("VIDIOC_S_FMT");
}

void v4l2_getfmt(struct camera *cam)
{
	struct v4l2_format fmt;

	if (-1 == xioctl(cam->fd, VIDIOC_G_FMT, &fmt)) {
		errno_exit("Unable to get format\n");
	}

	printf("\033[33mpix.pixelformat:\t%c%c%c%c\n\033[0m",
			fmt.fmt.pix.pixelformat & 0xFF,
			(fmt.fmt.pix.pixelformat >> 8) & 0xFF,
			(fmt.fmt.pix.pixelformat >> 16) & 0xFF,
			(fmt.fmt.pix.pixelformat >> 24) & 0xFF);
	printf("pix.height:\t\t%d\n", fmt.fmt.pix.height);
	printf("pix.width:\t\t%d\n", fmt.fmt.pix.width);
	printf("pix.field:\t\t%d\n", fmt.fmt.pix.field);

	return ;
}

void v4l2_setfps(struct camera *cam)
{
	struct v4l2_streamparm streamparm;

	streamparm.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	streamparm.parm.capture.timeperframe.numerator = 1;
	streamparm.parm.capture.timeperframe.denominator = cam->fps;

	if (-1 == xioctl(cam->fd, VIDIOC_S_PARM, &streamparm)) {
		errno_exit("Unable to set framerate\n");
	}
}
