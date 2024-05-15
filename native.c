#include <GLFW/glfw3.h>
#include <lean/lean.h>
#include <stdbool.h>
#include <stdio.h>


static void noop_foreach(void *data, b_lean_obj_arg arg) {
	// NOTHING
}

static void glfw_window_finalizer(void *ptr) {
	glfwDestroyWindow((GLFWwindow *) ptr);
}

static lean_external_class *get_glfw_window_class() {
	static lean_external_class *g_glfw_window_class = NULL;
	if (!g_glfw_window_class) {
		g_glfw_window_class = lean_register_external_class(
				&glfw_window_finalizer,
				&noop_foreach);
	}
	return g_glfw_window_class;
}

// lean_obj_res lean_glfwInit(lean_obj_arg world) {
lean_obj_res lean_glfwInit(lean_obj_arg world) {
	int result = glfwInit();
	return lean_io_result_mk_ok(lean_box(result));
}

lean_obj_res lean_glfwTerminate(lean_obj_arg world) {
	glfwTerminate();
	return lean_io_result_mk_ok(lean_box(0));
}

lean_obj_res lean_glfwCreateWindow(uint32_t width, uint32_t height, b_lean_obj_arg title, lean_obj_arg world) {
	printf("lean_glfwCreateWindow %dx%d\n", width, height);
	char const *title_cstr = lean_string_cstr(title);
	GLFWwindow *win = glfwCreateWindow(width, height, title_cstr, NULL, NULL);
	return lean_io_result_mk_ok(lean_alloc_external(get_glfw_window_class(), win));
}

lean_obj_res lean_glfwWindowShouldClose(lean_obj_arg winp, lean_obj_arg world) {
	assert(lean_is_external(winp));
	GLFWwindow * win = (GLFWwindow *) lean_get_external_data(winp);
	bool status = glfwWindowShouldClose(win);
	return lean_io_result_mk_ok(lean_box(status));
}

lean_obj_res lean_glfwPollEvents(lean_obj_arg world) {
	glfwPollEvents();
	return lean_io_result_mk_ok(lean_box(0));
}
