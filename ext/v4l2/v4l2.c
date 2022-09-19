/*
 * Video 4 Linux V2 for Ruby interface library.
 *
 *  Copyright (C) 2015 Hiroshi Kuwagata. All rights reserved.
 */

/*
 * $Id: v4l2.c 121 2016-11-18 04:32:27Z pi $
 */

#include "ruby.h"
#include "ruby/encoding.h"

#include "camera.h"

#define N(x)                            (sizeof((x))/sizeof(*(x)))

#define EQ_STR(val,str)                 (rb_to_id(val) == rb_intern(str))

extern rb_encoding* rb_utf8_encoding(void);
extern rb_encoding* rb_default_internal_encoding(void);

static VALUE module;
static VALUE camera_klass;
static VALUE control_klass;
static VALUE integer_klass;
static VALUE boolean_klass;
static VALUE menu_klass;
//static VALUE imenu_klass;
static VALUE menu_item_klass;
static VALUE frame_cap_klass;
static VALUE fmt_desc_klass;

static ID id_iv_name;
static ID id_iv_driver;
static ID id_iv_bus;
static ID id_iv_id;
static ID id_iv_max;
static ID id_iv_min;
static ID id_iv_step;
static ID id_iv_default;
static ID id_iv_items;
static ID id_iv_index;
static ID id_iv_width;
static ID id_iv_height;
static ID id_iv_rate;
static ID id_iv_fcc;
static ID id_iv_desc;

static void rb_camera_free(void* ptr);
static size_t rb_camera_size(const void* ptr);

static const rb_data_type_t camera_data_type = {
  "V4L2 for ruby",                      // wrap_struct_name
  NULL,                                 // function.dmark
  rb_camera_free,                       // function.dfree
  rb_camera_size,                       // function.dsize
  NULL,                                 // function.dcompact
  NULL,                                 // reserved[0]
  NULL,                                 // parent
  NULL,                                 // data
  (VALUE)RUBY_TYPED_FREE_IMMEDIATELY    // flags
};

static void
rb_camera_free(void* ptr)
{
  if (((camera_t*)ptr)->state != 6) {
    camera_finalize( ptr);
  }

  free( ptr);
}

static size_t
rb_camera_size(const void* ptr)
{
  return sizeof(camera_t);
}

static VALUE
rb_camera_alloc(VALUE self)
{
  camera_t* ptr;

  return TypedData_Make_Struct(camera_klass, camera_t, &camera_data_type, ptr);
}

static VALUE
rb_camera_open(VALUE self, VALUE device)
{
  VALUE ret;

  ret = rb_obj_alloc(camera_klass);
  rb_obj_call_init(ret, 1, &device);

  return ret;
}

static VALUE
rb_camera_initialize(VALUE self, VALUE dev)
{
  int err;
  camera_t* ptr;

  /*
   * argument check
   */
  Check_Type(dev, T_STRING);

  /*
   * strip object
   */
  TypedData_Get_Struct(self, camera_t, &camera_data_type, ptr);

  /*
   * initialize struct
   */
  err = camera_initialize(ptr, RSTRING_PTR(dev));
  if (err) {
    rb_raise(rb_eRuntimeError, "initialize camera context failed.");
  }

  /*
   * set default instance variable
   */
  rb_ivar_set(self, id_iv_name,
              rb_enc_str_new_cstr(ptr->name, rb_utf8_encoding()));

  rb_ivar_set(self, id_iv_driver,
              rb_enc_str_new_cstr(ptr->driver, rb_utf8_encoding()));

  rb_ivar_set(self, id_iv_bus,
              rb_enc_str_new_cstr(ptr->bus, rb_utf8_encoding()));

  return Qtrue;
}

