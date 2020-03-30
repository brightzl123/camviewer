

#ifndef _CAMVIEWER_H_
#define _CAMVIEWER_H_

#include <stdbool.h>
#include <SDL2/SDL.h>
#include <linux/videodev2.h>

//#define CLEAR(x) memset(&(x), 0, sizeof(x))

#define max(a, b) ((a) > (b) ? (a) : (b))
#define min(a, b) ((a) > (b) ? (b) : (a))

#ifndef bool
typedef enum {
	false,
	true,
} bool;
#endif

typedef enum {
	IO_METHOD_READ,
	IO_METHOD_MMAP,
	IO_METHOD_USERPTR,
} io_method;

struct frame_buffer {
	void         *start;
	unsigned int  length;
};

struct camera {
	const char  *name;
	int          fd;
	unsigned int width;
	unsigned int height;
	unsigned int n_buffers;
	unsigned int n_frames;
	unsigned int fps;
	struct frame_buffer *framebufs;
	struct v4l2_buffer   buf;
	io_method    io;
};

typedef struct SDL_monitor {
    SDL_Window   *window;
    SDL_Renderer *renderer;
    SDL_Texture  *texture;
    SDL_Rect      rect;
    const void   *pixels;
    int           width;
    int           height;
    int           r_width;
    int           r_height;
    void          (*update)(struct SDL_monitor *monitor);
    void          (*draw  )(struct SDL_monitor *monitor);
    bool          (*events)(struct SDL_monitor *monitor);
} SDL_monitor;

void errno_exit(const char *s);
int xioctl(int fd, int request, void *arg);

void set_camera(struct camera *cam, const char *devname, io_method io,
		unsigned int width, unsigned int height, unsigned int fps);
void open_device(struct camera *cam);
void close_device(struct camera *cam);
void frame_dqbuf(struct camera *cam);
void frame_qbuf(struct camera *cam);
void stop_capturer(struct camera *cam);
void start_capturer(struct camera *cam);
void v4l2_uninit_mmap(struct camera *cam);
void v4l2_init_mmap(struct camera *cam);
void v4l2_querycap(struct camera *cam);
void v4l2_getfmt(struct camera *cam);
void v4l2_setfmt(struct camera *cam, unsigned int pixfmt);
void v4l2_setfps(struct camera *cam);


typedef bool (*SDL_monitorEventsFunc)(SDL_monitor *monitor);
//typedef void (*SDL_monitorUpdateFunc)(SDL_monitor *monitor);
//typedef void (*SDL_monitorDrawFunc  )(SDL_monitor *monitor);

SDL_monitor * SDL_CreateContext(SDL_monitorEventsFunc events, int width, int height);
void SDL_MainLoop(SDL_monitor *monitor);
void SDL_DeleteContext(SDL_monitor *monitor);
void SDL_FrameRenderer(SDL_monitor *monitor);

#endif /* _CAMVIEWER_H_ */
