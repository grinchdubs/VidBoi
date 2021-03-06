#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <assert.h>
#include <unistd.h>
#include <string>
#include <iostream>
#include <fstream>
#include <thread>

#include <linux/input.h>


//gl Includes
#include "bcm_host.h"
#include "GLES2/gl2.h"
#include "GLES2/gl2ext.h"
#include "EGL/egl.h"
#include "EGL/eglext.h"

//my includes
#include "src/util.cpp"
#include "lib/SOIL.h"
#include "src/input.h"
#include "src/audio.h"
#include "src/fileWatcher.h"

//Global defines
#include <time.h>
#define NUM_SCENES 1
#define IMAGE_SIZE 128

#define IN_TEX_NAME "/images/pusheen.png"
#define CONTEXT_DIV 1000

clock_t begin = clock();

bool programRunning = true;

Input* inputs;
Audio* audio;
FileWatcher* watcher;

bool loadNewScene = false;
float lastButtonState = 0;
float buttonState = 0;

typedef struct
{
  uint32_t screen_width;
  uint32_t screen_height;
// OpenGL|ES objects
  EGLDisplay display;
  EGLSurface surface;
  EGLContext context;

  GLuint verbose;
  GLint inputValY = 0;
  GLint inputValX = 0;
  GLint sceneIndex = 1;
  std::vector<GLushort> inputCVList;
  GLfloat inputCV0 = 0.0;
  GLfloat inputCV1 = 0.0;
  GLfloat inputCV2 = 0.;
  GLfloat inputCV6 = 0.0;
  GLfloat inputCV7 = 0.0;

  GLfloat inputFFT[4] = {0.0, 0.0, 0.0, 0.0};
  GLuint vshader;
  GLuint fshader;
  GLuint mshader;
  GLuint program;
  GLuint framebuffer;
  GLuint tex[4];
  GLuint buf;

  //create pixel buffer pointer (for passing color into audio engine)
  GLubyte* pixels;
  int pixelsSize;

  //audio to video vars
  std::vector<float> buffer1;
  std::vector<float> buffer2;



  //unsigned char* fb_buf; //feedback disabled
  unsigned char* inputImageTexBuf;
  int buf_height, buf_width;
// my shader attribs
  GLuint unif_color, attr_vertex, unif_scale, unif_offset, unif_tex, unif_centre, unif_resolution, unif_texCV, unif_texIN, unif_cv0, unif_cv1, unif_cv2, unif_cv6, unif_cv7, unif_fft;
  GLuint unif_inputVal, unif_sceneIndex;

  GLuint unif_time;
} CUBE_STATE_T;

static CUBE_STATE_T _state, *state = &_state;
int e;
#define check() e = glGetError(); if(e != 0) std::cout << "GL ERROR "<< e << " on line: "<< __LINE__ <<std::endl; assert(glGetError() == 0); 

static void checkFramebuffer() {
  GLenum ret = glCheckFramebufferStatus(GL_FRAMEBUFFER);
  if (ret != GL_FRAMEBUFFER_COMPLETE) {
    std::cout << " Framebuffer issue " << ret <<   std::endl;
    if ( ret == GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT) {
      std::cout << "GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT" << std::endl;
    }
  }
}

static void showlog(GLint shader)
{
  // Prints the compile log for a shader
  char log[1024];
  glGetShaderInfoLog(shader, sizeof log, NULL, log);
  printf("\n%d:shader:\n%s\n", shader, log);
}

static void showprogramlog(GLint shader)
{
  // Prints the information log for a program object
  char log[1024];
  glGetProgramInfoLog(shader, sizeof log, NULL, log);
  printf("\n%d:program:\n%s\n", shader, log);
}

static std::string getExecutablePath() {
  char buf[255] = "";
  readlink("/proc/self/exe", buf, sizeof(buf) );
  std::string path = std::string(buf);
  int position = path.rfind ('/');
  path = path.substr(0, position) + "/";
  return path;
}




/***********************************************************
 * Name: init_ogl
 *
 * Arguments:
 *       CUBE_STATE_T *state - holds OGLES model info
 *
 * Description: Sets the display, OpenGL|ES context and screen stuff
 *
 * Returns: void
 *
 ***********************************************************/