static VALUE
rb_camera_close( VALUE self)
{
  int err;
  camera_t* ptr;
  int ready;

  /*
   * strip object
   */
  TypedData_Get_Struct(self, camera_t, &camera_data_type, ptr);

  /*
   * do close
   */
  do {
    err = camera_check_ready(ptr, &ready);
    if (err) {
      rb_raise(rb_eRuntimeError, "camera_check_ready() failed.");
    }

    if (ready) {
      err = camera_stop(ptr);
      if (err) {
        rb_raise(rb_eRuntimeError, "camera_stop() failed.");
      }
    }

    err = camera_finalize(ptr);
    if (err) {
      rb_raise(rb_eRuntimeError, "finalize camera context failed.");
    }
  } while (0);

  return Qnil;
}

static VALUE
get_menu_list(camera_t* ptr, int ctrl, int min, int max)
{
  VALUE ret;
  int i;
  int err;
  struct v4l2_querymenu item;

  ret = rb_ary_new();
  for (i = min; i <= max; i++) {
    err = camera_get_menu_item(ptr, ctrl, i, &item);
    if (!err) {
      VALUE tmp;
      VALUE name;

      tmp  = rb_obj_alloc(menu_item_klass);
      name = rb_enc_str_new_cstr((const char*)item.name, rb_utf8_encoding());

      rb_ivar_set(tmp, id_iv_name, name);
      rb_ivar_set(tmp, id_iv_index, INT2FIX(item.index));
      
      rb_ary_push(ret, tmp);
    }
  }

  return ret;
}

static VALUE
get_int_menu_list(camera_t* ptr, int ctrl, int min, int max)
{
  VALUE ret;
  int i;
  int err;
  struct v4l2_querymenu item;

  ret = rb_ary_new();
  for (i = min; i <= max; i++) {
    err = camera_get_menu_item(ptr, ctrl, i, &item);
    if (!err) {
      VALUE tmp;
      VALUE name;

      tmp  = rb_obj_alloc(menu_item_klass);
      name = rb_enc_sprintf(rb_utf8_encoding(), "%lld", item.value);

      rb_ivar_set(tmp, id_iv_name, name);
      rb_ivar_set(tmp, id_iv_index, INT2FIX(item.index));
      
      rb_ary_push(ret, tmp);
    }
  }

  return ret;
}

static VALUE
get_control_info(camera_t* ptr, int ctrl)
{
  VALUE ret;
  VALUE name;
  int err; 
  struct v4l2_queryctrl info;
  VALUE list;

  ret = Qnil;
  err = camera_get_control_info(ptr, ctrl, &info);

  if (!err) {
      switch (info.type) {
      case V4L2_CTRL_TYPE_INTEGER:
        ret  = rb_obj_alloc(integer_klass);
        name = rb_enc_str_new_cstr((const char*)info.name,
                                   rb_utf8_encoding());

        rb_ivar_set(ret, id_iv_name, name);
        rb_ivar_set(ret, id_iv_id, INT2NUM(info.id));
        rb_ivar_set(ret, id_iv_min, INT2FIX(info.minimum));
        rb_ivar_set(ret, id_iv_max, INT2FIX(info.maximum));
        rb_ivar_set(ret, id_iv_step, INT2FIX(info.step));
        rb_ivar_set(ret, id_iv_default, INT2FIX(info.default_value));
        break;

      case V4L2_CTRL_TYPE_BOOLEAN:
        ret  = rb_obj_alloc(boolean_klass);
        name = rb_enc_str_new_cstr((const char*)info.name,
                                   rb_utf8_encoding());

        rb_ivar_set(ret, id_iv_name, name);
        rb_ivar_set(ret, id_iv_id, INT2NUM(info.id));
        rb_ivar_set(ret, id_iv_default, (info.default_value)? Qtrue: Qfalse);
        break;

      case V4L2_CTRL_TYPE_MENU:
        ret  = rb_obj_alloc(menu_klass);
        name = rb_enc_str_new_cstr((const char*)info.name,
                                   rb_utf8_encoding());
        list = get_menu_list(ptr, ctrl, info.minimum, info.maximum);

        rb_ivar_set(ret, id_iv_name, name);
        rb_ivar_set(ret, id_iv_id, INT2NUM(info.id));
        rb_ivar_set(ret, id_iv_default, INT2FIX(info.default_value));
        rb_ivar_set(ret, id_iv_items, list);
        break;

      case V4L2_CTRL_TYPE_INTEGER_MENU:
        ret  = rb_obj_alloc(menu_klass);
        name = rb_enc_str_new_cstr((const char*)info.name,
                                   rb_utf8_encoding());
        list = get_int_menu_list(ptr, ctrl, info.minimum, info.maximum);

        rb_ivar_set(ret, id_iv_name, name);
        rb_ivar_set(ret, id_iv_id, INT2NUM(info.id));
        rb_ivar_set(ret, id_iv_default, INT2FIX(info.default_value));
        rb_ivar_set(ret, id_iv_items, list);
        break;

      default:
        fprintf(stderr, "unsupported type %d, name: %s\n",
                info.type, info.name);
        break;
      }
  }

  return ret;
}

