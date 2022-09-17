/*
 *
 * Video 4 Linux V2 driver library (camera utility).
 *
 *  Copyright (C) 2015 Hiroshi Kuwagata. All rights reserved.
 *
 */

/*
 * $Id: camera.c 158 2017-10-26 08:51:55Z pi $
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <strings.h>
#include <memory.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/stat.h>

#include "camera.h"

#ifdef RUBY_EXTLIB
#include <ruby.h>
#else /* defined(RUBY_EXTLIB) */
#include <sys/select.h>
#endif /* defined(RUBY_EXTLIB) */

#define BZERO(x)                  bzero(&x, sizeof(x))
#define HEXDUMP(x)                hexdump(&x, sizeof(x))
#define N(x)                      (sizeof(x)/sizeof(*x))
#define DBG_LOG(...)              fprintf(stderr, __VA_ARGS__)

#define DEFAULT_FORMAT            V4L2_PIX_FMT_MJPEG
#define DEFAULT_WIDTH             640
#define DEFAULT_HEIGHT            480
#define DEFAULT_FRAMERATE_NUM     30
#define DEFAULT_FRAMERATE_DENOM   1

#define NEED_CAPABILITY           (V4L2_CAP_VIDEO_CAPTURE|V4L2_CAP_STREAMING)
#define IS_CAPABLE(x)             (((x) & NEED_CAPABILITY) == NEED_CAPABILITY)

#define NUM_PLANE                 3
#define CAMERA_UP_PERIOD          15
#define COPIED                    0x8000
                                  
#define ST_ERROR                  (-1)
#define ST_NONE                   (0) /* fialized */
#define ST_INITIALIZED            (1)
#define ST_PREPARE                (2)
#define ST_READY                  (3)
#define ST_REQUESTED              (4)
#define ST_BREAK                  (5)
#define ST_STOPPING               (6)
#define ST_FINALIZED              (7)

static int xioctl(int fh, unsigned long request, void *arg)
{
  int r;

  do {
    r = ioctl(fh, request, arg);
  } while (r == -1 && EINTR == errno);

  return r;
}

static int
device_open(char* path, int *_fd, char* name, char* driver, char* bus)
{
  int ret;
  int err;
  int fd;
  struct v4l2_capability cap;

  do {
    /*
     * entry process
     */
    ret = !0;
    fd  = -1;

    /*
     * device open
     */
    fd = open(path, O_RDWR);
    if (fd < 0) {
      perror("open()");
      break;
    }

    /*
     * get device capability info
     */
    err = xioctl(fd, VIDIOC_QUERYCAP, &cap);
    if (err < 0) {
      perror("ioctl(VIDIOC_QUERYCAP)");
      break;
    }

    /*
     * check capability
     */
    if (!IS_CAPABLE(cap.capabilities)) {
      fprintf(stderr, "This device not have camera function that required.");
      break;
    }

    /*
     * set return parameters
     */
    strncpy( name, (char*)cap.card, N(cap.card));
    strncpy( driver, (char*)cap.driver, N(cap.driver));
    strncpy( bus, (char*)cap.bus_info, N(cap.bus_info));

    *_fd = fd;

    /*
     * mark succeed
     */
    ret = 0;

  } while(0);

  /*
   * fallback for error occured
   */
  if (ret) {
    if (fd >= 0) close(fd);
  }

  return ret;
}

static int
set_format(int fd, uint32_t fcc, int wd, int ht)
{
  int ret;
  int err;
  struct v4l2_format fmt;

  ret = 0;

  BZERO(fmt);

  fmt.type                = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  fmt.fmt.pix.width       = wd;
  fmt.fmt.pix.height      = ht;
  fmt.fmt.pix.pixelformat = fcc;
  fmt.fmt.pix.field       = V4L2_FIELD_INTERLACED;

  err = xioctl(fd, VIDIOC_S_FMT, &fmt);
  if (err < 0) {
    perror("ioctl(VIDIOC_S_FMT)");
    ret = !0;
  }

  return ret;
}

