/*
 *
 * Video 4 Linux V2 driver library (camera utility).
 *
 *  Copyright (C) 2015 Hiroshi Kuwagata. All rights reserved.
 *
 */

/*
 * $Id: camera.h 158 2017-10-26 08:51:55Z pi $
 */

#ifndef __CAMERA_H__
#define __CAMERA_H__

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#ifdef RUBY_EXTLIB
#include <ruby.h>
#else /* defined(RUBY_EXTLIB) */
#include <pthread.h>
#endif /* !defined(RUBY_EXTLIB) */

#ifdef __OpenBSD__
#include <sys/videoio.h>
#else
#include <linux/videodev2.h>
#endif

#define MAX_PLANE           3

#define F_JPG_OUTPUT        1

typedef struct __mblock__ {
  void* ptr;
  size_t size;
  size_t used;
} mblock_t;

typedef struct __camera__ {
  char device[64];

  char name[sizeof(((struct v4l2_capability*)NULL)->card) + 1];
  char driver[sizeof(((struct v4l2_capability*)NULL)->driver) + 1];
  char bus[sizeof(((struct v4l2_capability*)NULL)->bus_info) + 1];

  int fd;
  int format;
  int width;
  int height;

  struct {
    int num;
    int denom;
  } framerate;

  int state;
  size_t image_size;

  int latest;

  mblock_t mb[MAX_PLANE];
} camera_t;

#ifndef V4L2_CTRL_CLASS_JPEG
#define V4L2_CTRL_CLASS_JPEG            0x009d0000
#define V4L2_CID_JPEG_CLASS_BASE        (V4L2_CTRL_CLASS_JPEG | 0x900)
#endif /* !defined(V4L2_CTRL_CLASS_JPEG) */

#ifndef V4L2_CTRL_TYPE_INTEGER_MENU
#define V4L2_CTRL_TYPE_INTEGER_MENU     9
struct __v4l2_querymenu_substitute_ {
  uint32_t  id;
  uint32_t  index;
  union {
    uint8_t name[32];
    int64_t value;
  };
  uint32_t  reserved;
}  __attribute__ ((packed));

#define v4l2_querymenu  __v4l2_querymenu_substitute_
#endif /* !defined(V4L2_CTRL_TYPE_INTEGER_MENU) */



/*
 * イニシャライズ時のデフォルト設定は以下の通りです。
 *   format:    Motion JPEG
 *   size:      VGA
 *   framerate: 30/1
 */
extern int camera_initialize(camera_t* cam, char dev[]);

/*
 * finalized後にコンテキストを再利用する場合は、initialize()からやり直す
 * 必要があります。
 */
extern int camera_finalize(camera_t* cam);

extern int camera_start(camera_t* cam);
extern int camera_stop(camera_t* cam);

extern int camera_set_format(camera_t* cam, uint32_t format);
extern int camera_get_image_width(camera_t* cam, int* width);
extern int camera_set_image_width(camera_t* cam, int width);
extern int camera_get_image_height(camera_t* cam, int* height);
extern int camera_set_image_height(camera_t* cam, int height);
extern int camera_set_framerate(camera_t* cam, int num, int denom);

extern int camera_get_image_size(camera_t* cam, size_t* sz);
extern int camera_get_image(camera_t* cam, void* ptr, size_t* used);
extern int camera_check_busy(camera_t* cam, int *busy);
extern int camera_check_error(camera_t* cam, int *error);

extern int camera_get_format_desc(camera_t* cam, int i,
                                  struct v4l2_fmtdesc* desc);

extern int camera_get_control_info(camera_t* cam, int ctrl,
                                   struct v4l2_queryctrl* info);

extern int camera_get_frame_size(camera_t* cam, uint32_t fmt, uint32_t idx,
                                 struct v4l2_frmsizeenum* info);
extern int camera_get_frame_rate(camera_t* cam, uint32_t fmt,
                                 uint32_t wd, uint32_t ht, uint32_t idx,
                                 struct v4l2_frmivalenum* dst);

extern int camera_get_menu_item(camera_t* cam, int ctrl, int index,
                                struct v4l2_querymenu* item);
extern int camera_set_control(camera_t* cam, uint32_t id, int32_t value);
extern int camera_get_control(camera_t* cam, uint32_t id, int32_t* value);

#endif /* !defined(__CAMERA_H__) */