static VALUE
rb_camera_get_controls(VALUE self)
{
  VALUE ret;
  camera_t* ptr;
  int i;
  VALUE info;

  TypedData_Get_Struct(self, camera_t, &camera_data_type, ptr);

  ret = rb_ary_new();

  for (i = 0; i < 43; i++) {
    info = get_control_info(ptr, V4L2_CID_BASE + i);
    if (info != Qnil) rb_ary_push(ret, info);
  }

  for (i = 0; i < 30; i++) {
    info = get_control_info(ptr, V4L2_CID_CAMERA_CLASS_BASE + i);
    if (info != Qnil) rb_ary_push(ret, info);
  }

  for (i = 0; i < 20; i++) {
    info = get_control_info(ptr, V4L2_CID_JPEG_CLASS_BASE + i);
    if (info != Qnil) rb_ary_push(ret, info);
  }

  return ret;
}

static VALUE
get_framerate_list(camera_t* cam,
                   uint32_t fmt, struct v4l2_frmsize_discrete* size)
{
  VALUE ret;
  int i;
  int err;
  struct v4l2_frmivalenum intval;
  VALUE rate;
  int num;
  int deno;

  ret = rb_ary_new();

  for (i = 0; ; i++) {
    err = camera_get_frame_rate(cam, fmt,
                                size->width, size->height, i, &intval);
    if (err) break;

    switch (intval.type) {
    case V4L2_FRMIVAL_TYPE_DISCRETE:
      rate = rb_rational_new(INT2FIX(intval.discrete.denominator),
                             INT2FIX(intval.discrete.numerator));

      rb_ary_push(ret, rate);
      break;

    case V4L2_FRMIVAL_TYPE_CONTINUOUS:
    case V4L2_FRMIVAL_TYPE_STEPWISE:
      num  = intval.stepwise.max.numerator;
      deno = intval.stepwise.max.denominator;

      while (num <= intval.stepwise.min.numerator) {
        while (deno <= intval.stepwise.min.denominator) {
          rate = rb_rational_new(INT2FIX(deno), INT2FIX(num));
          rb_ary_push(ret, rate);

          deno += intval.stepwise.step.denominator;
        }

        num += intval.stepwise.step.numerator;
        deno = intval.stepwise.max.denominator;
      }
      break;
    }
  }

  return ret;
}