static int
set_param(int fd, int num, int denom)
{
  int ret;
  int err;
  struct v4l2_streamparm param;
  struct v4l2_fract* tpf;

  ret = 0;

  BZERO(param);
  param.type       = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  tpf              = &param.parm.capture.timeperframe;
  tpf->numerator   = num;
  tpf->denominator = denom;

  err = xioctl(fd, VIDIOC_S_PARM, &param);
  if (err < 0) {
    perror("ioctl(VIDIOC_S_PARM:V4L2_BUF_TYPE_VIDEO_CAPTURE)");
    ret = !0;
  }

  return ret;
}

static int
mb_init(mblock_t* mb, int fd, struct v4l2_buffer* buf)
{
  int ret;
  void* ptr;

  ret = 0;
  ptr = mmap(NULL, buf->length, PROT_READ, MAP_SHARED, fd, buf->m.offset);

  if (ptr != NULL) {
    mb->ptr  = ptr;
    mb->size = buf->length;
  } else{
    ret = !0;
  }

  return ret;
}

static void
mb_copyto(mblock_t* src, void* dst, size_t* used)
{
  memcpy(dst, src->ptr, src->used);
  *used = src->used;
}

static void
mb_discard(mblock_t* mb)
{
  if (mb->ptr != NULL) {
    munmap(mb->ptr, mb->size);
    mb->ptr = NULL;
  }
}

static int
request_buffer(int fd, int n, mblock_t* mb)
{
  int ret;
  int err;
  int i;

  struct v4l2_requestbuffers req;
  struct v4l2_buffer buf;

  /*
   * initialize
   */
  ret = 0;

  bzero(mb, sizeof(*mb) * n);

  /*
   * body
   */
  do {
    /*
     * request for camera buffer allocation
     */
    BZERO(req);

    req.count  = n;
    req.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_MMAP;

    err = xioctl(fd, VIDIOC_REQBUFS, &req);
    if (err < 0) {
      perror("ioctl(VIDIOC_REQBUFS)");
      ret = !0;
      break;
    }

    /*
     * get camera buffer and mapping to user area
     */
    for (i = 0; i < n; i++) {
      BZERO(buf);

      buf.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
      buf.memory = V4L2_MEMORY_MMAP;
      buf.index  = i;

      err = xioctl(fd, VIDIOC_QUERYBUF, &buf);
      if (err < 0) {
        perror( "ioctl(VIDIOC_QUERYBUF)");
        ret = !0;
        break;
      }

      err = mb_init(mb + i, fd, &buf);
      if (err) {
        ret = !0;
        break;
      }
    }

    if (ret) break;

  } while (0);

  /*
   * post process
   */
  if (ret) {
    for (i = 0; i < n; i++) {
      if (mb[i].ptr != NULL) mb_discard(mb + i);
    }
  }

  return ret;
}

static int
enqueue_buffer(int fd, int i)
{
  int err;
  int ret;
  struct v4l2_buffer buf;

  ret = 0;

  BZERO(buf);

  buf.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  buf.memory = V4L2_MEMORY_MMAP;
  buf.index  = i;

  err = xioctl(fd, VIDIOC_QBUF, &buf);
  if (err) {
    perror("ioctl(VIDIOC_QBUF)");
    ret = !0;
  }

  return ret;
}

static int
start(int fd)
{
  int ret;
  int err;
  enum v4l2_buf_type typ;

  ret = 0;
  typ = V4L2_BUF_TYPE_VIDEO_CAPTURE;

  err = xioctl(fd, VIDIOC_STREAMON, &typ);
  if (err < 0) {
    perror("ioctl(VIDIOC_STREAMON)");
    ret = !0;
  }

  return ret;
}

