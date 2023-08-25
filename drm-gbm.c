// gcc -o drm-gbm drm-gbm.c -ldrm -lgbm -lEGL -lGL -I/usr/include/libdrm

// general documentation: man drm

#include <EGL/egl.h>
#include <GL/gl.h>
#include <fcntl.h>
#include <gbm.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <xf86drm.h>
#include <xf86drmMode.h>

#define EXIT(msg)                                                              \
  {                                                                            \
    fputs(msg, stderr);                                                        \
    exit(EXIT_FAILURE);                                                        \
  }

static int device;

static drmModeConnector *find_connector(drmModeRes *resources) {
  // iterate the connectors
  int i;
  for (i = 0; i < resources->count_connectors; i++) {
    drmModeConnector *connector =
        drmModeGetConnector(device, resources->connectors[i]);
    // pick the first connected connector
    if (connector->connection == DRM_MODE_CONNECTED) {
      return connector;
    }
    drmModeFreeConnector(connector);
  }
  // no connector found
  return NULL;
}

static drmModeEncoder *find_encoder(drmModeRes *resources,
                                    drmModeConnector *connector) {
  if (connector->encoder_id) {
    return drmModeGetEncoder(device, connector->encoder_id);
  }
  // no encoder found
  return NULL;
}

static uint32_t connector_id;
static drmModeModeInfo mode_info;
static drmModeCrtc *crtc;

static void find_display_configuration() {
  drmModeRes *resources = drmModeGetResources(device);
  // find a connector
  drmModeConnector *connector = find_connector(resources);
  if (!connector)
    EXIT("no connector found\n");
  // save the connector_id
  connector_id = connector->connector_id;
  // save the first mode
  mode_info = connector->modes[0];
  printf("resolution: %ix%i\n", mode_info.hdisplay, mode_info.vdisplay);
  // find an encoder
  drmModeEncoder *encoder = find_encoder(resources, connector);
  if (!encoder)
    EXIT("no encoder found\n");
  // find a CRTC
  if (encoder->crtc_id) {
    crtc = drmModeGetCrtc(device, encoder->crtc_id);
  }
  drmModeFreeEncoder(encoder);
  drmModeFreeConnector(connector);
  drmModeFreeResources(resources);
}

static struct gbm_device *gbm_device;
static EGLDisplay display;
static EGLContext context;
static struct gbm_surface *gbm_surface;
static EGLSurface egl_surface;

static uint32_t previous_fb;

static void swap_buffers(int render_fd) {

  struct gbm_device *gbm = gbm_create_device(render_fd);

  printf("fd ===%d,gbm =%p\n", render_fd, gbm);
  struct gbm_bo *bo = gbm_bo_create(gbm, mode_info.hdisplay, mode_info.vdisplay,
                                    GBM_BO_FORMAT_XRGB8888,
                                    GBM_BO_USE_SCANOUT | GBM_BO_USE_RENDERING);

#if 0
  uint32_t handle = gbm_bo_get_handle(bo).u32;
#else

  uint32_t handle;
  int prime_fd = gbm_bo_get_fd(bo);
  int ret = drmPrimeFDToHandle(device, prime_fd, &handle);
    printf("prime fd to handle==%d\n", ret);
#endif
  uint32_t pitch = gbm_bo_get_stride(bo);

  uint32_t stride;
  uint32_t flags = GBM_BO_TRANSFER_READ | GBM_BO_TRANSFER_WRITE;
  void *map_data = NULL;
  void *addr = gbm_bo_map(bo, 0, 0, gbm_bo_get_width(bo), gbm_bo_get_height(bo),
                          flags, &stride, &map_data);

  uint32_t *pixel = (uint32_t *)addr;
  for (uint32_t y = 0; y < mode_info.hdisplay; y++) {
    for (uint32_t x = 0; x < mode_info.vdisplay; x++) {
      pixel[y * (pitch / 8) + x] =
          0x555555; // compute_color(x, y, handle.width, handle.height);
    }
  }
  uint32_t fb;
  ret = drmModeAddFB(device, mode_info.hdisplay, mode_info.vdisplay, 24, 32,
                         pitch, handle, &fb);
  printf("add FB ret ===%d\n", ret);
  ret = drmModeSetCrtc(device, crtc->crtc_id, fb, 0, 0, &connector_id, 1,
                       &mode_info);
  printf("CRTC ret ===%d\n", ret);

  gbm_bo_unmap(bo, map_data);
}

static void draw(float progress) {}

int main() {
  device = open("/dev/dri/card0", O_RDWR | O_CLOEXEC);

  char *render = "/dev/dri/renderD128";

  int render_fd = open(render, O_RDWR | O_CLOEXEC);
  if (render_fd < 0) {
    exit(1);
  }
  find_display_configuration();

  int i;
  for (i = 0; i < 400; i++) {
    swap_buffers(render_fd);
  }

  close(device);
  return 0;
}