static void init_ogl(CUBE_STATE_T *state)
{
  int32_t success = 0;
  EGLBoolean result;
  EGLint num_config;

  static EGL_DISPMANX_WINDOW_T nativewindow;

  DISPMANX_ELEMENT_HANDLE_T dispman_element;
  DISPMANX_DISPLAY_HANDLE_T dispman_display;
  DISPMANX_UPDATE_HANDLE_T dispman_update;
  VC_RECT_T dst_rect;
  VC_RECT_T src_rect;

  static const EGLint attribute_list[] =
  {
    EGL_RED_SIZE, 8,
    EGL_GREEN_SIZE, 8,
    EGL_BLUE_SIZE, 8,
    EGL_ALPHA_SIZE, 8,
    EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
    EGL_NONE
  };

  static const EGLint context_attributes[] =
  {
    EGL_CONTEXT_CLIENT_VERSION, 2,
    EGL_NONE
  };
  EGLConfig config;

  // get an EGL display connection
  state->display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
  assert(state->display != EGL_NO_DISPLAY);
  check();

  // initialize the EGL display connection
  result = eglInitialize(state->display, NULL, NULL);
  assert(EGL_FALSE != result);
  check();

  // get an appropriate EGL frame buffer configuration
  result = eglChooseConfig(state->display, attribute_list, &config, 1, &num_config);
  assert(EGL_FALSE != result);
  check();

  // get an appropriate EGL frame buffer configuration
  result = eglBindAPI(EGL_OPENGL_ES_API);
  assert(EGL_FALSE != result);
  check();

  // create an EGL rendering context
  state->context = eglCreateContext(state->display, config, EGL_NO_CONTEXT, context_attributes);
  assert(state->context != EGL_NO_CONTEXT);
  check();

  // create an EGL window surface
  success = graphics_get_display_size(0 /* LCD */, &state->screen_width, &state->screen_height);
  assert( success >= 0 );

  dst_rect.x = 0;
  dst_rect.y = 0;
  dst_rect.width = state->screen_width ;
  dst_rect.height = state->screen_height;

  src_rect.x = 0;
  src_rect.y = 0;
  src_rect.width = state->screen_width << 16;
  src_rect.height = state->screen_height << 16;

  dispman_display = vc_dispmanx_display_open( 0 /* LCD */);
  dispman_update = vc_dispmanx_update_start( 0 );

  dispman_element = vc_dispmanx_element_add ( dispman_update, dispman_display,
                    0/*layer*/, &dst_rect, 0/*src*/,
                    &src_rect, DISPMANX_PROTECTION_NONE, 0 /*alpha*/, 0/*clamp*/,  (DISPMANX_TRANSFORM_T)0/*transform*/);

  nativewindow.element = dispman_element;
  nativewindow.width = state->screen_width;
  nativewindow.height = state->screen_height;
  vc_dispmanx_update_submit_sync( dispman_update );

  check();

  state->surface = eglCreateWindowSurface( state->display, config, &nativewindow, NULL );
  assert(state->surface != EGL_NO_SURFACE);
  check();

  // connect the context to the surface
  result = eglMakeCurrent(state->display, state->surface, state->surface, state->context);
  assert(EGL_FALSE != result);
  check();

  // Set background color and clear buffers
  glClearColor(0.15f, 0.25f, 0.35f, 1.0f);
  glClear( GL_COLOR_BUFFER_BIT );
  //disable depth Buffer
  //glDepthMask(false);

  //set 
  eglSwapInterval( state->display, 2.1);

  check();

  state->pixelsSize = state->screen_width * state->screen_height * 3;
  state->pixels = new GLubyte[state->pixelsSize];

  std::cout<< "GL VERSION: " << glGetString(GL_SHADING_LANGUAGE_VERSION) << std::endl;
}

std::string readFile(const char *filePath) {

  std::string line = "";
  std::ifstream in(filePath);
  if (!in.is_open()) {
    std::cerr << "Could not read file " << filePath << ". File does not exist." << std::endl;
    return "";
  }
  std::string content((std::istreambuf_iterator<char>(in)),
                      std::istreambuf_iterator<char>());

  in.close();
  return content;
}


static void load_tex_images(CUBE_STATE_T *state)
{
  SOIL_free_image_data(state->inputImageTexBuf);
  //SOIL LOADER
  state->inputImageTexBuf = SOIL_load_image(( getExecutablePath() + std::string(IN_TEX_NAME)).c_str(), &state->buf_width, &state->buf_height, 0, SOIL_LOAD_RGB);

  if ( 0 == state->inputImageTexBuf)
  {
    printf( "SOIL loading error: '%s'\n", SOIL_last_result() );
  }

}