static int
stop(int fd)
{
  int ret;
  int err;
  enum v4l2_buf_type typ;

  ret = 0;
  typ = V4L2_BUF_TYPE_VIDEO_CAPTURE;

  err = xioctl(fd, VIDIOC_STREAMOFF, &typ);
  if (err < 0) {
    perror("ioctl(VIDIOC_STREAMOFF)");
    ret = !0;
  }

  return ret;
}

static int
query_captured_buffer(int fd, int* plane, size_t* used)
{
  int ret;
  int err;
  struct v4l2_buffer buf;

  ret = 0;

  BZERO(buf);

  buf.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  buf.memory = V4L2_MEMORY_MMAP;

  err = xioctl(fd, VIDIOC_DQBUF, &buf);
  if (err < 0) {
      perror( "ioctl(VIDIOC_DQBUF)");
      ret = !0;
  }

  if (!ret) {
    *plane = buf.index;
    *used  = buf.bytesused;
  }

  return ret;
}

static int
update_image_size(camera_t* cam)
{
  int ret;
  size_t size;

  ret = 0;

  switch (cam->format) {
  case V4L2_PIX_FMT_NV21:
    size = (cam->width * cam->height * 3) / 2;
    break;

  case V4L2_PIX_FMT_YUYV:
  case V4L2_PIX_FMT_NV16:
  case V4L2_PIX_FMT_RGB565:
    size = cam->width * cam->height * 2;
    break;

  case V4L2_PIX_FMT_MJPEG:
    size = cam->width * cam->height;
    break;

  case V4L2_PIX_FMT_H264:
    size = cam->width * cam->height * 3;
    break;

  default:
    fprintf(stderr, "update_image_size():unknown image format.\n");
    ret = !0;
  }

  if (!ret) cam->image_size = size;

  return ret;
}

static VALUE
wait_fd(VALUE arg)
{
  camera_t* cam = (camera_t*)arg;

  rb_thread_wait_fd(cam->fd);

  return Qundef;
}

#ifdef RUBY_EXTLIB
static void
capture_frame(camera_t* cam)
{
  int err;
  int plane;
  size_t used;
  int state;

  errno = 0;

	// これはwarning対応
	used  = 0;
	plane = 0;

  rb_protect(wait_fd, (VALUE)cam, &state);

  if (state == 8) {
    cam->state = ST_ERROR;
    rb_jump_tag(state);

  } else if (state != 0) {
    cam->state = ST_READY;
    rb_jump_tag(state);

  } else if (errno == EINTR) {
    cam->state = ST_BREAK;

  } else {
    do {
      err = query_captured_buffer(cam->fd, &plane, &used);
      if (err) {
        cam->state = ST_ERROR;
        break;
      }

      cam->mb[plane].used = used;

      if (cam->latest >= 0) {
        err = enqueue_buffer(cam->fd, cam->latest & ~COPIED);
        if (err) {
          cam->state = ST_ERROR;
          break;
        }
      }

      cam->latest = plane;

    } while(0);
  }
}
#else /* defined(RUBY_EXTLIB_ */
static void
capture_frame(camera_t* cam)
{
  int err;
  int plane;
  size_t used;
  int state;
  fd_set fds;
  size_t used;

  do {
		// これはwarning対応
		used  = 0;
		plane = 0;

    FD_ZERO(&fds);
    FD_SET(cam->fd, &fds);
    select(cam->fd + 1, &fds, NULL, NULL, NULL);

    if (errno == EINTR) continue;

		err = query_captured_buffer(cam->fd, &plane, &used);
		if (err) {
			cam->state = ST_ERROR;
			break;
		}

		cam->mb[plane].used = used;

		if (cam->latest >= 0) {
			err = enqueue_buffer(cam->fd, cam->latest & ~COPIED);
			if (err) {
				cam->state = ST_ERROR;
				break;
			}
		}

		cam->latest = plane;
	} while(0);
}
#endif /* defined(RUBY_EXTLIB_ */