static uint32_t
to_pixfmt(VALUE fmt)
{
  uint32_t ret;

  if (EQ_STR(fmt, "YUYV") || EQ_STR(fmt, "YUV422")) {
    ret = V4L2_PIX_FMT_YUYV;

  } else if (EQ_STR(fmt, "NV12")) {
    ret = V4L2_PIX_FMT_NV12;

  } else if (EQ_STR(fmt, "NV21")) {
    ret = V4L2_PIX_FMT_NV21;

  } else if (EQ_STR(fmt, "NV16")) {
    ret = V4L2_PIX_FMT_NV16;

  } else if (EQ_STR(fmt, "YUV420") || EQ_STR(fmt, "YU12")) {
    ret = V4L2_PIX_FMT_YUV420;

  } else if (EQ_STR(fmt, "YVU420") || EQ_STR(fmt, "YV12")) {
    ret = V4L2_PIX_FMT_YVU420;

  } else if (EQ_STR(fmt, "NV16")) {
    ret = V4L2_PIX_FMT_NV16;

  } else if (EQ_STR(fmt, "RGB565") || EQ_STR(fmt, "RGBP")) {
    ret = V4L2_PIX_FMT_RGB565;

  } else if (EQ_STR(fmt, "MJPEG") || EQ_STR(fmt, "MJPG")) {
    ret = V4L2_PIX_FMT_MJPEG;

  } else if (EQ_STR(fmt, "H264")) {
    ret = V4L2_PIX_FMT_H264;

  } else {
    rb_raise(rb_eRuntimeError, "Unsupported pixel format.");
  }

  return ret;
}

static VALUE
rb_camera_get_support_formats(VALUE self)
{
  VALUE ret;
  camera_t* ptr;

  int i;
  int err;
  struct v4l2_fmtdesc desc;

  VALUE fmt;
  VALUE fcc;
  VALUE str;

  TypedData_Get_Struct(self, camera_t, &camera_data_type, ptr);

  ret = rb_ary_new();

  for (i = 0; ;i++) {
    err = camera_get_format_desc(ptr, i, &desc);
    if (err) break;

    fmt = rb_obj_alloc(fmt_desc_klass);
    fcc = rb_enc_sprintf(rb_utf8_encoding(),
                         "%c%c%c%c",
                         desc.pixelformat >>  0 & 0xff,
                         desc.pixelformat >>  8 & 0xff,
                         desc.pixelformat >> 16 & 0xff,
                         desc.pixelformat >> 24 & 0xff);

    str = rb_enc_str_new_cstr((const char*)desc.description,
                               rb_utf8_encoding());

    rb_ivar_set(fmt, id_iv_fcc, fcc);
    rb_ivar_set(fmt, id_iv_desc, str);
    
    rb_ary_push(ret, fmt);
  }

  return ret;
}

static void
make_dummy_capabilities(camera_t* ptr, VALUE ary,
                        uint32_t fmt, int max_width, int max_height)
{
  struct v4l2_frmsize_discrete dsize;
  VALUE capa;

  if (max_width > max_height) {
    /*
     * when landscape
     */

    for (dsize.width = 160; dsize.width < max_width; dsize.width *= 2) {
      dsize.height = (dsize.width * 9) / 16;

      capa = rb_obj_alloc(frame_cap_klass);
      rb_ivar_set(capa, id_iv_width, INT2NUM(dsize.width));
      rb_ivar_set(capa, id_iv_height, INT2NUM(dsize.height));
      rb_ivar_set(capa, id_iv_rate, get_framerate_list(ptr, fmt, &dsize));

      rb_ary_push(ary, capa);

      dsize.height = (dsize.width * 3) / 4;

      capa = rb_obj_alloc(frame_cap_klass);
      rb_ivar_set(capa, id_iv_width, INT2NUM(dsize.width));
      rb_ivar_set(capa, id_iv_height, INT2NUM(dsize.height));
      rb_ivar_set(capa, id_iv_rate, get_framerate_list(ptr, fmt, &dsize));

      rb_ary_push(ary, capa);
    }

  } else {
    /*
     * when portrate
     */

    dsize.height = 160;

    for (dsize.height = 160; dsize.height < max_height; dsize.height *= 2) {
      dsize.width = (dsize.height * 9) / 16;

      capa = rb_obj_alloc(frame_cap_klass);
      rb_ivar_set(capa, id_iv_width, INT2NUM(dsize.width));
      rb_ivar_set(capa, id_iv_height, INT2NUM(dsize.height));
      rb_ivar_set(capa, id_iv_rate, get_framerate_list(ptr, fmt, &dsize));

      rb_ary_push(ary, capa);

      dsize.width = (dsize.height * 3) / 4;

      capa = rb_obj_alloc(frame_cap_klass);
      rb_ivar_set(capa, id_iv_width, INT2NUM(dsize.width));
      rb_ivar_set(capa, id_iv_height, INT2NUM(dsize.height));
      rb_ivar_set(capa, id_iv_rate, get_framerate_list(ptr, fmt, &dsize));

      rb_ary_push(ary, capa);
    }
  }
}

