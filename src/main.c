#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <stdio.h>

#include "GLFW/glfw3.h"

#define VM_IMPL
#define VM_STATIC
#include "vmlib/vmlib.h"

#include "opengl.h"

static GLFWwindow *window;
static int context_version;
static GLuint tshader, wshader;
static GLuint tvao, tvbo;
static mat4 proj, view;
static int detail = 32;

static double cam_radius = 10.0;
static double cam_yaw, cam_pitch;
static double last_x, last_y;
static int dragging, ignore_mouse;
static double sensitivity = 0.005;
vec3 cam_pos;

static const vec3 cam_target = {{ 0.0f, 0.0f, 0.0f }};
static const vec3 cam_up = {{ 0.0f, 1.0f, 0.0f }};

#define DEFAULT_WIDTH 1280
#define DEFAULT_HEIGHT 720

static char *tshader_vert = NULL;
static char *tshader_frag = NULL;

static char *wshader_vert = "#version 330\n"
"layout (location=0) in vec3 pos;"
"uniform mat4 proj, view, model;"
"void main(){gl_Position = proj * view * model * vec4(pos, 1.0);}";
static char *wshader_frag = "#version 330\n"
"out vec4 frag;"
"void main(){frag=vec4(1.0);}";

static void glfw_err_cb(int id, const char *message)
{
	fprintf(stderr, "GLFW: %s (%d)\n", message, id);
}

static void APICALL opengl_err_cb(
	GLenum source, GLenum type, GLuint id,
	GLenum severity, GLsizei length,
	const GLchar *message, const void *user
) {
	(void)source; (void)type; (void)severity; (void)length; (void)user;
	if (id == 131185) return;
	fprintf(stderr, "OPENGL: %s (%u)\n", message, id);
}

static void cleanup(void)
{
	const GLuint vaos[] = { tvao };
	const GLuint bos[] = { tvbo };

	glDeleteProgram(tshader);
	glDeleteProgram(wshader);
	glDeleteVertexArrays(1, vaos);
	glDeleteBuffers(1, bos);

	free(tshader_vert);
	free(tshader_frag);

	glfwTerminate();
}

static void get_view(void)
{
	cam_pos.x = cam_target.x + (float)(cam_radius * cos(cam_pitch) * sin(cam_yaw));
	cam_pos.y = cam_target.y + (float)(cam_radius * sin(cam_pitch));
	cam_pos.z = cam_target.z + (float)(cam_radius * cos(cam_pitch) * cos(cam_yaw));
	mat4lookat(&view, &cam_pos, &cam_target, &cam_up);
}

static void update_uniforms(void)
{
	glUseProgram(tshader);
	get_view();
	glUniformMatrix4fv(glGetUniformLocation(tshader, "proj"), 1, GL_FALSE, proj.d);
	glUniform3f(glGetUniformLocation(tshader, "campos"), cam_pos.x, cam_pos.y, cam_pos.z);
	glUniformMatrix4fv(glGetUniformLocation(tshader, "view"), 1, GL_FALSE, view.d);

	glUseProgram(wshader);
	glUniformMatrix4fv(glGetUniformLocation(wshader, "proj"), 1, GL_FALSE, proj.d);
	glUniformMatrix4fv(glGetUniformLocation(wshader, "view"), 1, GL_FALSE, view.d);
}

static void update_framebuffer(GLFWwindow *window, int width, int height)
{
	(void)(window);
	mat4perspective(&proj, VRADIANS(90.0f), (float)width / (float)height, 0.01f, 1000.0f);
	glViewport(0, 0, width, height);
	update_uniforms();
}

static void mouse_enter(GLFWwindow *window, int entered)
{
	ignore_mouse = 1;
}

static void mouse_down(GLFWwindow *window, int button, int action, int mods)
{
	(void)(mods);
	if (button != GLFW_MOUSE_BUTTON_RIGHT) return;
	dragging = action == GLFW_PRESS;
	if (!dragging) return;
	glfwGetCursorPos(window, &last_x, &last_y);
	update_uniforms();
}

static void mouse_scroll(GLFWwindow *window, double dx, double dy)
{
	cam_radius -= dy * 0.2;
	if (cam_radius < 0.2) cam_radius = 0.2;
	update_uniforms();
}

static void mouse_move(GLFWwindow *window, double px, double py)
{
	const double gim_lock = VRADIANS(89.99);
	double dx, dy;

	(void)(window);
	if (!dragging) return;
	if (ignore_mouse) {
		ignore_mouse = 0;
		return;
	}

	dx = px - last_x;
	dy = py - last_y;
	
	cam_yaw += dx * sensitivity;
	cam_pitch -= dy * sensitivity;

	if (cam_pitch > gim_lock) cam_pitch = gim_lock;
	if (cam_pitch < -gim_lock) cam_pitch = -gim_lock;
	
	last_x = px;
	last_y = py;
	update_uniforms();
}