static void init_shaders( bool firstrun = true)
{
  // Points for a screen size poly to draw the shader on
  static const GLfloat vertex_data[] = {
    -1.0, -1.0, 1.0, 1.0,
    1.0, -1.0, 1.0, 1.0,
    1.0, 1.0, 1.0, 1.0,
    -1.0, 1.0, 1.0, 1.0
  };
  //initialize cv input texture
  //state->inputCVList = std::vector<GLushort>(CV_LIST_SIZE + sqrt(CV_LIST_SIZE) + 1 , 0);
  state->inputCVList = std::vector<GLushort>(RECORD_BUFFER + sqrt(RECORD_BUFFER) + 1 , 0);

  //TODO: Automatically read files in Shaders/ directory
  //load up the shader files
  std::string vertShaderStr = readFile((getExecutablePath() + "vshader.vert").c_str());
  std::string fragShaderStr = readFile((getExecutablePath() + "/Shaders/myShader" + std::to_string(state->sceneIndex) + ".frag").c_str());
  const char *vertShaderSrc = vertShaderStr.c_str();
  const char *fragShaderSrc = fragShaderStr.c_str();

  //basic vert shader
  const GLchar *vshader_source = vertShaderSrc;
  //my custom fragment shader
  const GLchar *fshader_source = fragShaderSrc;

  //set verbose to always be true ( print out shader errors)
  state->verbose = true;

  state->vshader = glCreateShader(GL_VERTEX_SHADER);
  glShaderSource(state->vshader, 1, &vshader_source, 0);
  glCompileShader(state->vshader);
  check();

  if (state->verbose)
    showlog(state->vshader);

  state->fshader = glCreateShader(GL_FRAGMENT_SHADER);
  glShaderSource(state->fshader, 1, &fshader_source, 0);
  glCompileShader(state->fshader);
  check();

  if (state->verbose)
    showlog(state->fshader);


  // custom shader attach
  state->program = glCreateProgram();
  glAttachShader(state->program, state->vshader);
  glAttachShader(state->program, state->fshader);
  glLinkProgram(state->program);
  glDetachShader(state->program, state->vshader);
  glDetachShader(state->program, state->fshader);
  glDeleteShader(state->vshader);
  glDeleteShader(state->fshader);

  check();

  if (state->verbose)
    showprogramlog(state->program);

  //set up input uniforms

  state->attr_vertex = glGetAttribLocation(state->program, "vertex");
  state->unif_color  = glGetUniformLocation(state->program, "color");
  state->unif_scale  = glGetUniformLocation(state->program, "scale");
  state->unif_offset = glGetUniformLocation(state->program, "offset");
  state->unif_tex    = glGetUniformLocation(state->program, "texFB");
  state->unif_texCV    = glGetUniformLocation(state->program, "texCV");
  state->unif_texIN    = glGetUniformLocation(state->program, "texIN");
  state->unif_resolution   = glGetUniformLocation(state->program, "resolution");
  state->unif_centre = glGetUniformLocation(state->program, "centre");
  state->unif_time = glGetUniformLocation(state->program, "time");
  state->unif_inputVal = glGetUniformLocation(state->program, "inputVal");
  state->unif_sceneIndex = glGetUniformLocation(state->program, "sceneIndex");
  state->unif_cv0 = glGetUniformLocation(state->program, "cv0");
  state->unif_cv1 = glGetUniformLocation(state->program, "cv1");
  state->unif_cv2 = glGetUniformLocation(state->program, "cv2");
  state->unif_cv6 = glGetUniformLocation(state->program, "cv6");
  state->unif_cv7 = glGetUniformLocation(state->program, "cv7");
  state->unif_fft = glGetUniformLocation(state->program, "fft");



  glClearColor ( 0.0, 1.0, 1.0, 1.0 );
  if (!firstrun) {
    glDeleteBuffers(1, &state->buf);
  }
  glGenBuffers(1, &state->buf);

  check();

  // Prepare a texture image
  if (!firstrun) {
    glDeleteTextures(3,  &state->tex[0]);
  }
  glActiveTexture(GL_TEXTURE0);
  glGenTextures(3, &state->tex[0]);
  check();
  //setup drawing texture ( bound to main fb later)
  glBindTexture(GL_TEXTURE_2D, state->tex[0]);
  check();

  //usually has "/ CONTEXT_DIV " this isnt working in some cases for some reason
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, state->screen_width   , state->screen_height  , 0, GL_RGB, GL_UNSIGNED_SHORT_5_6_5, 0);
  check();
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  check();

  // setup tex index 1 (used for CV input)
  glActiveTexture(GL_TEXTURE0 + 1);
  glBindTexture(GL_TEXTURE_2D, state->tex[1]);

  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 3 , 2, 0,
               GL_RGB, GL_UNSIGNED_SHORT_5_6_5, &state->inputCVList[0]);
  check();
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, (GLfloat)GL_LINEAR);
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, (GLfloat)GL_LINEAR);
  check();

  //bind input tex
  load_tex_images(state);
  glActiveTexture(GL_TEXTURE0 + 2);
  glBindTexture(GL_TEXTURE_2D, state->tex[2]);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, state->buf_width, state->buf_height, 0,
               GL_RGB, GL_UNSIGNED_BYTE, state->inputImageTexBuf);
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, (GLfloat)GL_LINEAR);
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, (GLfloat)GL_LINEAR);
  check();


  // Prepare a framebuffer for rendering
  if (!firstrun) {
    glDeleteFramebuffers(1, &state->framebuffer);
  }

  glGenFramebuffers(1, &state->framebuffer);
  check();

  glBindFramebuffer(GL_FRAMEBUFFER, state->framebuffer);
  check();

  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, state->tex[0], 0);
  check();
  
  checkFramebuffer();
  // glBindFramebuffer(GL_FRAMEBUFFER,0);
  // check();
  // Prepare viewport
  glViewport ( 0, 0, state->screen_width, state->screen_height );
  check();

  // Upload vertex data to a buffer
  glBindBuffer(GL_ARRAY_BUFFER, state->buf);
  glBufferData(GL_ARRAY_BUFFER, sizeof(vertex_data),
               vertex_data, GL_STATIC_DRAW);
  glVertexAttribPointer(state->attr_vertex, 4, GL_FLOAT, 0, 16, 0);
  glEnableVertexAttribArray(state->attr_vertex);
  check();

}