static VALUE
rb_camera_get_frame_capabilities(VALUE self, VALUE _fmt)
{
  VALUE ret;
  camera_t* ptr;
  int i;
  int j;
  uint32_t fmt;

  int err;
  struct v4l2_frmsizeenum size;

  VALUE capa;
  VALUE list;
  VALUE rate;

  TypedData_Get_Struct(self, camera_t, &camera_data_type, ptr);

  ret = rb_ary_new();
  fmt = to_pixfmt(_fmt);

  for (i = 0; ;i++){
    err = camera_get_frame_size(ptr, fmt, i, &size);
    if (err) break;

    if (size.type == V4L2_FRMSIZE_TYPE_DISCRETE) {
      capa = rb_obj_alloc(frame_cap_klass);
      list = get_framerate_list(ptr, fmt, &size.discrete);

      rb_ivar_set(capa, id_iv_width, INT2NUM(size.discrete.width));
      rb_ivar_set(capa, id_iv_height, INT2NUM(size.discrete.height));
      rb_ivar_set(capa, id_iv_rate, list);

      rb_ary_push(ret, capa);

    } else if (size.type == V4L2_FRMSIZE_TYPE_STEPWISE) {
      make_dummy_capabilities(ptr,
                              ret,
                              fmt,
                              size.stepwise.max_width,
                              size.stepwise.max_height);
      break;
    }
  }

  return ret;
}

static VALUE
rb_camera_set_control(VALUE self, VALUE id, VALUE _val)
{
  camera_t* ptr;
  int err;
  int val;

  TypedData_Get_Struct(self, camera_t, &camera_data_type, ptr);

  switch (TYPE(_val)) {
  case T_TRUE:
    val = 1;
    break;

  case T_FALSE:
    val = 0;
    break;

  case T_FIXNUM:
    val = FIX2INT(_val);
    break;

  default:
    rb_raise(rb_eTypeError, "invalid type of control value");
  }

  err = camera_set_control(ptr, FIX2INT(id), val);
  if (err) {
    rb_raise(rb_eRuntimeError, "set control failed.");
  }

  return Qnil;
}

static VALUE
rb_camera_get_control(VALUE self, VALUE id)
{
  camera_t* ptr;
  int err;
  int32_t value;

  TypedData_Get_Struct(self, camera_t, &camera_data_type, ptr);

  err = camera_get_control(ptr, FIX2INT(id), &value);
  if (err) {
      rb_raise(rb_eRuntimeError, "get control failed.");
  }

  return INT2FIX(value);
}

static VALUE
rb_camera_set_format(VALUE self, VALUE fmt)
{
  camera_t* ptr;
  int err;

  /*
   * strip object
   */
  TypedData_Get_Struct(self, camera_t, &camera_data_type, ptr);

  /*
   * set parameter
   */
  err = camera_set_format(ptr, to_pixfmt(fmt));
  if (err) {
    rb_raise(rb_eRuntimeError, "set format failed.");
  }

  return Qnil;
}

static VALUE
rb_camera_get_image_width(VALUE self)
{
  int ret;
  camera_t* ptr;
  int err;

  /*
   * strip object
   */
  TypedData_Get_Struct(self, camera_t, &camera_data_type, ptr);

  /*
   * get parameter
   */
  err = camera_get_image_width(ptr, &ret);
  if (err) {
    rb_raise(rb_eRuntimeError, "get image width failed.");
  }

  return INT2FIX(ret);
}