/*
 * ここからパブリックな関数
 */

int
camera_initialize(camera_t* cam, char* dev)
{
  int ret;
  int err;

  do {
    /*
     * entry process
     */
    ret = !0;

    /*
     * check arguments
     */
    if (cam == NULL) break;
    if (strnlen(dev, N(cam->device)) >= N(cam->device)) break;

    /*
     * initailize context data
     */
    bzero(cam, sizeof(*cam));
    cam->fd = -1;

    /*
     * camera open
     */
    err = device_open(dev, &cam->fd, cam->name, cam->driver, cam->bus);
    if (err) break;

    /*
     * initialize camera context
     */
    strcpy(cam->device, dev);

    cam->format          = DEFAULT_FORMAT;
    cam->width           = DEFAULT_WIDTH;
    cam->height          = DEFAULT_HEIGHT;
    cam->framerate.num   = DEFAULT_FRAMERATE_NUM;
    cam->framerate.denom = DEFAULT_FRAMERATE_DENOM;
                        
    cam->state           = ST_INITIALIZED;
    cam->latest          = -1;

    update_image_size(cam);

    /*
     * mark succeed
     */
    ret = 0;

  } while (0);

  /*
   * fallback for error occured
   */
  if (ret) {
    if (cam->fd >= 0) close(cam->fd);
  }

  return ret;
}

int
camera_get_format_desc(camera_t* cam, int i, struct v4l2_fmtdesc* dst)
{
  int ret;
  int err;

  do {
    /*
     * entry process
     */
    ret = !0;

    /*
     * check argument
     */
    if (cam == NULL) break;
    if (dst == NULL) break;

    /*
     * do query
     */
    memset(dst, 0, sizeof(*dst));

    dst->index = i;
    dst->type  = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    err = xioctl(cam->fd, VIDIOC_ENUM_FMT, dst);

    if (err) break;

    /*
     * mark succeed
     */
    ret = 0;

  } while(0);

  return ret;
}

int
camera_get_control_info(camera_t* cam, int ctrl, struct v4l2_queryctrl* dst)
{
  int ret;
  int err;

  do {
    /*
     * entry process
     */
    ret = !0;

    /*
     * check argument
     */
    if (cam == NULL) break;
    if (dst == NULL) break;

    /*
     * do query
     */
    memset(dst, 0, sizeof(*dst));

    dst->id = ctrl;

    err = xioctl(cam->fd, VIDIOC_QUERYCTRL, dst);
    if (err) break;

    /*
     * mark succeed
     */
    ret = 0;

  } while(0);

  return ret;
}

int
camera_get_frame_size(camera_t* cam, uint32_t fmt, uint32_t idx,
                      struct v4l2_frmsizeenum* dst)
{
  int ret;
  int err;

  do {
    /*
     * entry process
     */
    ret = !0;

    /*
     * check argument
     */
    if (cam == NULL) break;
    if (dst == NULL) break;

    /*
     * do query
     */
    memset(dst, 0, sizeof(*dst));

    dst->index        = idx;
    dst->pixel_format = fmt;

    err = xioctl(cam->fd, VIDIOC_ENUM_FRAMESIZES, dst);
    if (err) break;

    /*
     * mark succeed
     */
    ret = 0;

  } while(0);

  return ret;
}

int
camera_get_frame_rate(camera_t* cam, uint32_t fmt,
                      uint32_t wd, uint32_t ht, uint32_t idx,
                      struct v4l2_frmivalenum* dst)
{
  int ret;
  int err;

  do {
    /*
     * entry process
     */
    ret = !0;

    /*
     * check argument
     */
    if (cam == NULL) break;
    if (dst == NULL) break;

    /*
     * do query
     */
    memset(dst, 0, sizeof(*dst));

    dst->index        = idx;
    dst->pixel_format = fmt;
    dst->width        = wd;
    dst->height       = ht;

    err = xioctl(cam->fd, VIDIOC_ENUM_FRAMEINTERVALS, dst);
    if (err) break;

    /*
     * mark succeed
     */
    ret = 0;

  } while(0);