static int get_file(const char *relative, const char *backup, char **res)
{
	long len;
	FILE *f = fopen(relative, "rb");
	if (!f && !(f = fopen(backup, "rb"))) return 0;
	fseek(f, 0, SEEK_END);
	len = ftell(f);
	fseek(f, 0, SEEK_SET);
	*res = calloc(1, (size_t)len + 1);
	if (*res) fread(*res, 1, (size_t)len, f);
	fclose(f);
	return *res != NULL;

}

static void setup_context(void)
{
	size_t i;
	const unsigned char versions[] = {
		OPENGL_MAKE_VERSION(4, 6), OPENGL_MAKE_VERSION(4, 5), OPENGL_MAKE_VERSION(4, 4),
		OPENGL_MAKE_VERSION(4, 3), OPENGL_MAKE_VERSION(4, 2), OPENGL_MAKE_VERSION(4, 1), OPENGL_MAKE_VERSION(4, 0),
		OPENGL_MAKE_VERSION(3, 3), OPENGL_MAKE_VERSION(3, 2), OPENGL_MAKE_VERSION(3, 1), OPENGL_MAKE_VERSION(3, 0)
	};

	glfwSetErrorCallback(glfw_err_cb);
	if (!glfwInit()) goto fail;

#ifndef NDEBUG
	glfwWindowHint(GLFW_CONTEXT_DEBUG, GLFW_TRUE);
#endif
#ifdef __APPLE__
	glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
#endif

	for (i = 0; i < sizeof versions; ++i) {
		glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, OPENGL_VERSION_MAJOR(versions[i]));
		glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, OPENGL_VERSION_MINOR(versions[i]));
		window = glfwCreateWindow(DEFAULT_WIDTH, DEFAULT_HEIGHT, "Raymarch noise test", NULL, NULL);
		if (window) break; 
	}

	if (!window) goto fail;
	glfwMakeContextCurrent(window);
	if (!(context_version = opengl_init(opengl_err_cb))) goto fail;

	if (!get_file("march.vs", "src/march.vs", &tshader_vert)) goto fail;
	if (!get_file("march.fs", "src/march.fs", &tshader_frag)) goto fail;

	glfwSetWindowSizeLimits(window, 640, 360, GLFW_DONT_CARE, GLFW_DONT_CARE);
	glfwSetFramebufferSizeCallback(window, update_framebuffer);
	glfwSwapInterval(1);

	glfwSetMouseButtonCallback(window, mouse_down);
	glfwSetScrollCallback(window, mouse_scroll);
	glfwSetCursorPosCallback(window, mouse_move);
	glfwSetCursorEnterCallback(window, mouse_enter);

	return;
fail:
	fprintf(stderr, "Failed to initialize context.\n");
	cleanup();
	exit(EXIT_FAILURE);
}

static void verify_shader(GLuint shader, const char *type)
{
	GLint success;
	char buf[512];
	
	glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
	if (success) return;

	glGetShaderInfoLog(shader, sizeof buf, NULL, buf);
	fprintf(stderr, "%s SHADER ERROR: %s", type, buf);

	cleanup();
	exit(EXIT_FAILURE);
}

static void create_shader(GLuint *program, const char *vert_src, const char *frag_src)
{
	GLuint vert = glCreateShader(GL_VERTEX_SHADER);
	GLuint frag = glCreateShader(GL_FRAGMENT_SHADER);
	GLint success;
	char buf[512];

	glShaderSource(vert, 1, &vert_src, NULL);
	glCompileShader(vert);
	verify_shader(vert, "VERTEX");

	glShaderSource(frag, 1, &frag_src, NULL);
	glCompileShader(frag);
	verify_shader(frag, "FRAGMENT");

	*program = glCreateProgram();
	glAttachShader(*program, vert);
	glAttachShader(*program, frag);
	glLinkProgram(*program);

	glGetProgramiv(*program, GL_LINK_STATUS, &success);
	if (!success) {
		glGetProgramInfoLog(*program, sizeof buf, NULL, buf);
		fprintf(stderr, "PROGRAM ERROR: %s", buf);
		cleanup();
		exit(EXIT_FAILURE);
	}

	glDetachShader(*program, vert);
	glDeleteShader(vert);
	glDetachShader(*program, frag);
	glDeleteShader(frag);
}