static VALUE
rb_camera_set_image_width(VALUE self, VALUE val)
{
  camera_t* ptr;
  int err;

  /*
   * strip object
   */
  TypedData_Get_Struct(self, camera_t, &camera_data_type, ptr);

  /*
   * set parameter
   */
  err = camera_set_image_width(ptr, NUM2INT(val));
  if (err) {
    rb_raise(rb_eRuntimeError, "set image width failed.");
  }

  return Qnil;
}

static VALUE
rb_camera_get_image_height(VALUE self)
{
  int ret;
  camera_t* ptr;
  int err;

  /*
   * strip object
   */
  TypedData_Get_Struct(self, camera_t, &camera_data_type, ptr);

  /*
   * get parameter
   */
  err = camera_get_image_height(ptr, &ret);
  if (err) {
    rb_raise(rb_eRuntimeError, "get image height failed.");
  }

  return INT2FIX(ret);
}

static VALUE
rb_camera_set_image_height(VALUE self, VALUE val)
{
  camera_t* ptr;
  int err;

  /*
   * strip object
   */
  TypedData_Get_Struct(self, camera_t, &camera_data_type, ptr);

  /*
   * set parameter
   */
  err = camera_set_image_height(ptr, NUM2INT(val));
  if (err) {
    rb_raise(rb_eRuntimeError, "set image height failed.");
  }

  return Qnil;
}

static VALUE
rb_camera_get_framerate(VALUE self)
{
  VALUE ret;
  camera_t* ptr;

  /*
   * strip object
   */
  TypedData_Get_Struct(self, camera_t, &camera_data_type, ptr);

  /*
   * create return object
   */
  ret = rb_rational_new(INT2FIX(ptr->framerate.num),
                        INT2FIX(ptr->framerate.denom));

  return ret;
}

static VALUE
rb_camera_set_framerate(VALUE self, VALUE val)
{
  camera_t* ptr;
  int num;
  int denom;
  int err;

  /*
   * argument check
   */
  switch (TYPE(val)) {
  case T_FIXNUM:
    num   = 1;
    denom = FIX2INT(val);
    break;

  case T_FLOAT:
    num   = 1000;
    denom = (int)(NUM2DBL(val) * 1000.0);
    break;

  case T_RATIONAL:
    num   = FIX2INT(rb_rational_den(val));
    denom = FIX2INT(rb_rational_num(val));
    break;
  
  default:
    rb_raise(rb_eTypeError, "illeagal framerate value.");
  }

  /*
   * strip object
   */
  TypedData_Get_Struct(self, camera_t, &camera_data_type, ptr);

  /*
   * set framerate
   */
  err = camera_set_framerate(ptr, num, denom);
  if (err) {
    rb_raise(rb_eRuntimeError, "set framerate failed.");
  }

  return Qnil;
}

static VALUE
rb_camera_state( VALUE self)
{
  camera_t* ptr;
  const char* str;

  /*
   * strip object
   */
  TypedData_Get_Struct(self, camera_t, &camera_data_type, ptr);

  /*
   * convert state code
   */
  switch (ptr->state) {
  case -1:
    str = "ERROR";
    break;

  case 0:
    str = "FINALIZED";
    break;

  case 1:
    str = "INITIALIZED";
    break;

  case 2:
    str = "PREAPARE";
    break;

  case 3:
    str = "READY";
    break;

  case 4:
    str = "ST_REQUESTED";
    break;

  default:
    str = "unknown";
    break;
  }

  return ID2SYM(rb_intern(str));
}

static VALUE
rb_camera_start(VALUE self)
{
  int err;
  camera_t* ptr;
  int state;

  /*
   * strip object
   */
  TypedData_Get_Struct(self, camera_t, &camera_data_type, ptr);

  /*
   * do start
   */
  err = camera_start(ptr);
  if (err) {
    rb_raise(rb_eRuntimeError, "start capture failed.");
  }

  /*
   *
   */
  if (rb_block_given_p()) {
    rb_protect(rb_yield, self, &state);
    camera_stop(ptr);
    if (state) rb_jump_tag(state);
  }

  return self;
}