static void restart_shaders(){
  loadNewScene = true;
}


static void draw_triangles(CUBE_STATE_T *state, GLfloat cx, GLfloat cy, GLfloat scale)
{
  //render to a texture
  glBindFramebuffer(GL_FRAMEBUFFER, state->framebuffer);
  //glBindFramebuffer(GL_FRAMEBUFFER,state->tex_fb); //ping pong here for framebuffer
  // Clear the background (not really necessary I suppose)
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  //
  checkFramebuffer();
  check();

  glBindBuffer(GL_ARRAY_BUFFER, state->buf);
  check();
  glUseProgram ( state->program );
  check();
  glUniform4f(state->unif_color, 0.5, 0.5, 0.8, 1.0);
  glUniform2f(state->unif_scale, scale, scale);
  glUniform2f(state->unif_centre, cx, cy);
  glUniform2f( state->unif_resolution, state->screen_width, state->screen_height);
  glUniform2f(state->unif_inputVal, state->inputValX, state->inputValY);
  //pass sceneIndex into shader
  glUniform1i( state->unif_sceneIndex, state->sceneIndex);

  //UPDATE UNIFORM VALUES

  //pass CVs into shader
  state->inputCV0 = abs( inputs->getCV(0) ); //inputs->getCV(0) +
  state->inputCV1 = abs(  inputs->getCV(1) ); //inputs->getCV(1) +
  state->inputCV2 = abs(  inputs->getCV(2) ); // inputs->getCV(2) +

  //sliders
  state->inputCV6 = abs(  inputs->getCV(6) );
  state->inputCV7 = abs(  inputs->getCV(7) );

  //OLD VER ( CV LIST ONLY )
  //get sqrt of the list size to find out a dimension of the square texture
  // double texDim = sqrt(CV_LIST_SIZE);
  //copy CV list into uniform array
  // std::vector<float> cvlist[3] = {inputs->getCVList(3), inputs->getCVList(4), inputs->getCVList(5)} ;
  // int offset = 0;
  // for (int i = 0; i < CV_LIST_SIZE; ++i) {
  //   if (i != 0 && (i % (int)texDim) == texDim) { // zero value at start of every line (GL formating for textures is like this, not sure why)
  //     state->inputCVList[i] = 0;
  //     offset -= 1;
  //   }
  //   else {
  //     state->inputCVList[i] = ushortColor( cvlist[1][i + offset], cvlist[1][i + offset], cvlist[2][i + offset]);
  //   }
  // }

  //INTERNAL OSCS VER
  //get audio buffer and feed that into an input
  if( audio->hasNewBuffer()){
    state->buffer1 = audio->getBuffer(0);
    state->buffer2 = audio->getBuffer(1);
  }
  double texDim = sqrt(state->buffer1.size());

  std::vector<float>::iterator it1 = state->buffer1.begin();
  std::vector<float>::iterator it2 = state->buffer2.begin();

  int offset = 0;
  for (int i = 0; i < state->buffer1.size(); ++i) {
    if (i != 0 && (i % (int)texDim) == texDim) { // zero value at start of every line (GL formating for textures is like this, not sure why)
      state->inputCVList[i] = 0;
      offset -= 1;
    }
    else {
      
      float audioVal1 = ((*it1 + 1.) / 2.);
      float audioVal2 = ((*it2 + 1.) / 2.);
      //std::cout<< audioVal << std::endl; 
      if(audioVal1 < 0 || audioVal1 > 1. ){
        std::cout<< "         WHOA:" << audioVal1 <<std::endl;
      }
      else if(audioVal2 < 0 || audioVal2 > 1. ){
        std::cout<< "         WHOA:" << audioVal2 <<std::endl;
      }
      else{
        state->inputCVList[i] = ushortColor( audioVal1 , audioVal2 , 0);
      }
      ++it1;
      ++it2;
    }
  }
  //std::cout<< "reading buffer" <<std::endl;

  glBindTexture(GL_TEXTURE_2D, state->tex[1]);

  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, texDim , texDim, 0,
               GL_RGB, GL_UNSIGNED_SHORT_5_6_5, &state->inputCVList[0]);

  check();
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, (GLfloat)GL_LINEAR);
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, (GLfloat)GL_LINEAR);
  check();

  glUniform1f(state->unif_cv0, state->inputCV0);
  glUniform1f(state->unif_cv1, state->inputCV1);
  glUniform1f(state->unif_cv2, state->inputCV2);
  glUniform1f(state->unif_cv6, state->inputCV6);
  glUniform1f(state->unif_cv7, state->inputCV7);
  glUniform1i(state->unif_tex, 0); // I don't really understand this part, perhaps it relates to active texture?
  glUniform1i(state->unif_texCV, 1);
  glUniform1i(state->unif_texIN, 2);

  //pass time into the frag shader
  clock_t end = clock();
  double elapsed_secs = double(end - begin) / (CLOCKS_PER_SEC / 100);
  glUniform1f(state->unif_time, elapsed_secs);


  check();

  glDrawArrays ( GL_TRIANGLE_FAN, 0, 4 );
  check();
  checkFramebuffer();
  glBindBuffer(GL_ARRAY_BUFFER, 0);

  glFlush();
  glFinish();
  check();

  // Now render that texture to the main frame buffer
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  check();

  glBindBuffer(GL_ARRAY_BUFFER, state->buf);
  check();


  glDrawArrays ( GL_TRIANGLE_FAN, 0, 4 );
  check();


  glBindBuffer(GL_ARRAY_BUFFER, 0);


  glFlush();
  glFinish();
  check();

   //glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, state->screen_width   , state->screen_height  , 0, GL_RGB, GL_UNSIGNED_SHORT_5_6_5, 0);
  
  glReadPixels(0,0, state->screen_width, state->screen_height, GL_RGB, GL_UNSIGNED_BYTE, state->pixels);

  // for( int x = 0 ; x < 6; ++x){
  //   std::cout << (int)state->pixels[x] << " " ;
  // }
  // std::cout <<std::endl;

  check();


  eglSwapBuffers(state->display, state->surface);
  check();


  //load color into audio
  audio->loadByteWaveTable( state->pixels, state->pixelsSize );


}