static void setup_main(void)
{
	static const float cube_verts[] = {
		-0.5f, -0.5f, -0.5f,
		 0.5f, -0.5f, -0.5f,
		 0.5f,  0.5f, -0.5f,
		 0.5f,  0.5f, -0.5f,
		-0.5f,  0.5f, -0.5f,
		-0.5f, -0.5f, -0.5f,

		-0.5f, -0.5f,  0.5f,
		 0.5f, -0.5f,  0.5f,
		 0.5f,  0.5f,  0.5f,
		 0.5f,  0.5f,  0.5f,
		-0.5f,  0.5f,  0.5f,
		-0.5f, -0.5f,  0.5f,

		-0.5f,  0.5f,  0.5f,
		-0.5f,  0.5f, -0.5f,
		-0.5f, -0.5f, -0.5f,
		-0.5f, -0.5f, -0.5f,
		-0.5f, -0.5f,  0.5f,
		-0.5f,  0.5f,  0.5f,

		 0.5f,  0.5f,  0.5f,
		 0.5f,  0.5f, -0.5f,
		 0.5f, -0.5f, -0.5f,
		 0.5f, -0.5f, -0.5f,
		 0.5f, -0.5f,  0.5f,
		 0.5f,  0.5f,  0.5f,

		-0.5f, -0.5f, -0.5f,
		 0.5f, -0.5f, -0.5f,
		 0.5f, -0.5f,  0.5f,
		 0.5f, -0.5f,  0.5f,
		-0.5f, -0.5f,  0.5f,
		-0.5f, -0.5f, -0.5f,

		-0.5f,  0.5f, -0.5f,
		 0.5f,  0.5f, -0.5f,
		 0.5f,  0.5f,  0.5f,
		 0.5f,  0.5f,  0.5f,
		-0.5f,  0.5f,  0.5f,
		-0.5f,  0.5f, -0.5f
	};

	glEnable(GL_DEPTH_TEST);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	glGenVertexArrays(1, &tvao);
	glGenBuffers(1, &tvbo);
	glBindVertexArray(tvao);
	glBindBuffer(GL_ARRAY_BUFFER, tvbo);
	glBufferData(GL_ARRAY_BUFFER, sizeof cube_verts, cube_verts, GL_STATIC_DRAW);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, NULL);

	create_shader(&tshader, tshader_vert, tshader_frag);
	create_shader(&wshader, wshader_vert, wshader_frag);

	return;
fail:
	cleanup();
	exit(EXIT_FAILURE);
}

static void main_loop(void)
{
	double current = 0.0, prev = current - 0.016;
	double printlast = 0.0;
	double scaled_time = 0.0;

	vec3 campos = VEC3(1.0f, 1.0f, -2.0f);
	vec3 targetpos = VEC3I;
	vec3 up = VEC3(0.0f, 1.0f, 0.0f);

	vec3 base_scale = VEC3I;
	vec3 final_scale = VEC3(10.0f, 2.0f, 10.0f);
	vec3 current_scale;

	mat4 scale;
	mat4sets(&scale, 1.0f);

	update_framebuffer(window, DEFAULT_WIDTH, DEFAULT_HEIGHT);
	update_uniforms();

	glBindVertexArray(tvao);

	while (!glfwWindowShouldClose(window)) {
		double dt = current - prev;
		prev = current;
		current = glfwGetTime();
		scaled_time = current * 0.2;

		glfwPollEvents();
		if (glfwGetKey(window, GLFW_KEY_ESCAPE)) glfwSetWindowShouldClose(window, GLFW_TRUE);

		mat4sets(&scale, 1.0f);
		vec3mix(&current_scale, &base_scale, &final_scale, (sin(scaled_time) * 0.5) + 0.5);
		mat4scale(&scale, &scale, &current_scale);

		glClearColor(0.2f, 0.2f, 0.2f, 1.0f);
		glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);

		glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
		glUseProgram(tshader);
		glUniform1f(glGetUniformLocation(tshader, "time"), scaled_time);
		glUniformMatrix4fv(glGetUniformLocation(tshader, "model"), 1, GL_FALSE, scale.d);
		glDrawArrays(GL_TRIANGLES, 0, 36);

		mat4sets(&scale, 1.0f);
		vec3mix(&current_scale, &base_scale, &final_scale, (sin(scaled_time) * 0.5) + 0.51);
		mat4scale(&scale, &scale, &current_scale);

		glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
		glUseProgram(wshader);
		glUniformMatrix4fv(glGetUniformLocation(wshader, "model"), 1, GL_FALSE, scale.d);
		glBindVertexArray(tvao);
		glDrawArrays(GL_TRIANGLES, 0, 36);

		glfwSwapBuffers(window);

		if (printlast >= 1.0) {
			printf("\rFPS: %f", 1.0 / dt);
			fflush(stdout);
			printlast = 0.0;
		} else printlast += dt;
	}

	printf("\r");
}

int main(int argc, char **argv)
{
	setup_context();
	setup_main();
	main_loop();
	cleanup();
}