static VALUE
rb_camera_stop(VALUE self)
{
  int err;
  camera_t* ptr;

  /*
   * strip object
   */
  TypedData_Get_Struct(self, camera_t, &camera_data_type, ptr);

  /*
   * convert
   */
  err = camera_stop(ptr);
  if (err) {
    rb_raise(rb_eRuntimeError, "stop capture failed.");
  }

  return self;
}


static VALUE
rb_camera_capture(VALUE self)
{
  VALUE ret;
  camera_t* ptr;
  size_t used;
  int err;

  /*
   * strip object
   */
  TypedData_Get_Struct(self, camera_t, &camera_data_type, ptr);

  /*
   * allocate return value.
   */
  ret = rb_str_buf_new(ptr->image_size);
  rb_str_set_len(ret, ptr->image_size);

  /*
   * do capture
   */
  err = camera_get_image(ptr, RSTRING_PTR(ret), &used);
  if (err) {
    rb_raise(rb_eRuntimeError, "capture failed.");
  }

  if (ptr->image_size != used) {
    rb_str_set_len(ret, used);
  }

  return ret;
}

static VALUE
rb_camera_is_busy(VALUE self)
{
  camera_t* ptr;
  int err;
  int busy;

  /*
   * strip object
   */
  TypedData_Get_Struct(self, camera_t, &camera_data_type, ptr);

  /*
   * do check
   */
  err = camera_check_busy(ptr, &busy);
  if (err) {
    rb_raise(rb_eRuntimeError, "check failed.");
  }

  return (busy)? Qtrue: Qfalse;
}

static VALUE
rb_camera_is_ready(VALUE self)
{
  camera_t* ptr;
  int err;
  int ready;

  /*
   * strip object
   */
  TypedData_Get_Struct(self, camera_t, &camera_data_type, ptr);

  /*
   * do check
   */
  err = camera_check_ready(ptr, &ready);
  if (err) {
    rb_raise(rb_eRuntimeError, "check failed.");
  }

  return (ready)? Qtrue: Qfalse;
}

static VALUE
rb_camera_is_error(VALUE self)
{
  camera_t* ptr;
  int err;
  int error;

  /*
   * strip object
   */
  TypedData_Get_Struct(self, camera_t, &camera_data_type, ptr);

  /*
   * do check
   */
  err = camera_check_error(ptr, &error);
  if (err) {
    rb_raise(rb_eRuntimeError, "check failed.");
  }

  return (error)? Qtrue: Qfalse;
}