static int get_mouse(CUBE_STATE_T *state, int *outx, int *outy)
{
  static int fd = -1;
  const int width = state->screen_width, height = state->screen_height;
  static int x = 800, y = 400;
  const int XSIGN = 1 << 4, YSIGN = 1 << 5;
  if (fd < 0) {
    fd = open("/dev/input/mouse0", O_RDONLY | O_NONBLOCK);
  }
  if (fd >= 0) {
    struct {char buttons, dx, dy; } m;
    while (1) {
      int bytes = read(fd, &m, sizeof m);
      if (bytes < (int)sizeof m) goto _exit;
      if (m.buttons & 8) {
        break; // This bit should always be set
      }
      read(fd, &m, 1); // Try to sync up again
    }
    if (m.buttons & 3)
      return m.buttons & 3;
    x += m.dx;
    y += m.dy;
    if (m.buttons & XSIGN)
      x -= 256;
    if (m.buttons & YSIGN)
      y -= 256;
    if (x < 0) x = 0;
    if (y < 0) y = 0;
    if (x > width) x = width;
    if (y > height) y = height;
  }
_exit:
  if (outx) *outx = x;
  if (outy) *outy = y;
  return 0;
}


static const char *const evval[3] = {
  "RELEASED",
  "PRESSED ",
  "REPEATED"
};