  return ret;
}

int
camera_get_menu_item(camera_t* cam, int ctrl, int index,
                     struct v4l2_querymenu* dst)
{
  int ret;
  int err;

  do {
      /*
       * entry process
       */
      ret = !0;

      /*
       * check argument
       */
      if (cam == NULL) break;
      if (dst == NULL) break;

      /*
       * do query
       */
      memset(dst, 0, sizeof(*dst));

      dst->id    = ctrl;
      dst->index = index;

      err = xioctl(cam->fd, VIDIOC_QUERYMENU, dst);
      if (err) break;

      /*
       * mark succeed
       */
      ret = 0;

  } while(0);

  return ret;
}

int
camera_get_control(camera_t* cam, uint32_t id, int32_t* dst)
{
  int ret;
  int err;
  struct v4l2_control ctrl;

  do {
      /*
       * entry process
       */
      ret = !0;

      /*
       * check argument
       */
      if (cam == NULL) break;
      if (dst == NULL) break;

      /*
       * do ioctl
       */
      memset(&ctrl, 0, sizeof(ctrl));

      ctrl.id = id;

      err = xioctl(cam->fd, VIDIOC_G_CTRL, &ctrl);
      if (err) {
        perror("ioctl(VIDIOC_S_CTRL)");
        break;
      }

      /*
       * set return parameter
       */
      *dst = ctrl.value;

      /*
       * mark succeed
       */
      ret = 0;

  } while(0);

  return ret;
}

int
camera_set_control(camera_t* cam, uint32_t id, int32_t src)
{
  int ret;
  int err;
  struct v4l2_control ctrl;

  do {
    /*
     * entry process
     */
    ret = !0;

    /*
     * check argument
     */
    if (cam == NULL) break;

    /*
     * do ioctl
     */
    ctrl.id    = id;
    ctrl.value = src;

    err = xioctl(cam->fd, VIDIOC_S_CTRL, &ctrl);
    if (err) {
        perror("ioctl(VIDIOC_S_CTRL)");
        break;
    }

    /*
     * mark succeed
     */
    ret = 0;

  } while(0);

  return ret;
}

int
camera_set_format(camera_t* cam, uint32_t format)
{
  int ret;

  do {
    /*
     * entry process
     */
    ret = !0;

    /*
     * check argument
     */
    if (cam == NULL) break;
    if (cam->state != ST_INITIALIZED) break;
    
    switch (format) {
    case V4L2_PIX_FMT_NV21:
    case V4L2_PIX_FMT_YUYV:
    case V4L2_PIX_FMT_NV16:
    case V4L2_PIX_FMT_RGB565:
    case V4L2_PIX_FMT_MJPEG:
    case V4L2_PIX_FMT_H264:
      break;

    default:
      fprintf(stderr, "camera_set_format():unknown image format.\n");
      goto out;
    }

    /*
     * update camera context
     */
    cam->format = format;
    update_image_size(cam);

    /*
     * mark succeed
     */
    ret = 0;
  } while(0);

  out:

  return ret;
}

int
camera_set_framerate(camera_t* cam, int num, int denom)
{
  int ret;

  do {
    /*
     * entry process
     */
    ret = !0;

    /*
     * check argunments
     */
    if (cam == NULL) break;
    if (cam->state != ST_INITIALIZED) break;

    /*
     * update camera context
     */
    cam->framerate.num   = num;
    cam->framerate.denom = denom;

    /*
     * mark succeed
     */
    ret = 0;

  } while (0);

  return ret;
}