void
Init_v4l2()
{
#ifdef HAVE_RB_EXT_RACTOR_SAFE
  rb_ext_ractor_safe(true);
#endif /* defined(HAVE_RB_EXT_RACTOR_SAFE) */

  rb_require("monitor");

  module           = rb_define_module("Video4Linux2");
  camera_klass     = rb_define_class_under(module, "Camera", rb_cObject);

  rb_define_alloc_func(camera_klass, rb_camera_alloc);
  rb_define_method(camera_klass, "initialize", rb_camera_initialize, 1);
  rb_define_method(camera_klass, "close", rb_camera_close, 0);
  rb_define_method(camera_klass, "controls", rb_camera_get_controls, 0);
  rb_define_method(camera_klass,
                   "support_formats", rb_camera_get_support_formats, 0);
  rb_define_method(camera_klass,
                   "frame_capabilities", rb_camera_get_frame_capabilities, 1);
  rb_define_method(camera_klass, "set_control", rb_camera_set_control, 2);
  rb_define_method(camera_klass, "get_control", rb_camera_get_control, 1);
  rb_define_method(camera_klass, "format=", rb_camera_set_format, 1);
  rb_define_method(camera_klass, "image_width", rb_camera_get_image_width, 0);
  rb_define_method(camera_klass, "image_width=", rb_camera_set_image_width, 1);
  rb_define_method(camera_klass, "image_height", rb_camera_get_image_height,0);
  rb_define_method(camera_klass, "image_height=", rb_camera_set_image_height,1);
  rb_define_method(camera_klass, "framerate", rb_camera_get_framerate, 0);
  rb_define_method(camera_klass, "framerate=", rb_camera_set_framerate, 1);
  rb_define_method(camera_klass, "state", rb_camera_state, 0);
  rb_define_method(camera_klass, "start", rb_camera_start, 0);
  rb_define_method(camera_klass, "stop", rb_camera_stop, 0);
  rb_define_method(camera_klass, "capture", rb_camera_capture, 0);
  rb_define_method(camera_klass, "busy?", rb_camera_is_busy, 0);
  rb_define_method(camera_klass, "ready?", rb_camera_is_ready, 0);
  rb_define_method(camera_klass, "error?", rb_camera_is_error, 0);

  rb_define_attr(camera_klass, "name", !0, 0);
  rb_define_attr(camera_klass, "driver", !0, 0);
  rb_define_attr(camera_klass, "bus", !0, 0);

  rb_define_singleton_method( camera_klass, "open", rb_camera_open, 1);

  control_klass   = rb_define_class_under(camera_klass,
                                          "Control", rb_cObject);
  rb_define_attr(control_klass, "name", !0, 0);
  rb_define_attr(control_klass, "id", !0, 0);

  integer_klass   = rb_define_class_under(camera_klass,
                                          "IntegerControl", control_klass);
  rb_define_attr(integer_klass, "min", !0, 0);
  rb_define_attr(integer_klass, "max", !0, 0);
  rb_define_attr(integer_klass, "step", !0, 0);
  rb_define_attr(integer_klass, "default", !0, 0);

  boolean_klass   = rb_define_class_under(camera_klass,
                                          "BooleanControl", control_klass);
  rb_define_attr(boolean_klass, "default", !0, 0);

  menu_klass      = rb_define_class_under(camera_klass,
                                          "MenuControl", control_klass);
  rb_define_attr(menu_klass, "default", !0, 0);
  rb_define_attr(menu_klass, "items", !0, 0);

  menu_item_klass = rb_define_class_under(camera_klass,
                                          "MenuItem", rb_cObject);
  rb_define_attr(menu_item_klass, "name", !0, 0);
  rb_define_attr(menu_item_klass, "index", !0, 0);

  frame_cap_klass = rb_define_class_under(camera_klass,
                                          "FrameCapability", rb_cObject);
  rb_define_attr(frame_cap_klass, "width", !0, 0);
  rb_define_attr(frame_cap_klass, "height", !0, 0);
  rb_define_attr(frame_cap_klass, "rate", !0, 0);

  fmt_desc_klass  = rb_define_class_under(camera_klass,
                                          "FormatDescription", rb_cObject);
  rb_define_attr(fmt_desc_klass, "fcc", !0, 0);
  rb_define_attr(fmt_desc_klass, "description", !0, 0);

  id_iv_name    = rb_intern_const("@name");
  id_iv_driver  = rb_intern_const("@driver");
  id_iv_bus     = rb_intern_const("@bus");
  id_iv_id      = rb_intern_const("@id");
  id_iv_min     = rb_intern_const("@min");
  id_iv_max     = rb_intern_const("@max");
  id_iv_step    = rb_intern_const("@step");
  id_iv_default = rb_intern_const("@default");
  id_iv_items   = rb_intern_const("@items");
  id_iv_index   = rb_intern_const("@index");
  id_iv_width   = rb_intern_const("@width");
  id_iv_height  = rb_intern_const("@height");
  id_iv_rate    = rb_intern_const("@rate");
  id_iv_fcc     = rb_intern_const("@fcc");
  id_iv_desc    = rb_intern_const("@description");
}