void onButton(bool button) {
  if (state->sceneIndex + button >= NUM_SCENES) {
    state->sceneIndex = 0;
  }
  else {
    state->sceneIndex =  state->sceneIndex + button;
  }

  std::cout << "switching Shaders" << std::endl;
  if (button != lastButtonState) {
    loadNewScene = true;
  }
  lastButtonState = buttonState;
}



//Keyboard input

int keyboardFd = -1;

bool setupKeyboard() {
  const char *dev = "/dev/input/by-id/usb-_USB_Keyboard-event-kbd";

  keyboardFd = open(dev, O_RDONLY | O_NONBLOCK);
  if (keyboardFd == -1) {
    fprintf(stderr, "Cannot open %s: %s.\n", dev, strerror(errno));
    return false;
  }
  return true;
}

bool readKeyboard() {
  struct input_event ev;
  ssize_t n;
  // if(keyboardFd == -1){
  //  setupKeyboard();
  // }

  n = read(keyboardFd, &ev, sizeof ev);
  if (n == (ssize_t) - 1) {
    if (errno == EINTR)
      return true;
    else {
      //just continue here
      return true;
    }
  } else if (n != sizeof ev) {
    errno = EIO;

    return false;
  }
  if (ev.type == EV_KEY && ev.value >= 0 && ev.value <= 2)
    printf("%s 0x%04x (%d)\n", evval[ev.value], (int)ev.code, (int)ev.code);


  if (ev.code == KEY_LEFT) {
    state->inputValX -= 1;
  }
  if (ev.code == KEY_RIGHT) {
    state->inputValX += 1;
  }
  if (ev.code == KEY_DOWN) {
    state->inputValY -= 1;
  }
  if (ev.code == KEY_UP) {
    state->inputValY += 1;
  }


  if (ev.code == KEY_ESC) {
    return false;
  }
  return true;
}

void destroyShader() {
  glDeleteProgram(state->program);
  eglMakeCurrent(state->display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
  eglDestroySurface( state->display, state->surface);
  eglDestroyContext(state->display, state->context);
}


//==============================================================================
#define SHOW_FRAMETIME false

int main ()
{

  int terminate = 0;
  GLfloat cx, cy;
  bcm_host_init();

  // Clear application state
  memset( state, 0, sizeof( *state ) );

  //Start Inputs
  inputs = new Input();

  //Start Audio Engine
  audio = new Audio(inputs);
  std::cout << "Audio Started" << std::endl;

  watcher = new FileWatcher(getExecutablePath() + "Shaders", &restart_shaders);
  

  // Start OGLES
  init_ogl(state);
  init_shaders();
  cx = state->screen_width / 2;
  cy = state->screen_height / 2;

  setupKeyboard();
  inputs->addButtonCallback(&onButton);
  while (!terminate)
  {
    clock_t frameBegin = clock();

    if (!readKeyboard()) {
      destroyShader();
      terminate = true;
    }
    else if (loadNewScene  == true) {
      destroyShader();
      init_ogl(state);
      init_shaders(false);
      loadNewScene = false;
    }
    else {
      draw_triangles(state, cx, cy, 0.003);
    }

    if (SHOW_FRAMETIME) {
      clock_t frameEnd = clock();
      double elapsed_secs = double(frameEnd - frameBegin) / CLOCKS_PER_SEC;
      std::cout << "Frame Time: " << elapsed_secs << std::endl;
    }

    //if(elapsed_secs < .03 ){ sleep(0.03 - elapsed_secs); }

  }


  fflush(stdout);
  fprintf(stderr, "%s.\n", strerror(errno));

  delete(inputs);
  // delete(audio);
  delete(state->pixels);


  return 0;
}