int
camera_get_image_width(camera_t* cam, int* width)
{
  int ret;

  do {
    /*
     * entry process
     */
    ret = !0;

    /*
     * check argunments
     */
    if (cam == NULL) break;

    /*
     * set return paramater
     */
    *width = cam->width;

    /*
     * mark succeed
     */
    ret = 0;

  } while (0);

  return ret;
}

int
camera_set_image_width(camera_t* cam, int width)
{
  int ret;

  do {
    /*
     * entry process
     */
    ret = !0;

    /*
     * check argunments
     */
    if (cam == NULL) break;
    if (cam->state != ST_INITIALIZED) break;

    /*
     * update camera context
     */
    cam->width = width;
    update_image_size(cam);

    /*
     * mark succeed
     */
    ret = 0;

  } while (0);

  return ret;
}

int
camera_get_image_height(camera_t* cam, int* height)
{
  int ret;

  do {
    /*
     * entry process
     */
    ret = !0;

    /*
     * check argunments
     */
    if (cam == NULL) break;

    /*
     * set return paramater
     */
    *height = cam->height;

    /*
     * mark succeed
     */
    ret = 0;

  } while (0);

  return ret;
}


int
camera_set_image_height(camera_t* cam, int height)
{
  int ret;

  do {
    /*
     * entry process
     */
    ret = !0;

    /*
     * check argunments
     */
    if (cam == NULL) break;
    if (cam->state != ST_INITIALIZED) break;

    /*
     * update camera context
     */
    cam->height = height;
    update_image_size(cam);

    /*
     * mark succeed
     */
    ret = 0;

  } while (0);

  return ret;
}

int
camera_finalize(camera_t* cam)
{
  int ret;
  int i;

  do {
    /*
     * entry process
     */
    ret = !0;

    /*
     * check argument
     */
    if (cam == NULL) break;
    if (cam->state != ST_INITIALIZED && cam->state != ST_ERROR) break;

    if (cam->state != ST_ERROR) {
      /*
       * closing camera device
       */
      close(cam->fd);
      for (i = 0; i < NUM_PLANE; i++) mb_discard(cam->mb + i);
    }

    /*
     * destroy camera context
     */
    bzero(cam, sizeof(*cam));

    cam->fd    = -1;
    cam->state = ST_FINALIZED;

    /*
     * mark succeed
     */
    ret = 0;

  } while (0);

  return ret;
}

int
camera_start(camera_t* cam)
{
  int ret;
  int err;
  int i;

  do {
    /*
     * entry process
     */
    ret = !0;

    /*
     * check argunments
     */
    if (cam == NULL) break;
    if (cam->state != ST_INITIALIZED) break;

    /*
     * setup for camera device
     */
    err = set_format(cam->fd, cam->format, cam->width, cam->height);
    if (err) break;

    err = set_param(cam->fd, cam->framerate.num, cam->framerate.denom);
    if (err) break;

    err = request_buffer(cam->fd, NUM_PLANE, cam->mb);
    if (err) break;

    for (i = 0; i < NUM_PLANE; i++) {
        err = enqueue_buffer(cam->fd, i);
        if (err) break;
    }
    if (err) break;


    /*
     * start image capture
     */
    cam->state = ST_PREPARE;

    err = start(cam->fd);
    if (err) break;

    cam->state = ST_READY;

    /*
     * mark succeed
     */
    ret = 0;

  } while (0);

  if (ret) {
    /*
     * post process on error occured.
     */
    if (cam->state == ST_PREPARE) {
      cam->state = ST_INITIALIZED;
    }

    /*
     * 以下の資源を確保したままになっているかもしれないので解放しておく
     *   - ファイルハンドル
     *   - リクエストバッファ
     */
    if (cam->fd >= 0) {
        close(cam->fd);
        cam->fd = -1;
    }

    for (i = 0; i < NUM_PLANE; i++) mb_discard(cam->mb + i);
  }

  return ret;
}

int
camera_stop(camera_t* cam)
{
  int ret;
  int err;
  int i;

  do {
    /*
     * entry process
     */
    ret = !0;

    /*
     * check argument
     */
    if (cam == NULL) break;

    /*
     * stop image capture
     */
    if (cam->state != ST_PREPARE && cam->state != ST_READY &&
        cam->state != ST_ERROR) {
      break;
    }

    if (cam->state != ST_ERROR) {
      cam->state = ST_STOPPING;

      if (cam->state != ST_ERROR) {
        err = stop(cam->fd);
        if (err) break;
      }

      /*
       * device reset
       */
      close(cam->fd);
      for (i = 0; i < NUM_PLANE; i++) mb_discard(cam->mb + i);

      cam->fd = open(cam->device, O_RDWR);
      cam->state  = ST_INITIALIZED;
      cam->latest = -1;

    } else {
      /*
       * エラーの場合はオブジェクトの再利用を許さない。
       * 再オープンも初期化も行わず、デバイスのクロー
       * ズとリソースの解放のみを行う。
       */
      close(cam->fd);
      for (i = 0; i < NUM_PLANE; i++) mb_discard(cam->mb + i);

      cam->latest = -1;
    }

    /*
     * mark succeed
     */
    ret = 0;

  } while (0);

  return ret;
}

int
camera_get_image(camera_t* cam, void* ptr, size_t* used)
{
  int ret;

  do {
    /*
     * entry process
     */
    ret = !0;

    /*
     * check arguments
     */
    if (cam == NULL) break;

    /*
     * check statement
     */
    if (cam->state != ST_PREPARE && cam->state != ST_READY) {
      break;
    }

    /*
     * image copy
     */

		// 状態を変更
    cam->state = ST_REQUESTED;

		// フレームをキャプチャー。フレームが届くまで待つ。
		capture_frame(cam);

    if (cam->state == ST_ERROR) {
			// エラーが発生した場合はエラー状態を保持したまま処理をスキップする
      break;
    }

		if (cam->state == ST_BREAK) {
			// 割り込まれていた場合は、状態を元に戻して処理をスキップする
			cam->state = ST_READY;
			break;
		}

    // フレームをコピー
    mb_copyto(cam->mb + cam->latest, ptr, used);

    // コピー済みであることをマーク
    cam->latest |= COPIED;

    // 状態を元に戻す
    cam->state   = ST_READY;

    /*
     * mark succeed
     */
    ret = 0;

  } while (0);

  return ret;
}

int
camera_check_busy(camera_t* cam, int* busy)
{
  int ret;
  int err;
  struct v4l2_format fmt;

  do {
    /*
     * entry process
     */
    ret = !0;

    /*
     * check arguments
     */
    if (cam == NULL) break;
    if (busy == NULL) break;

    /*
     * do check (try set format)
     */
    BZERO(fmt);

    fmt.type                = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmt.fmt.pix.width       = cam->width;
    fmt.fmt.pix.height      = cam->height;
    fmt.fmt.pix.pixelformat = cam->format;
    fmt.fmt.pix.field       = V4L2_FIELD_INTERLACED;

    err = xioctl(cam->fd, VIDIOC_S_FMT, &fmt);
    if (err >= 0) {
      *busy = 0;
      ret   = 0;

    } else if (errno == EBUSY) {
      *busy = !0;
      ret   = 0;

    } else {
      perror("ioctl(VIDIOC_S_FMT)");
    }
  } while(0);

  return ret;
}

int
camera_check_error(camera_t* cam, int* error)
{
  int ret;
  int err;
  struct v4l2_format fmt;

  do {
    /*
     * entry process
     */
    ret = !0;

    /*
     * check arguments
     */
    if (cam == NULL) break;
    if (error == NULL) break;

    /*
     * do check (check state)
     */
    *error = (cam->state == ST_ERROR);

    ret = 0;
  } while(0);

  return ret;
}
